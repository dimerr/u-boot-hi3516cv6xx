/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#include <common.h>
#include <sdhci.h>
#include <linux/delay.h>

#include "sdhci_nebula.h"
#include "platform_priv.h"

__weak void plat_prepare_tuning(struct sdhci_host *host)
{
}

__weak void plat_phy_init(struct sdhci_host *host)
{
}

__weak u32 plat_get_bus_width(struct sdhci_host *host)
{
	return MMC_BUS_WIDTH_8BIT;
}

void plat_sdhci_extra_init(struct sdhci_host *host)
{
	u32 reg;

	reg = sdhci_readl(host, SDHCI_AXI_MBIIU_CTRL);
	reg &= ~SDHCI_UNDEFL_INCR_EN;
	sdhci_writel(host, reg, SDHCI_AXI_MBIIU_CTRL);

	if (host->index == MMC_PORT_0) {
		reg = sdhci_readl(host, SDHCI_EMMC_CTRL);
		reg |= SDHCI_CARD_IS_EMMC;
		sdhci_writel(host, reg, SDHCI_EMMC_CTRL);
	}
}

__weak void plat_power_cycle(struct sdhci_host *host)
{
}

void plat_set_enhanced_strobe(struct sdhci_host *host, bool enable)
{
	u16 ctrl;

	ctrl = sdhci_readw(host, SDHCI_EMMC_CTRL);
	if (enable)
		ctrl |= SDHCI_ENH_STROBE_EN;
	else
		ctrl &= ~SDHCI_ENH_STROBE_EN;

	sdhci_writew(host, ctrl, SDHCI_EMMC_CTRL);
}

void plat_enbale_rx_sample_tuning(struct sdhci_host *host)
{
	u32 reg;

	reg = sdhci_readl(host, SDHCI_AT_CTRL);
	reg |= SDHCI_SAMPLE_EN;
	sdhci_writel(host, reg, SDHCI_AT_CTRL);
}

void plat_set_rx_sample_phase(struct sdhci_host *host, u32 phase)
{
	u32 reg;

	reg = sdhci_readl(host, SDHCI_AT_STAT);
	reg &= ~SDHCI_PHASE_SEL_MASK;
	reg |= phase;
	sdhci_writel(host, reg, SDHCI_AT_STAT);
}

u32 __maybe_unused plat_get_rx_sample_phase(struct sdhci_host *host)
{
	return sdhci_readl(host, SDHCI_AT_STAT) & SDHCI_PHASE_SEL_MASK;
}

void plat_set_tx_drv_phase(struct sdhci_host *host, u32 phase)
{
	unsigned int reg;
	struct sdhci_nebula *nebula = host->priv;

	reg = readl(nebula->crg_base + COMM_MMC_DRV_DLL_OFFSET);
	reg &= ~COMM_DRV_PHASE_SEL_MASK;
	reg |= (phase & COMM_DRV_PHASE_SEL_WIDTH) << COMM_DRV_PHASE_SEL_SHIFT;
	writel(reg, nebula->crg_base + COMM_MMC_DRV_DLL_OFFSET);
}

