/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#include <common.h>
#include <dm.h>
#include <malloc.h>
#include <memalign.h>
#include <mmc.h>
#include <sdhci.h>
#include <linux/delay.h>
#include <linux/errno.h>

#include "mmc_private.h"
#include "ext_mmc.h"
#include "nebula_quick.h"
#include "nebula_platform.h"
#include "sdhci_nebula.h"

void sdhci_nebula_reset(struct sdhci_host *host, u8 mask)
{
	unsigned long timeout;

	/* Wait max 100 ms, 10000 loop */
	timeout = 10000;
	sdhci_writeb(host, mask, SDHCI_SOFTWARE_RESET);
	while ((sdhci_readb(host, SDHCI_SOFTWARE_RESET) & mask) != 0) {
		if (timeout == 0) {
			printf("%s: Reset 0x%x never completed.\n",
			       __func__, (int)mask);
			return;
		}
		timeout--;
		udelay(10); /* 10us per loop */
	}
}

#ifdef MMC_SUPPORTS_TUNING
static void nebula_select_rx_sample_phase(struct sdhci_host *host, u32 phase)
{
	plat_disable_card_clk(host);
	plat_set_rx_sample_phase(host, phase);
	plat_wait_sample_dll_slave_ready(host);
	plat_enable_card_clk(host);
	udelay(1);
}

static void nebula_enable_edge_tuning(struct sdhci_host *host)
{
	u32 reg;

	reg = sdhci_readl(host, SDHCI_MULTI_CYCLE);
	reg |= SDHCI_EDGE_DETECT_EN;
	sdhci_writel(host, reg, SDHCI_MULTI_CYCLE);
}

static void nebula_disable_edge_tuning(struct sdhci_host *host)
{
	u32 reg;

	reg = sdhci_readl(host, SDHCI_MULTI_CYCLE);
	reg &= ~SDHCI_EDGE_DETECT_EN;
	sdhci_writel(host, reg, SDHCI_MULTI_CYCLE);
}

static int nebula_send_tuning(struct mmc *mmc, u32 opcode)
{
	int err;

	err = mmc_send_tuning(mmc, opcode, NULL);
	if (err != 0) {
		mdelay(1);
		mmc_abort_tuning(mmc, MMC_CMD_SEND_TUNING_BLOCK_HS200);
	}

	return err;
}

static int nebula_get_best_edges_range(struct mmc *mmc, u32 opcode,
				unsigned int *edge_p2f_ptr, unsigned int *edge_f2p_ptr)
{
	int err;
	unsigned int index, val;
	bool found = false;
	bool prev_found = false;
	struct sdhci_host *host = mmc->priv;
	unsigned int edge_p2f, edge_f2p, start, end;

	start = 0;
	end = PHASE_SCALE / EDGE_TUNING_PHASE_STEP;

	edge_p2f = start;
	edge_f2p = end;
	for (index = 0; index <= end; index++) {
		nebula_select_rx_sample_phase(host, index * EDGE_TUNING_PHASE_STEP);

		err = nebula_send_tuning(mmc, opcode);
		if (err == 0) {
			val = sdhci_readl(host, SDHCI_MULTI_CYCLE);
			found = ((val & SDHCI_FOUND_EDGE) != 0);
		} else {
			found = true;
		}

		if (prev_found && !found) {
			edge_f2p = index;
		} else if (!prev_found && found) {
			edge_p2f = index;
		}

		if ((edge_p2f != start) && (edge_f2p != end))
			break;

		prev_found = found;
		found = false;
	}

	if ((edge_p2f == start) && (edge_f2p == end))
		return NOT_FOUND;

	*edge_p2f_ptr = edge_p2f;
	*edge_f2p_ptr = edge_f2p;

	return 0;
}

