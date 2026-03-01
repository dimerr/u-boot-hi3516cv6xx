/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2020-2023. All rights reserved.
 */

#include <stdio.h>
#include <cpu_common.h>
#include <cpu_func.h>
#include <command.h>
#include <image.h>
#include <asm/u-boot-arm.h>
#include <asm/setup.h>
#include <dm/of.h>
#include <securec.h>
#include <linux/arm-smccc.h>
#include <linux/kernel.h>
#include "ipc.h"
#if CONFIG_IS_ENABLED(CMD_TIMESTAMP)
#include "cmd_timestamp.h"
#endif

#define	ipcboot_info(fmt, args...)		printf("Info:[%s, %d] "fmt, __func__, __LINE__, ##args)
#define	ipcboot_warn(fmt, args...)		printf("Warn:[%s, %d] "fmt, __func__, __LINE__, ##args)
#define	ipcboot_err(fmt, args...)		printf("Error:[%s, %d] "fmt, __func__, __LINE__, ##args)

#define __2MB		0x200000UL
#define ADDR_OFFSET     32

static ulong get_fdt_load_addr(void)
{
	return CONFIG_FDT_LOAD_ADDR;
}

static ulong get_kernel_load_addr(void)
{
	void *fdt = (void *)get_fdt_load_addr();
	int offset;
	ulong kernel_mem_base;

	offset = fdt_subnode_offset(fdt, 0, "memory");
	if (offset < 0) {
		ipcboot_err("fdt: /memory node is not found!\n");
		return OF_BAD_ADDR;
	}

	kernel_mem_base = fdt_get_base_address(fdt, offset);
	if (kernel_mem_base == OF_BAD_ADDR) {
		ipcboot_err("fdt: failed to get kernel memory base!\n");
		return OF_BAD_ADDR;
	}
	return round_up(kernel_mem_base, __2MB);
}

static void gsl_core_pwrdwn(void)
{
#if CONFIG_ARM_SMCCC
	struct arm_smccc_res res;
	unsigned long funcid = TEGRA_SMC_GSL_FUNCID;

	printf("Run the smc command to switch to the GSL.\n");
	arm_smccc_smc(funcid, 0, 0, 0, 0, 0, 0, 0, &res);
#else
	printf("Error, Please Enable the SMC service.\n");
#endif
	return;
}

static int wait_boot_msg_ack(void)
{
	struct ipc_share_msg msg;

	while (1) {
		int ret = ipc_recv_msg(IPC_NODE_CORE1, &msg, 1);
		if (ret < 0) {
			ipcboot_err("ipc_recv_msg() failed!\n");
			return ret;
		}

		if (msg.cmd == IPC_CMD_ACK)
			return 0;

		if (msg.cmd == IPC_CMD_NEED_BL31) {
			ipcboot_err("need bl31!\n");
			break;
		} else if (msg.cmd == IPC_CMD_NEED_TEEIMG) {
			ipcboot_err("need teeimg!\n");
			break;
		} else if (msg.cmd == IPC_CMD_NEED_KERNEL) {
			ipcboot_err("need kernel!\n");
			break;
		} else {
			ipcboot_warn("received an unexpected message.\n");
		}
	}

	return -1;
}

static int send_boot_msg(const struct ipc_share_msg *boot_msg)
{
	int ret;

	if (boot_msg == NULL) {
		ipcboot_err("boot_msg is NULL\n");
		return -1;
	}

	invalidate_dcache_all();

	ret = ipc_send_msg(IPC_NODE_CORE0, boot_msg);
	if (ret != 0)
		return ret;

	ret = wait_boot_msg_ack();
	if (ret != 0)
		return ret;

	return 0;
}

