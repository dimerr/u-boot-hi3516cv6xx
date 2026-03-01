/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#include <common.h>
#include <errno.h>
#include <mmc.h>
#include <sdhci.h>
#include <linux/delay.h>

#include "sdhci_nebula.h"

static void sdhci_dump_dma(struct sdhci_host *host)
{
	if ((host->flags & USE_ADMA64) != 0) {
		printf("ADMA Err:  0x%08x | ADMA Ptr: 0x%08x%08x\n",
				sdhci_readl(host, SDHCI_ADMA_ERROR),
				sdhci_readl(host, SDHCI_ADMA_ADDRESS_HI),
				sdhci_readl(host, SDHCI_ADMA_ADDRESS));
	} else if ((host->flags & USE_ADMA) != 0) {
		printf("ADMA Err:  0x%08x | ADMA Ptr: 0x%08x\n",
				sdhci_readl(host, SDHCI_ADMA_ERROR),
				sdhci_readl(host, SDHCI_ADMA_ADDRESS));
	}
}

void sdhci_dumpregs(struct sdhci_host *host)
{
	printf("=========== REGISTER DUMP (mmc%d)===========\n", host->index);

	printf("Sys addr: 0x%08x | Version:  0x%08x\n",
			sdhci_readl(host, SDHCI_DMA_ADDRESS),
			sdhci_readw(host, SDHCI_HOST_VERSION));
	printf("Blk size: 0x%08x | Blk cnt:  0x%08x\n",
			sdhci_readw(host, SDHCI_BLOCK_SIZE),
			sdhci_readw(host, SDHCI_BLOCK_COUNT));
	printf("Argument: 0x%08x | Trn mode: 0x%08x\n",
			sdhci_readl(host, SDHCI_ARGUMENT),
			sdhci_readw(host, SDHCI_TRANSFER_MODE));
	printf("Present:  0x%08x | Host ctl: 0x%08x\n",
			sdhci_readl(host, SDHCI_PRESENT_STATE),
			sdhci_readb(host, SDHCI_HOST_CONTROL));
	printf("Power:    0x%08x | Blk gap:  0x%08x\n",
			sdhci_readb(host, SDHCI_POWER_CONTROL),
			sdhci_readb(host, SDHCI_BLOCK_GAP_CONTROL));
	printf("Wake-up:  0x%08x | Clock:    0x%08x\n",
			sdhci_readb(host, SDHCI_WAKE_UP_CONTROL),
			sdhci_readw(host, SDHCI_CLOCK_CONTROL));
	printf("Timeout:  0x%08x | Int stat: 0x%08x\n",
			sdhci_readb(host, SDHCI_TIMEOUT_CONTROL),
			sdhci_readl(host, SDHCI_INT_STATUS));
	printf("Int enab: 0x%08x | Sig enab: 0x%08x\n",
			sdhci_readl(host, SDHCI_INT_ENABLE),
			sdhci_readl(host, SDHCI_SIGNAL_ENABLE));
	printf("ACMD stat:0x%08x | Slot int: 0x%08x\n",
			sdhci_readw(host, SDHCI_ACMD12_ERR),
			sdhci_readw(host, SDHCI_SLOT_INT_STATUS));
	printf("Caps:     0x%08x | Caps_1:   0x%08x\n",
			sdhci_readl(host, SDHCI_CAPABILITIES),
			sdhci_readl(host, SDHCI_CAPABILITIES_1));
	printf("Cmd:      0x%08x | Max curr: 0x%08x\n",
			sdhci_readw(host, SDHCI_COMMAND),
			sdhci_readl(host, SDHCI_MAX_CURRENT));
	printf("Resp[0]:   0x%08x | Resp[1]:  0x%08x\n",
			sdhci_readl(host, SDHCI_RESPONSE),
			sdhci_readl(host, SDHCI_RESPONSE + 4)); /* 4: respone 1 offset */
	printf("Resp[2]:   0x%08x | Resp[3]:  0x%08x\n",
			sdhci_readl(host, SDHCI_RESPONSE + 8), /* 8: respone 2 offset */
			sdhci_readl(host, SDHCI_RESPONSE + 12)); /* 12: respone 3 offset */
	printf("Host ctl2:0x%08x\n",
			sdhci_readw(host, SDHCI_HOST_CONTROL2));

	sdhci_dump_dma(host);

	printf("===========================================\n");
}