static unsigned int nebula_get_best_phase(struct mmc *mmc, u32 opcode,
				unsigned int edge_p2f_ptr, unsigned int edge_f2p_ptr)
{
	int err;
	int prev_err = 0;
	unsigned int index, start, end;
	unsigned int phase, fall, rise;
	bool fall_updat_flag = false;
	struct sdhci_host *host = mmc->priv;

	start = edge_p2f_ptr * EDGE_TUNING_PHASE_STEP;
	end = edge_f2p_ptr * EDGE_TUNING_PHASE_STEP;
	if (end <= start)
		end += PHASE_SCALE;

	fall = start;
	rise = end;
	for (index = start; index <= end; index++) {
		nebula_select_rx_sample_phase(host, index % PHASE_SCALE);

		err = nebula_send_tuning(mmc, opcode);
		if (err != 0) {
			pr_debug("send tuning CMD%u fail! phase:%d err:%d\n",
				opcode, index, err);
		}

		if ((err != 0) && (index == start)) {
			if (!fall_updat_flag) {
				fall_updat_flag = true;
				fall = start;
			}
		} else if ((prev_err == 0) && (err != 0)) {
			if (!fall_updat_flag) {
				fall_updat_flag = true;
				fall = index;
			}
		}

		if ((prev_err != 0) && (err == 0))
			rise = index;

		if ((err != 0) && (index == end)) {
			rise = end;
		}

		prev_err = err;
	}

	/* Calculate the center value by devide 2 */
	phase = ((fall + rise) / 2 + PHASE_SCALE / 2) % PHASE_SCALE;

	printf("tuning done! valid phase shift [%d, %d] Final Phase:%d\n",
			rise % PHASE_SCALE, fall % PHASE_SCALE, phase);

	return phase;
}

static int sdhci_nebula_edge_tuning(struct mmc *mmc, u32 opcode)
{
	int err ;
	u16 ctrl;
	struct sdhci_host *host = mmc->priv;
	struct sdhci_nebula *nebula = host->priv;
	unsigned int phase, edge_p2f, edge_f2p;

	plat_prepare_tuning(host);
	plat_enbale_rx_sample_tuning(host);

	nebula_enable_edge_tuning(host);
	nebula->is_tuning = true;

	err = nebula_get_best_edges_range(mmc, opcode, &edge_p2f, &edge_f2p);
	if (err == NOT_FOUND) {
		nebula->is_tuning = false;
		nebula_disable_edge_tuning(host);
		printf("tuning failed! can not found edge!\n");
		return err;
	}

	nebula_disable_edge_tuning(host);

	phase = nebula_get_best_phase(mmc, opcode, edge_p2f, edge_f2p);

	nebula_select_rx_sample_phase(host, phase);
	nebula->tuning_phase = phase;

	ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl |= SDHCI_CTRL_TUNED_CLK;
	sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
	nebula->is_tuning = false;

	return 0;
}

static u32 mci_ror32(u32 word, u32 shift)
{
	if (shift > 31) /* 31 for Int highest bit */
		shift = 0;

	return (u32)(word >> shift) & 0x1;
}

static int sdhci_nebula_normal_tuning(struct mmc *mmc, u32 opcode)
{
	struct sdhci_host *host = mmc->priv;
	u32 i, shift, tuning_val, tuning_next_val;
	u32 fall = 0;
	u32 rise = 0;
	u32 candiates = 0;
	u32 length = 0;
	u32 tuning_sel = 0;

	plat_prepare_tuning(host);
	plat_enbale_rx_sample_tuning(host);

	for (i = 0; i < PHASE_SCALE; i++) {
		nebula_select_rx_sample_phase(host, i);

		if (nebula_send_tuning(mmc, opcode) == 0)
			candiates |= (1 << i);
	}
	printf("candiates: 0x%x\n", candiates);

	for (shift = 0; shift < PHASE_SCALE; shift++) {
		tuning_val = mci_ror32(candiates, 0);
		if (tuning_val != 0)
			candiates = (candiates >> 1) | 1 << 31; /* 31 for Int highest bit */
		else
			break;
	}

	for (i = 0; i < PHASE_SCALE; i++) {
		tuning_val = mci_ror32(candiates, i);
		tuning_next_val = mci_ror32(candiates, i + 1);
		if (tuning_val > tuning_next_val) {
			fall = i;
			if ((fall - rise) > length) {
				length = fall - rise;
				tuning_sel = (fall + rise + 1) / 2; /* Calculate the center value by devide 2 */
			}
		} else if (tuning_val < tuning_next_val) {
			rise = i;
		}
	}

	tuning_sel = (tuning_sel + shift) % PHASE_SCALE;
	printf("tuning_sel: 0x%x\n", tuning_sel);

	nebula_select_rx_sample_phase(host, tuning_sel);

	return 0;
}

