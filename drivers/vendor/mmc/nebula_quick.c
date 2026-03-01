/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#include <common.h>
#include <div64.h>
#include <sdhci.h>
#include <linux/delay.h>

#include "sdhci_nebula.h"
#include "mmc_import.h"
#include "nebula_platform.h"
#include "nebula_quick.h"

#ifdef CONFIG_MMC_QUICKBOOT
void mmc_set_cur_mode(struct sdhci_host *host, emmc_mode_u mode)
{
	emmc_param_gen1_u param1;
	struct sdhci_nebula *nebula = host->priv;

	param1.u32 = readl(nebula->qboot_param1_addr);
	param1.bits.emmc_cur_mode = mode;
	writel(param1.u32, nebula->qboot_param1_addr);
}

emmc_mode_u mmc_get_cur_mode(struct sdhci_host *host)
{
	emmc_param_gen1_u param1;
	struct sdhci_nebula *nebula = host->priv;

	param1.u32 = readl(nebula->qboot_param1_addr);

	return (emmc_mode_u)param1.bits.emmc_cur_mode;
}

emmc_qboot_u mmc_get_qboot_mode(struct sdhci_host *host)
{
	emmc_param_gen1_u param1;
	struct sdhci_nebula *nebula = host->priv;

	param1.u32 = readl(nebula->qboot_param1_addr);

	return (emmc_qboot_u)param1.bits.emmc_qboot_mode;
}

void mmc_set_qboot_mode(struct sdhci_host *host, emmc_qboot_u mode)
{
	emmc_param_gen1_u param1;
	struct sdhci_nebula *nebula = host->priv;

	if (mode < QUICK_BOOT_DIS || mode >= QUICK_BOOT_MAX) {
		printf("unavalid qboot mode %d\n", mode);
		return;
	}

	param1.u32 = readl(nebula->qboot_param1_addr);
	param1.bits.emmc_qboot_mode = mode;

	writel(param1.u32, nebula->qboot_param1_addr);
}

bool mmc_is_fast_boot(struct sdhci_host *host)
{
	return (host->index == MMC_PORT_0) && (mmc_get_qboot_mode(host) != QUICK_BOOT_DIS);
}

void mmc_disable_fast_boot(struct sdhci_host *host)
{
	mmc_set_qboot_mode(host, QUICK_BOOT_DIS);
}

static int mmc_check_status(struct mmc *mmc)
{
	int err;
	unsigned int status;

	err = mmc_send_status(mmc, &status);
	if (err != 0) {
		printf("get emmc status failed.\n");
		return err;
	}

	if (!(status & MMC_STATUS_RDY_FOR_DATA) ||
		((status & MMC_STATUS_CURR_STATE) == MMC_STATE_PRG)) {
		printf("emmc not ready status = 0x%x\n", status);
		return -ECOMM;
	}

	return 0;
}

static void mmc_bdesc_init(struct mmc *mmc)
{
	struct blk_desc *bdesc;

	/* fill in device description */
	bdesc = mmc_get_blk_desc(mmc);
	if (bdesc == NULL)
		return;

	bdesc->lun = 0;
	bdesc->hwpart = 0;
	bdesc->type = 0;
	bdesc->blksz = mmc->read_bl_len;
	bdesc->log2blksz = LOG2(bdesc->blksz);
	bdesc->lba = lldiv(mmc->capacity, mmc->read_bl_len);
}

static void mmc_reparse_cid(struct mmc *mmc)
{
	int i;
	struct sdhci_host *host = mmc->priv;

	/* CRC is stripped so we need to do some shifting. */
	for (i = 0; i < 4; i++) { /* 4: resp len */
		mmc->cid[i] = sdhci_readl(host,
				SDHCI_RESPONSE + (3 - i) * 4) << 8; /* 3,4,8: idx */
		if (i != 3) /* 3: idx */
			mmc->cid[i] |= sdhci_readb(host,
					SDHCI_RESPONSE + (3 - i) * 4 - 1); /* 3,4: idx */
	}
	/* CID[3] is invalid */
	mmc->cid[3] = 0;
}