void sdhci_nebula_dumpregs(struct sdhci_host *host)
{
	struct sdhci_nebula *nebula = host->priv;

	if (!nebula->is_tuning)
		sdhci_dumpregs(host);
}

void sdhci_nebula_config_mode(struct sdhci_host *host, u32 mode)
{
	struct sdhci_nebula *nebula = host->priv;

	if ((nebula->priv_quirk & NEBULA_QUIRK_RESET_AFTER_SET_XFER_MODE) != 0) {
		/* Workaround for STAR: 9001166728 */
		sdhci_writel(host, 0, SDHCI_INT_ENABLE);
		sdhci_writew(host, mode, SDHCI_TRANSFER_MODE);
		udelay(20); /* delay 20us */
		sdhci_nebula_reset(host, SDHCI_RESET_DATA);
		udelay(1); /* delay 1us */

		sdhci_writel(host, SDHCI_INT_DATA_MASK | SDHCI_INT_CMD_MASK, SDHCI_INT_ENABLE);

		sdhci_writel(host, (u32)host->start_addr, SDHCI_DMA_ADDRESS);
	} else {
		sdhci_writew(host, mode, SDHCI_TRANSFER_MODE);
	}
}

#if CONFIG_IS_ENABLED(MMC_SDHCI_ADMA)
struct sdhci_adma_desc *sdhci_adma_desc(struct sdhci_adma_desc *desc,
	dma_addr_t addr, u16 len, bool end)
{
	u16 split_len;

	if (((addr & (SZ_128M - 1)) + len) > SZ_128M) {
		split_len = SZ_128M - (addr & (SZ_128M - 1));
		_sdhci_adma_desc(desc, addr, split_len, false);
		addr += split_len;
		len -= split_len;
		desc++;
	}

	_sdhci_adma_desc(desc, addr, len, end);
	return desc;
}
#endif

#if !defined(CONFIG_DM_MMC)
#ifdef MMC_SUPPORTS_TUNING
int sdhci_execute_tuning(struct mmc *mmc, uint opcode)
{
	struct sdhci_host *host = mmc->priv;

	if ((host->ops != NULL) && (host->ops->platform_execute_tuning != NULL)) {
		return host->ops->platform_execute_tuning(mmc, opcode);
	}
	return 0;
}
#endif

void sdhci_host_power_cycle(struct mmc *mmc)
{
	struct sdhci_host *host = mmc->priv;

	if ((host->ops != NULL) && (host->ops->power_cycle != NULL))
		host->ops->power_cycle(host);
}

void sdhci_enhanced_strobe(struct mmc *mmc)
{
	struct sdhci_host *host = mmc->priv;

	if ((host->ops != NULL) && (host->ops->set_enhanced_strobe != NULL))
		host->ops->set_enhanced_strobe(host);
}

int sdhci_wait_dat0(struct mmc *mmc, int state,
			   int timeout_us)
{
	u32 reg;
	struct sdhci_host *host = mmc->priv;
	unsigned long timeout = timer_get_us() + timeout_us;

	/*
	 * readx_poll_timeout is unsuitable because sdhci_readl accepts two arguments
	 */
	do {
		reg = sdhci_readl(host, SDHCI_PRESENT_STATE);
		if (!!(reg & SDHCI_DATA_0_LVL_MASK) == !!state)
			return 0;
	} while ((timeout_us != 0) || !time_after(timer_get_us(), timeout));

	return -ETIMEDOUT;
}

#endif