static int sdhci_nebula_execute_tuning(struct mmc *mmc, u8 opcode)
{
	struct sdhci_host *host = mmc->priv;
	struct sdhci_nebula *nebula = host->priv;

	if ((nebula->priv_quirk & NEBULA_QUIRK_SAMPLE_TURNING) != 0)
		return sdhci_nebula_normal_tuning(mmc, (u32)opcode);

	return sdhci_nebula_edge_tuning(mmc, (u32)opcode);
}
#endif

static int sdhci_nebula_set_uhs_signaling(struct sdhci_host *host)
{
	u16 ctrl;

	ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl &= ~SDHCI_CTRL_UHS_MASK;

	switch (host->mmc->selected_mode) {
	case MMC_HS:
	case MMC_HS_52:
	case SD_HS:
	case UHS_SDR25:
		ctrl |= SDHCI_CTRL_UHS_SDR25;
		break;
	case UHS_SDR50:
		ctrl |= SDHCI_CTRL_UHS_SDR50;
		break;
	case MMC_HS_200:
	case UHS_SDR104:
		ctrl |= SDHCI_CTRL_UHS_SDR104;
		break;
	case MMC_DDR_52:
	case UHS_DDR50:
		ctrl |= SDHCI_CTRL_UHS_DDR50;
		break;
	case MMC_HS_400:
	case MMC_HS_400_ES:
		ctrl |= SDHCI_CTRL_HS400;
		break;
	default:
		break;
	}
	sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);

	return 0;
}

static int sdhci_nebula_power_cycle(struct sdhci_host *host)
{
	plat_power_cycle(host);
	return 0;
}

static int sdhci_nebula_set_enhanced_strobe(struct sdhci_host *host)
{
	plat_set_enhanced_strobe(host, true);
	return 0;
}

