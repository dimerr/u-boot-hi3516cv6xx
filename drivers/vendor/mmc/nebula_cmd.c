/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#include <common.h>
#include <command.h>
#include <sdhci.h>

#include "sdhci_nebula.h"
#include "nebula_quick.h"

#define MMC_M_MASK		0x70003FFF
#define MMC_M_DS		(MMC_CAP(MMC_LEGACY))
#define MMC_M_HS		(MMC_CAP(MMC_HS) | MMC_M_DS)
#define MMC_M_HS_52		(MMC_CAP(MMC_HS_52) | MMC_HS)
#define MMC_M_DDR_52	(MMC_CAP(MMC_DDR_52) | MMC_M_HS_52)
#define MMC_M_HS200		(MMC_CAP(MMC_HS_200) | MMC_M_DDR_52)
#define MMC_M_HS400		(MMC_CAP(MMC_HS_400) | MMC_M_HS200)
#define MMC_M_HS400_ES	(MMC_CAP(MMC_HS_400_ES) | MMC_M_HS400)
#define M_STR_LEN        8

typedef struct emmc_mode {
	char mode[M_STR_LEN];
	u32 speed;
} emmc_mode_t;

static const emmc_mode_t g_emmc_mode[] = {
	{"hs400es", MMC_M_HS400_ES},
	{"hs400",   MMC_M_HS400},
	{"hs200",   MMC_M_HS200},
	{"hsd52",   MMC_M_DDR_52},
	{"hs52",    MMC_M_HS_52},
	{"hs",      MMC_M_HS},
	{"ds",      MMC_M_DS}
};

static void mmc_reset_capblity(struct mmc *mmc, char *esm)
{
	int i;
	struct sdhci_host *host = mmc->priv;
	struct mmc_config *cfg = &host->cfg;
#ifdef CONFIG_DM_MMC
	struct ext_sdhci_plat *plat = dev_get_plat(mmc->dev);
#endif

	if (esm != NULL) {
#ifdef CONFIG_DM_MMC
		cfg = &plat->cfg;
		sdhci_probe(mmc->dev);
#endif
		cfg->host_caps &= ~MMC_M_MASK;
		cfg->host_caps |= MMC_M_DS;
		for (i = 0; i < ARRAY_SIZE(g_emmc_mode); i++) {
			if (strstr(esm, g_emmc_mode[i].mode)) {
				cfg->host_caps |= g_emmc_mode[i].speed;
				break;
			}
		}

		cfg->host_caps |= MMC_MODE_1BIT;
		cfg->host_caps |= (strstr(esm, "-8")) ? (MMC_MODE_8BIT | MMC_MODE_4BIT) :
			 ((strstr(esm, "-4")) ? (MMC_MODE_4BIT) : MMC_MODE_1BIT);
	}
}

static struct mmc *get_mmc_device(int dev, bool force_init)
{
	struct mmc *mmc = NULL;

	mmc = find_mmc_device(dev);
	if (mmc == NULL) {
		printf("no mmc device at slot %x\n", dev);
		return NULL;
	}

	if (mmc_getcd(mmc) == 0)
		force_init = true;

	if (force_init)
		mmc->has_init = 0;

	if (mmc_init(mmc) != 0)
		return NULL;

#ifdef CONFIG_MMC_QUICKBOOT
	mmc_set_cur_mode(mmc->priv, INIT_MODE);
	mmc_save_parameters(mmc);
#endif

#ifdef CONFIG_BLOCK_CACHE
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	blkcache_invalidate(bd->if_type, bd->devnum);
#endif

	return mmc;
}

static int do_mmc_cmd(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	__maybe_unused int err;
	struct mmc *mmc = NULL;
	static int curr_device = -1;

	if (curr_device < 0) {
		if (get_mmc_num() <= 0)
			return CMD_RET_FAILURE;
		curr_device = 0;
	}

	if (argc < 2) /* args should be 2 at least */
		return CMD_RET_USAGE;

	mmc = get_mmc_device(curr_device, false);
	if (mmc == NULL)
		return CMD_RET_FAILURE;

	if (strcmp(argv[1], "version") == 0) {
		printf("version: %s\n", SDHCI_NEBULA_UBOOT_VERSION);
	} else if (strcmp(argv[1], "rescan") == 0) {
		mmc_reset_capblity(mmc, argv[2]); /* argv 2 */
		mmc = get_mmc_device(curr_device, true);
		if (mmc == NULL)
			return CMD_RET_FAILURE;
		sdhci_nebula_print_mmcinfo(mmc);
	} else if (strcmp(argv[1], "qsupport") == 0) {
#ifdef CONFIG_MMC_QUICKBOOT
		printf("yes\n");
	} else if (strcmp(argv[1], "reset") == 0) {
		err = mmc_reset(mmc);
		if (err != 0)
			return CMD_RET_FAILURE;
	} else if (strcmp(argv[1], "sleep") == 0) {
		err = mmc_enter_sleep(mmc);
		if (err != 0)
			return CMD_RET_FAILURE;
	} else if (strcmp(argv[1], "quick") == 0) {
		const char *s = NULL;
		if (argc == 2) { /* args 2 show mode */
			printf("qboot mode: %d\n", mmc_get_qboot_mode(mmc->priv));
			return CMD_RET_SUCCESS;
		}

		s = argv[2]; /* argv 2 */
		printf("quick setting %d> ", mmc_get_qboot_mode(mmc->priv));
		mmc_set_qboot_mode(mmc->priv, *s - '0');
		printf("%d\n", mmc_get_qboot_mode(mmc->priv));
#endif
	} else {
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	mmcut, CONFIG_SYS_MAXARGS, 0, do_mmc_cmd,
	"mmc user tools",
	"version - mmc version show\n"
#ifdef CONFIG_MMC_QUICKBOOT
	"mmcut reset - set mmc go to idle mode\n"
	"mmcut sleep - set mmc device enter sleep mode\n"
	"mmcut quick <0|1|2> - <disable|warm|cold> set mmc quick boot mode\n"
#endif
	"mmcut rescan <esm mode> - rescan mmc device with choice mode\n"
	"     esm mode format: <mode>-<bit_witdh>\n"
	"     mode: ds, hs, hs52, hsd52, hs200, hs400, hs400es\n"
	"     bit_width: 1,4,8\n"
	"     esm mode sample: hs200-4\n"
);
