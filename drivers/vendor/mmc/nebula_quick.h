/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#ifndef _NEBULA_QUICK_H_
#define _NEBULA_QUICK_H_

#include <mmc.h>

#define MMC_CMD_SLEEP_AWAKE         5
#define MMC_SAWAKE_SHIFT_BIT        15
#define MMC_RCA_SHIFT_BIT           16

/* emmc parameters type */
typedef union {
	struct {
		u32 emmc_clk_ph_sel          : 5; // [4:0]
		u32 emmc_clk_sel             : 3; // [7:5]
		u32 emmc_sw_clk_ph           : 8; // [15:8]
		u32 emmc_cmd_drv             : 4; // [19:16]
		u32 emmc_cmd_sl              : 1; // [20]
		u32 emmc_clk_drv             : 4; // [24:21]
		u32 emmc_clk_sl              : 1; // [25]
		u32 emmc_data_drv            : 4; // [29:26]
		u32 emmc_data_sl             : 1; // [30]
		u32 reserved_0               : 1; // [31]
	} bits;
	u32 u32;
} emmc_param_gen0_u;

/* emmc parameters type */
typedef union {
	struct {
		u32 emmc_uhs_mode_sel        : 3;  // [2:0]
		u32 emmc_enh_strobe          : 1;  // [3]
		u32 emmc_bus_width           : 2;  // [5:4]
		u32 emmc_hcs_mode            : 1;  // [6]
		u32 emmc_spec_ver            : 4;  // [10:7]
		u32 emmc_chip_size           : 11; // [21:11]
		u32 emmc_qboot_mode          : 2;  // [23:22]
		u32 emmc_cur_mode            : 8;  // [31:24]
	} bits;
	u32 u32;
} emmc_param_gen1_u;

typedef enum {
	INIT_MODE = 0x0,
	SLEEP_MODE = 0x5A,
	BOOT_MODE,
	DS_MODE,
	TRAN_MODE,
} emmc_mode_u;

typedef enum {
	QUICK_BOOT_DIS = 0x0,
	QUICK_BOOT_WARM,
	QUICK_BOOT_COLD,
	QUICK_BOOT_MAX,
} emmc_qboot_u;

/* export by quick boot module  */
bool mmc_is_fast_boot(struct sdhci_host *host);
void mmc_disable_fast_boot(struct sdhci_host *host);
void mmc_set_cur_mode(struct sdhci_host *host, emmc_mode_u mode);
emmc_mode_u mmc_get_cur_mode(struct sdhci_host *host);
const char *mmc_get_mode_str(struct sdhci_host *host);
int mmc_restore(struct mmc *mmc);
int mmc_save_parameters(struct mmc *mmc);
int mmc_reset(struct mmc *mmc);
int mmc_enter_sleep(struct mmc *mmc);
emmc_qboot_u mmc_get_qboot_mode(struct sdhci_host *host);
void mmc_set_qboot_mode(struct sdhci_host *host, emmc_qboot_u mode);

#endif