void plat_disable_card_clk(struct sdhci_host *host)
{
	u16 clk;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

void plat_enable_card_clk(struct sdhci_host *host)
{
	u16 clk;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

void plat_disable_internal_clk(struct sdhci_host *host)
{
	u16 clk;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

void plat_enable_internal_clk(struct sdhci_host *host)
{
	u16 clk, timeout;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	/* Wait max 20 ms */
	timeout = 20;
	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	while ((clk & SDHCI_CLOCK_INT_STABLE) == 0) {
		if (timeout == 0) {
			printf("%s: Internal clock never stabilised.\n", __func__);
			return;
		}
		timeout--;
		udelay(COMM_WAIT_TIMEOUT_US);
		clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	}
}

void plat_p4_dll_reset_assert(struct sdhci_host *host)
{
	unsigned int reg;
	struct sdhci_nebula *nebula = host->priv;

	reg = readl(nebula->crg_base + COMM_MMC_P4_DLL_OFFSET);
	reg |= COMM_P4_DLL_SRST_REQ;
	writel(reg, nebula->crg_base + COMM_MMC_P4_DLL_OFFSET);
}

void plat_p4_dll_reset_deassert(struct sdhci_host *host)
{
	unsigned int reg;
	struct sdhci_nebula *nebula = host->priv;

	reg = readl(nebula->crg_base + COMM_MMC_P4_DLL_OFFSET);
	reg &= ~COMM_P4_DLL_SRST_REQ;
	writel(reg, nebula->crg_base + COMM_MMC_P4_DLL_OFFSET);
}

void plat_wait_p4_dll_lock(struct sdhci_host *host)
{
	unsigned int reg;
	unsigned int timeout = WAIT_MAX_TIMES;
	struct sdhci_nebula *nebula = host->priv;

	do {
		reg = readl(nebula->crg_base + COMM_MMC_STAT_OFFSET);
		if ((reg & COMM_P4_DLL_LOCKED) == COMM_P4_DLL_LOCKED)
			return;

		udelay(COMM_WAIT_TIMEOUT_US);
		timeout--;
	} while (timeout > 0);

	printf("sdhci: P4 DLL master not locked.\n");
}

void plat_wait_ds_dll_ready(struct sdhci_host *host)
{
	unsigned int reg;
	unsigned int timeout = WAIT_MAX_TIMES;
	struct sdhci_nebula *nebula = host->priv;

	do {
		reg = readl(nebula->crg_base + COMM_MMC_STAT_OFFSET);
		if ((reg & COMM_DS_DLL_READY) == COMM_DS_DLL_READY)
			return;

		udelay(COMM_WAIT_TIMEOUT_US);
		timeout--;
	} while (timeout > 0);

	printf("sdhci: DS DLL slave not ready.\n");
}

__weak void plat_wait_sample_dll_slave_ready(struct sdhci_host *host)
{
	unsigned int reg;
	unsigned int timeout = WAIT_MAX_TIMES;
	struct sdhci_nebula *nebula = host->priv;

	do {
		reg = readl(nebula->crg_base + COMM_MMC_STAT_OFFSET);
		if ((reg & COMM_SAM_DLL_READY) == COMM_SAM_DLL_READY)
			return;

		udelay(COMM_WAIT_TIMEOUT_US);
		timeout--;
	} while (timeout > 0);

	printf("sdhci: SAMPL DLL slave not ready.\n");
}

void plat_mmc_crg_init(struct sdhci_host *host)
{
	unsigned int reg;
	struct sdhci_nebula *nebula = host->priv;

	/* eMMC clk enable */
	reg = readl(nebula->crg_base);
	reg |= COMM_CLK_EN;
	writel(reg, nebula->crg_base);

	/* eMMC reset assert */
	reg = readl(nebula->crg_base);
	reg |= COMM_SRST_REQ;
	writel(reg, nebula->crg_base);
	udelay(25); /* delay 25 us */

	/* eMMC reset deassert */
	reg = readl(nebula->crg_base);
	reg &= ~COMM_SRST_REQ;
	writel(reg, nebula->crg_base);
	udelay(1);
}

__weak void plat_sdhci_set_clock(struct sdhci_host *host, unsigned int clk)
{
	enum bus_mode mode;

	plat_disable_card_clk(host);
	udelay(25); /* delay 25 us */
	plat_disable_internal_clk(host);

	if (clk == 0)
		return;

	plat_set_ioconfig(host);
	plat_clk_set_rate(host, clk);
	plat_set_drv_sample_phase(host);

	udelay(5); /* delay 5 us */

	plat_enable_internal_clk(host);

	mode = host->mmc->selected_mode;
	if ((mode == MMC_HS_400) || (mode == MMC_HS_400_ES) ||
		(mode == MMC_HS_200)) {
		plat_p4_dll_reset_assert(host);
		plat_p4_dll_reset_deassert(host);
		plat_wait_p4_dll_lock(host);
		plat_wait_sample_dll_slave_ready(host);
	}

	if ((mode == MMC_HS_400) || (mode == MMC_HS_400_ES))
		plat_wait_ds_dll_ready(host);

	plat_enable_card_clk(host);
	udelay(75); /* delay 75 us */
}

#ifdef CONFIG_MMC_QUICKBOOT
__weak u32 plat_get_clk(struct sdhci_host *host)
{
	struct sdhci_nebula *nebula = host->priv;

	return (readl(nebula->crg_base) & COMM_CLK_SEL_MASK) >> COMM_CLK_SEL_SHIFT;
}

__weak u32 plat_get_clk_phase(struct sdhci_host *host)
{
	unsigned int reg;
	struct sdhci_nebula *nebula = host->priv;

	reg = readl(nebula->crg_base + COMM_MMC_DRV_DLL_OFFSET);
	return (reg & COMM_DRV_PHASE_SEL_MASK) >> COMM_DRV_PHASE_SEL_SHIFT;
}
#endif