static int early_init_dt_scan_chosen(const void *fdt, unsigned long node, const char *uname,
			int depth, void *data)
{
	unsigned long l = 0;
	char *p = NULL;
	char *cmdline = data;

	if (depth != 1 || !cmdline ||
		(strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	p = (char *)fdt_getprop(fdt, node, "bootargs", (int *)&l);
	if (p != NULL && l > 0)
		strlcpy(p, cmdline, COMMAND_LINE_SIZE);

	/* break now */
	return 1;
}

static inline const char *kbasename(const char *path)
{
	const char *tail = strrchr(path, '/');
	return tail ? tail + 1 : path;
}

static int scan_flat_dt_save_bootargs(const void *fdt, char *bootargs)
{
	const void *blob = fdt;
	const char *pathp = NULL;
	int offset;
	int rc = 0;
	int depth = -1;

	for (offset = fdt_next_node(blob, -1, &depth);
		offset >= 0 && depth >= 0 && !rc;
		offset = fdt_next_node(blob, offset, &depth)) {
		pathp = fdt_get_name(blob, offset, NULL);
		if (*pathp == '/')
			pathp = kbasename(pathp);

		if (!strcmp(pathp, "chosen") || !strcmp(pathp, "chosen@0")) {
			rc = early_init_dt_scan_chosen(fdt, offset, pathp, depth, bootargs);
			break;
		}
	}

	return rc;
}

static ulong load_kernel(ulong uimg_addr)
{
	ulong load_addr;
	image_header_t *hdr = (image_header_t *)(void *)uimg_addr;
	ulong kernel_addr = image_get_data(hdr);
	ulong kernel_size = image_get_data_size(hdr);

#if CONFIG_IS_ENABLED(CMD_TIMESTAMP)
	timestamp_mark("load_kernel_start", __LINE__);
#endif

	load_addr = get_kernel_load_addr();
	if (load_addr == OF_BAD_ADDR) {
		ipcboot_err("failed to load kernel!\n");
		return OF_BAD_ADDR;
	}

	if (kernel_addr != load_addr) {
		if (memmove_s((void *)load_addr, kernel_size, (void *)kernel_addr, kernel_size))
			ipcboot_err("memmove_s kernel_addr failed!");
	}

#if CONFIG_IS_ENABLED(CMD_TIMESTAMP)
	timestamp_mark("load_kernel_end", __LINE__);
#endif

	return load_addr;
}

static ulong load_fdt(ulong fdt_addr)
{
	ulong load_addr = get_fdt_load_addr();
	const void *fdt = (const void *)fdt_addr;
	(void)fdt_move(fdt, (void *)load_addr, fdt_totalsize(fdt));
	return load_addr;
}

static ulong load_fdt_appended(ulong uimgfdt_addr)
{
	image_header_t *hdr = (image_header_t *)(void *)uimgfdt_addr;
	ulong fdt_addr = image_get_image_end(hdr);
	return load_fdt(fdt_addr);
}

static void ipc_boot_bl31(ulong bl31_addr)
{
	uint32_t i = 0;
	struct ipc_share_msg msg;

	msg.cmd = IPC_CMD_START_BL31;
	msg.buf[i++] = bl31_addr >> ADDR_OFFSET;
	msg.buf[i++] = bl31_addr & 0xffffffff;
	msg.len = i;
	(void)send_boot_msg(&msg);
}

static void ipc_boot_teeimg(ulong teeimg_addr)
{
	uint32_t i = 0;
	struct ipc_share_msg msg;

	msg.cmd = IPC_CMD_START_TEEIMG;
	msg.buf[i++] = teeimg_addr >> ADDR_OFFSET;
	msg.buf[i++] = teeimg_addr & 0xffffffff;
	msg.len = i;
	(void)send_boot_msg(&msg);
}

static void ipc_boot_kernel(ulong kernel_base, ulong fdt_base)
{
	int ret;
	uint32_t i = 0;
	struct ipc_share_msg msg;
	ulong bl33_base = kernel_base;

	scan_flat_dt_save_bootargs((const void *)fdt_base, env_get("bootargs"));

	msg.cmd = IPC_CMD_START_KERNEL;
	msg.buf[i++] = bl33_base >> ADDR_OFFSET;
	msg.buf[i++] = bl33_base & 0xffffffff;
	msg.buf[i++] = fdt_base >> ADDR_OFFSET;
	msg.buf[i++] = fdt_base & 0xffffffff;
	msg.len = i;

	ret = send_boot_msg(&msg);
	if (ret != 0)
		return;

	cleanup_before_linux();
	gsl_core_pwrdwn();
}

static int do_ipcboot(struct cmd_tbl *cmdtp, int flag, int argc, char* const argv[])
{
	char *sub_cmd;

	if (argc < 2) /* 2: Number of parameters */
		return -1;

	sub_cmd = argv[1];

	if (strcmp(sub_cmd, "bl31") == 0 && argc == 3) { /* 3: Number of parameters */
		ulong bl31_addr = simple_strtoul(argv[2], NULL, 16);
		ipc_boot_bl31(bl31_addr);
	} else if (strcmp(sub_cmd, "teeimg") == 0 && argc == 3) { /* 3: Number of parameters */
		ulong teeimg_addr = simple_strtoul(argv[2], NULL, 16);
		ipc_boot_teeimg(teeimg_addr);
	} else if (strcmp(sub_cmd, "uimage") == 0 && argc == 4) { /* 4: Number of parameters */
		ulong uimg_addr = simple_strtoul(argv[2], NULL, 16);
		ulong fdt_addr = simple_strtoul(argv[3], NULL, 16);
		ulong fdt_base = load_fdt(fdt_addr);
		ulong kernel_base = load_kernel(uimg_addr);
		if (kernel_base == OF_BAD_ADDR)
			return 0;
		ipc_boot_kernel(kernel_base, fdt_base);
	} else if (strcmp(sub_cmd, "uimgfdt") == 0 && argc == 3) { /* 3: Number of parameters */
		ulong uimgfdt_addr = simple_strtoul(argv[2], NULL, 16);
		ulong fdt_base = load_fdt_appended(uimgfdt_addr);
		ulong kernel_base = load_kernel(uimgfdt_addr);
		if (kernel_base == OF_BAD_ADDR)
			return 0;
		ipc_boot_kernel(kernel_base, fdt_base);
	} else {
		ipcboot_err("Command error!\n");
		return -1;
	}
	return 0;
}

U_BOOT_CMD(ipcboot,  5,  1,  do_ipcboot,
	"Send booting signal by inter-processor communication\n\n"
	"Tips: Boot the bl31 or teeimg first, then the kernel",
	"bl31 <bl31 addr> - boot the raw BL31\n"
	"ipcboot teeimg <teeimg addr> - boot the BL31 and TEE OS\n"
	"    integrated in TEE Image\n"
	"ipcboot uimage <uImage addr> <fdt addr> - boot the kernel\n"
	"    in uImage with the given fdt\n"
	"ipcboot uimgfdt <uImage addr> - boot the kernel in uImage\n"
	"    with fdt appended\n"
);