static void mmc_paramters_init(struct mmc *mmc)
{
	emmc_param_gen1_u param1;
	struct sdhci_host *host = mmc->priv;
	struct sdhci_nebula *nebula = host->priv;
	u8 bus_width[] = {MMC_BUS_WIDTH_1BIT, MMC_BUS_WIDTH_4BIT, MMC_BUS_WIDTH_8BIT, MMC_BUS_WIDTH_1BIT};
	u8 uhs_mode[] = {MMC_LEGACY, MMC_HS, 0, MMC_HS_200, MMC_DDR_52, 0, 0, MMC_HS_400};

	param1.u32 = readl(nebula->qboot_param1_addr);

	mmc->host_caps = mmc->cfg->host_caps | MMC_CAP(MMC_LEGACY) | MMC_MODE_1BIT;

	mmc->ocr = nebula->ocr_from_bootrom;
	if (mmc_get_cur_mode(host) == DS_MODE) {
		/* DS set to MMC_LEGACY mode, should be reassignment */
		param1.bits.emmc_uhs_mode_sel = 0x0;
		/* DS hcs mode from bootrom */
		param1.bits.emmc_hcs_mode = !!(mmc->ocr & OCR_HCS);
		/* DS set bus width to 0x0, should be reassignment */
		param1.bits.emmc_bus_width = 0x0;
		/* DS enhance strobe should be disabled */
		param1.bits.emmc_enh_strobe = 0x0;
	}

	mmc->read_bl_len = MMC_MAX_BLOCK_LEN;
	mmc->write_bl_len = MMC_MAX_BLOCK_LEN;
	mmc->high_capacity = param1.bits.emmc_hcs_mode;
	/* follow uboot, rca set to 0x1 */
	mmc->rca = 0x1;

	mmc->bus_width = bus_width[param1.bits.emmc_bus_width];
	mmc->selected_mode = uhs_mode[param1.bits.emmc_uhs_mode_sel];

	if (param1.bits.emmc_enh_strobe == 1) {
#if !CONFIG_IS_ENABLED(MMC_HS400_ES_SUPPORT)
		/* Enhace mode has been restored, warning only */
		printf("Warning: HS400 ES mismatch between kernel and uboot.\n");
#else
		mmc->selected_mode = MMC_HS_400_ES;
#endif
	}

	mmc_select_mode(mmc, mmc->selected_mode);
	mmc->clock = mmc->tran_speed;
	if (mmc_get_cur_mode(host) == TRAN_MODE) {
		nebula->tuning_phase = plat_get_rx_sample_phase(host);
		sdhci_nebula_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);
		mmc_set_clock(mmc, mmc->clock, MMC_CLK_ENABLE);
	}

	/* Warm boot CID is invalid, set vendor name "*" */
	mmc->cid[0] = 0x2a; /* "*" */
	mmc->cid[1] = 0x2a2a2a2a; /* "****" */
	mmc->part_config = MMCPART_NOAVAILABLE;
}

int mmc_restore(struct mmc *mmc)
{
	int err;

	if (mmc == NULL)
		return -EINVAL;

	/* BOOT_MODE not support fast boot */
	if (mmc_get_cur_mode(mmc->priv) == BOOT_MODE)
		return 0;

	mmc_paramters_init(mmc);

	/* Only support eMMC 4.0 upper version */
	mmc->version = MMC_VERSION_4;
	err = mmc_startup_v4(mmc);
	if (err != 0) {
		printf("parse device configuration failed\n");
		return err;
	}
	mmc->capacity = mmc->capacity_user;

	if (mmc_get_cur_mode(mmc->priv) == DS_MODE) {
		/* DS mode, bootrom get cid by respone[3:0],
		 * respone[0] should corrupted by other cmd,
		 * only for reference.
		 */
		mmc_reparse_cid(mmc);

		err = mmc_get_capabilities(mmc);
		if (err != 0) {
			printf("mmc get capabilities failed.\n");
			return err;
		}

		err = mmc_select_mode_and_width(mmc, mmc->card_caps);
		if (err != 0) {
			printf("mmc select mode failed.\n");
			return err;
		}
	}
	mmc->best_mode = mmc->selected_mode;

	mmc_bdesc_init(mmc);

	err = mmc_check_status(mmc);
	if (err != 0)
		return err;

	mmc->has_init = 1;

	return 0;
}