#define EXT_CSD_COND_BUS_WIDTH	3
static void mmc_enable_ext_csd_width(struct mmc *mmc, u8 *ext_csd, u16 len)
{
	int ret;
	unsigned int cur_boot_part, cur_boot_bus_width, boot_part, boot_bus_width;

	boot_bus_width = (plat_get_bus_width(mmc->priv) == MMC_BUS_WIDTH_8BIT) ?
		EXT_CSD_BUS_WIDTH_8 : EXT_CSD_BUS_WIDTH_4;
	cur_boot_part = EXT_CSD_EXTRACT_BOOT_PART(ext_csd[EXT_CSD_PART_CONF]);
	cur_boot_bus_width = ext_csd[EXT_CSD_BOOT_BUS_WIDTH] & 0x3; /* bit0 - 1 */
	/* 0x1 is boot1 area enable, 0x2 is boot2 area enable */
	if (cur_boot_part != 0x1 && cur_boot_part != 0x2)
		boot_part = 0x7; /* user area enable for boot */
	else
		boot_part = cur_boot_part;
	if (cur_boot_part == boot_part && cur_boot_bus_width == boot_bus_width &&
	    EXT_CSD_EXTRACT_BOOT_ACK(ext_csd[EXT_CSD_PART_CONF]) == 0)
		return;

	if (cur_boot_part != boot_part ||
	    EXT_CSD_EXTRACT_BOOT_ACK(ext_csd[EXT_CSD_PART_CONF]) != 0) {
		cur_boot_part = ext_csd[EXT_CSD_PART_CONF];
		cur_boot_part &= (~(0x3 << EXT_CSD_COND_BUS_WIDTH));
		cur_boot_part |= (boot_part << EXT_CSD_COND_BUS_WIDTH);
		/* boot_ack bit (bit6) always is 0 */
		cur_boot_part &= (~EXT_CSD_BOOT_ACK_ENABLE);
		ret = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONF, cur_boot_part);
		if (ret != 0) {
			printf("Set EXT_CSD_PART_CONF error!\n");
			return;
		}
	}
	if (cur_boot_bus_width != boot_bus_width) {
		cur_boot_bus_width = ext_csd[EXT_CSD_BOOT_BUS_WIDTH];
		cur_boot_bus_width &= (~0x3); /* bit0 - 1 */
		cur_boot_bus_width |= boot_bus_width;
		ret = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_BOOT_BUS_WIDTH, cur_boot_bus_width);
		if (ret != 0) {
			printf("set EXT_CSD_BOOT_BUS_WIDTH error!\n");
			return;
		}
	}

	ret = mmc_send_ext_csd(mmc, ext_csd);
	if (ret != 0) {
		printf("Check est_csd error!\n");
		return;
	}
	cur_boot_part = (ext_csd[EXT_CSD_PART_CONF] >> 0x3) & 0xF;
	cur_boot_bus_width = ext_csd[EXT_CSD_BOOT_BUS_WIDTH] & 0x3; /* bit0 - 1 */

	if (cur_boot_part == boot_part && cur_boot_bus_width == boot_bus_width)
		printf("EXT_CSD CONFIG OK!\n");
	else
		printf("EXT_CSD CONFIG Fail!\n");
}

static void mmc_enable_rst_relwr(struct mmc *mmc)
{
	int ret;
	ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);

	ret = mmc_send_ext_csd(mmc, ext_csd);
	if (ret != 0) {
		printf("%s get ext_csd error!\n", __func__);
		return;
	}

	if (ext_csd[EXT_CSD_REV] < 5) /* 5 for MMC v4.41 */
		return;

	if ((ext_csd[EXT_CSD_RST_N_FUNCTION] & EXT_CSD_RST_N_EN_MASK)
		!= EXT_CSD_RST_N_ENABLED) {
		ret = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_RST_N_FUNCTION,
				EXT_CSD_RST_N_ENABLED);
		if (ret != 0) {
			printf("mmc: Enable EXT_CSD_RST_N_FUNCTION failed!\n");
			return;
		}
	}

	if ((ext_csd[EXT_CSD_WR_REL_PARAM] &
	   (EXT_CSD_WR_REL_PARAM_HCR | EXT_CSD_WR_REL_PARAM_EN)) ==
	   (EXT_CSD_WR_REL_PARAM_HCR | EXT_CSD_WR_REL_PARAM_EN)) {
		if (ext_csd[EXT_CSD_WR_REL_SET] != EXT_CSD_WR_REL_VALUE) {
			ret = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_WR_REL_SET,
					EXT_CSD_WR_REL_VALUE);
			if (ret != 0) {
				printf("mmc: Enable EXT_CSD_WR_REL_SET failed!\n");
				return;
			}
		}
	}

	mmc_enable_ext_csd_width(mmc, ext_csd, MMC_MAX_BLOCK_LEN);
}

static void sdhci_nebula_extra_init(struct sdhci_host *host)
{
	plat_sdhci_extra_init(host);
}

static void sdhci_nebula_set_clock(struct sdhci_host *host, unsigned int clk)
{
	plat_sdhci_set_clock(host, clk);
}

static const struct sdhci_ops g_ext_sdhci_ops = {
	.set_clock	                = sdhci_nebula_set_clock,
	.set_ios_post	            = sdhci_nebula_set_uhs_signaling,
	.extra_init	                = sdhci_nebula_extra_init,
	.set_enhanced_strobe        = sdhci_nebula_set_enhanced_strobe,
	.power_cycle                = sdhci_nebula_power_cycle,
#ifdef MMC_SUPPORTS_TUNING
	.platform_execute_tuning    = sdhci_nebula_execute_tuning,
#endif
};

static void nebula_set_host_caps(struct sdhci_host *host)
{
	if (plat_get_bus_width(host) == MMC_BUS_WIDTH_8BIT)
		host->host_caps |= MMC_MODE_HS400_ES |
			MMC_MODE_HS400 | MMC_MODE_8BIT;
}

#ifdef CONFIG_DM_MMC
static int nebula_sdhci_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct ext_sdhci_plat *plat = dev_get_plat(dev);
	struct sdhci_host *host = dev_get_priv(dev);
	const u32 index = 0;
	int ret;
	fdt_addr_t base;

	base = devfdt_get_addr(dev);
	if (base == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = mmc_of_parse(dev, &plat->cfg);
	if (ret != 0)
		return ret;

	host->mmc = &plat->mmc;
	host->mmc->dev = dev;
	upriv->mmc = &plat->mmc;
	host->mmc->priv = host;
	host->priv = &plat->nebula;

	host->name = dev->name;
	host->max_clk = plat->nebula.max_clk;
	host->ioaddr = (void *)base;
	host->index = index;
	host->version = SDHCI_SPEC_300;
	host->ops = &g_ext_sdhci_ops;

	plat->nebula.is_tuning = false;
	plat->nebula.max_clk = CLK_200M;
	plat_host_pre_init(host);

	plat_sdhci_init(host);
	plat_phy_init(host);

	nebula_set_host_caps(host);

	ret = sdhci_setup_cfg(&plat->cfg, host, plat->nebula.max_clk, MIN_FREQ);
	if (ret != 0) {
		printf("%s: failed to setup sdhci, err=%d\n", __func__, ret);
		return ret;
	}

	ret = sdhci_probe(dev);
	if (ret != 0) {
		printf("%s: failed to setup sdhci, err=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int nebula_sdhci_bind(struct udevice *dev)
{
	struct ext_sdhci_plat *plat = dev_get_plat(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id g_ext_mmc_match[] = {
	{ .compatible = "huanglong,sdhci" },
	{ }
};

U_BOOT_DRIVER(soc_mmc) = {
	.name = "soc_mmc",
	.id = UCLASS_MMC,
	.of_match = g_ext_mmc_match,
	.ops = &sdhci_ops,
	.bind = nebula_sdhci_bind,
	.probe = nebula_sdhci_probe,
	.priv_auto = sizeof(struct sdhci_host),
	.plat_auto = sizeof(struct ext_sdhci_plat),
};

#else
int ext_sdhci_add_port(int index, uintptr_t regbase)
{
	ulong start;
	struct sdhci_host *host = NULL;
	struct sdhci_nebula *nebula = NULL;

	start = get_timer(0);
	host = calloc(1, sizeof(struct sdhci_host));
	if (host == NULL) {
		puts("sdhci_soc: sdhci_host malloc fail!\n");
		return -ENOMEM;
	}

	nebula = calloc(1, sizeof(struct sdhci_nebula));
	if (nebula == NULL) {
		puts("sdhci_soc: sdhci_nebula malloc fail!\n");
		free(host);
		return -ENOMEM;
	}

	host->priv = nebula;
	host->name = "soc-sdhci";
	host->ioaddr = (void *)regbase;
	host->index = index;
	host->version = SDHCI_SPEC_300;
	host->ops = &g_ext_sdhci_ops;

	nebula->max_clk = CLK_200M;
	plat_host_pre_init(host);

	nebula_set_host_caps(host);

	host->max_clk = nebula->max_clk;
	nebula->is_tuning = false;
	nebula->is_quickboot = mmc_is_fast_boot(host);

	if (nebula->is_quickboot) {
		if (mmc_get_cur_mode(host) == BOOT_MODE)
			plat_sdhci_init(host);
	} else {
		plat_sdhci_init(host);
	}

	plat_phy_init(host);
	nebula->host_init_time = get_timer(start);

	return add_sdhci(host, nebula->max_clk, CLK_400K);
}
#endif

void sdhci_nebula_print_mmcinfo(struct mmc *mmc)
{
	printf("MMC/SD Card:\n");
	printf("    MID:         0x%x\n", mmc->cid[0] >> 24); /* bit24 - 31 */
	printf("    Read Block:  %d Bytes\n", mmc->read_bl_len);
	printf("    Write Block: %d Bytes\n", mmc->write_bl_len);
	printf("    Chip Size:   %lld Bytes (%s)\n", mmc->capacity,
			mmc->high_capacity ? "High Capacity" : "Low Capacity");
	printf("    Name:        \"%c%c%c%c%c\"\n",
			 mmc->cid[0] & 0xff,          /* bit0  -  7 */
			(mmc->cid[1] >> 24),          /* bit24 - 32 */
			(mmc->cid[1] >> 16) & 0xff,   /* bit16 - 23 */
			(mmc->cid[1] >> 8) & 0xff,    /* bit8  - 15 */
			 mmc->cid[1] & 0xff);         /* bit0  -  7 */

	printf("    Chip Type:   %s\n"
			"    Version:     %d.%d\n",
			IS_SD(mmc) ? "SD" : "MMC",
			EXTRACT_SDMMC_MAJOR_VERSION(mmc->version),
			EXTRACT_SDMMC_MINOR_VERSION(mmc->version));

	printf("    Speed:       %dHz\n", mmc->clock);
	printf("    Bus Width:   %dbit\n", mmc->bus_width);

	printf("    Mode:        %s\n",
			mmc->selected_mode == MMC_HS_400_ES ? "HS400ES" :
			mmc->selected_mode == MMC_HS_400 ? "HS400" :
			mmc->selected_mode == MMC_HS_200 ? "HS200" :
			mmc->selected_mode == MMC_DDR_52 ? "DDR52" :
			mmc->selected_mode == MMC_HS_52 ? "HS_52" :
			mmc->selected_mode == MMC_HS ? "HS" : "DS");
}

int mmc_flash_init(int dev_num)
{
	int err;
	ulong start;
	struct mmc *mmc = NULL;
	struct sdhci_host *host = NULL;
	struct sdhci_nebula *nebula = NULL;

	start = get_timer(0);

	mmc = find_mmc_device(dev_num);
	if (mmc == NULL)
		return -EINVAL;

	host = mmc->priv;
	nebula = host->priv;
	if (nebula->is_quickboot) {
		if (mmc_restore(mmc) != 0) {
			mmc_set_cur_mode(host, INIT_MODE);
			plat_sdhci_init(host);
			mmc->has_init = 0;
			printf("mmc quick restore\n");
		}
	}

	err = mmc_init(mmc);
	if (err != 0) {
		printf("No eMMC/SD device found ! %d\n", err);
		return err;
	}

	if (!IS_SD(mmc)) {
		const char *boot_mode = "";

		emmc_set_cid(mmc->cid);
		sdhci_nebula_print_mmcinfo(mmc);
		boot_mode = mmc_get_mode_str(host);
		printf("    Boot Mode:   %s(%s), %ld ms\n", \
				nebula->is_quickboot ? "quick" : "normal", \
				boot_mode, get_timer(start) + nebula->host_init_time);
		if (nebula->is_quickboot)
			mmc_save_parameters(mmc);

		mmc_enable_rst_relwr(mmc);
	}

	return 0;
}