int mmc_save_parameters(struct mmc *mmc)
{
	emmc_param_gen0_u param0;
	emmc_param_gen1_u param1;
	struct sdhci_host *host = NULL;
	struct sdhci_nebula *nebula = NULL;

	if ((mmc == NULL) || (mmc->priv == NULL)) {
		printf("mmc struct or priv invalid\n");
		return -EINVAL;
	}

	host = mmc->priv;
	nebula = host->priv;

	/* parameters had been saved before, cold boot no need */
	if ((mmc_get_cur_mode(host) == TRAN_MODE) || \
		(mmc_get_qboot_mode(host) == QUICK_BOOT_COLD))
		return 0;

	param0.u32 = readl(nebula->qboot_param0_addr);

	/* store clock freq */
	param0.bits.emmc_clk_sel = plat_get_clk(host);
	/* store driver phase */
	param0.bits.emmc_clk_ph_sel = plat_get_clk_phase(host);
	/* store software clock phase */
	param0.bits.emmc_sw_clk_ph = sdhci_readb(host, SDHCI_AT_STAT);

	writel(param0.u32, nebula->qboot_param0_addr);

	/* store io information to pmc register */
	plat_get_io_drv_str_info(host);

	param1.u32 = readl(nebula->qboot_param1_addr);
	param1.bits.emmc_uhs_mode_sel = sdhci_readb(host, SDHCI_HOST_CONTROL2) & SDHCI_CTRL_UHS_MASK;
	param1.bits.emmc_enh_strobe =  (mmc->selected_mode == MMC_HS_400_ES);
	/* 0: 1bits, 1: 4bits, 2: 8bits */
	param1.bits.emmc_bus_width = (mmc->bus_width == MMC_BUS_WIDTH_8BIT) ? 0x2 : \
								((mmc->bus_width == MMC_BUS_WIDTH_4BIT) ? 1 : 0);
	param1.bits.emmc_hcs_mode = mmc->high_capacity;
	writel(param1.u32, nebula->qboot_param1_addr);

	mmc_set_cur_mode(host, TRAN_MODE);

	return 0;
}

const char *mmc_get_mode_str(struct sdhci_host *host)
{
	emmc_mode_u bmode;
	static char *boot_str[] = {"boot", "ds", "tran"};

	bmode = mmc_get_cur_mode(host);
	if (bmode == INIT_MODE)
		return "restore";

	if (bmode >= BOOT_MODE && bmode <= TRAN_MODE)
		return boot_str[bmode - BOOT_MODE];

	return "";
}

int mmc_enter_sleep(struct mmc *mmc)
{
	int err;
	struct mmc_cmd cmd;

	mmc_save_parameters(mmc);

	/* disconnect card(set card to stand-by mode) */
	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_NONE;
	cmd.cmdarg = 0x0;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err != 0) {
		printf("Disconnect card failed, err = %d\n", err);
		return err;
	}

	/* set card to sleep mode */
	cmd.cmdidx = MMC_CMD_SLEEP_AWAKE;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << MMC_RCA_SHIFT_BIT;
	cmd.cmdarg |= 1 << MMC_SAWAKE_SHIFT_BIT;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err != 0) {
		printf("Enter sleep mode failed, err = %d\n", err);
		return err;
	}

	/* Only warm boot support */
	if (mmc_get_qboot_mode(mmc->priv) == QUICK_BOOT_WARM)
		mmc_set_cur_mode(mmc->priv, SLEEP_MODE);

	return 0;
}

int mmc_reset(struct mmc *mmc)
{
	int err;
	struct mmc_cmd cmd;

	udelay(1000); /* 1000us */

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err != 0)
		return err;

	udelay(2000); /* 2000us */

	/* Only warm boot support */
	if (mmc_get_qboot_mode(mmc->priv) == QUICK_BOOT_WARM)
		mmc_set_cur_mode(mmc->priv, SLEEP_MODE);

	return 0;
}
#else /* ! CONFIG_MMC_QUICKBOOT */

bool mmc_is_fast_boot(struct sdhci_host *host)
{
	return false;
}

void mmc_disable_fast_boot(struct sdhci_host *host) {}
void mmc_set_cur_mode(struct sdhci_host *host, emmc_mode_u mode) {}
emmc_mode_u mmc_get_cur_mode(struct sdhci_host *host)
{
	return 0;
}

int mmc_restore(struct mmc *mmc)
{
	return 0;
}

int mmc_save_parameters(struct mmc *mmc)
{
	return 0;
}

const char *mmc_get_mode_str(struct sdhci_host *host)
{
	return "";
}

#endif
