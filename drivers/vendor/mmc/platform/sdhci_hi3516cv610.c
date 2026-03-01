/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#include <mmc.h>
#include <sdhci.h>
#include <linux/delay.h>

#include "sdhci_nebula.h"
#include "nebula_quick.h"
#include "nebula_platform.h"
#include "platform_priv.h"
#include "platform_timing.h"

/* CRG register */
#define CRG_MMC_BASE         (CRG_REG_BASE + 0x35c0)
#define CRG_MMC_OFFSET       0x100

/* eMMC io reg */
#define REG_MMC_IO_CFG_BASE  0x10260000
#define REG_MMC_CLK_IO       0x34
#define REG_MMC_CMD_IO       0x4c
#define REG_MMC_D0_IO        0x2c
#define REG_MMC_D1_IO        0x38
#define REG_MMC_D2_IO        0x30
#define REG_MMC_D3_IO        0x50
#define REG_MMC_D4_IO        0x44
#define REG_MMC_D5_IO        0x54
#define REG_MMC_D6_IO        0x3c
#define REG_MMC_D7_IO        0x40
#define REG_MMC_DQS_IO       0x48
#define REG_MMC_RSTN_IO      0x28

/* eMMC QFN io reg */
#define REG_MMC_CLK_IO_QFN       0x34
#define REG_MMC_CMD_IO_QFN       0x38
#define REG_MMC_D0_IO_QFN        0x30
#define REG_MMC_D1_IO_QFN        0x2c
#define REG_MMC_D2_IO_QFN        0x40
#define REG_MMC_D3_IO_QFN        0x3c

/* SDIO0 IO */
#define REG_SDIO0_CMD_IO    0x38
#define REG_SDIO0_D0_IO     0x30
#define REG_SDIO0_D1_IO     0x2c
#define REG_SDIO0_D2_IO     0x40
#define REG_SDIO0_D3_IO     0x3c
#define REG_SDIO0_CLK_IO    0x34

/* SDIO1 IO */
#define REG_SDIO1_IO_CFG_BASE  0x11130000
#define REG_SDIO1_CMD_IO    0x40
#define REG_SDIO1_D0_IO     0x48
#define REG_SDIO1_D1_IO     0x4c
#define REG_SDIO1_D2_IO     0x38
#define REG_SDIO1_D3_IO     0x3c
#define REG_SDIO1_CLK_IO    0x44

 /* IO CFG */
#define IO_DRV_BIT                  4
#define IO_DRV_MASK                 (0xF << IO_DRV_BIT)
#define io_cfg_drv_str_sel(str)     ((str) << IO_DRV_BIT)
#define io_extract_drv_str(val)     (((val) & IO_DRV_MASK) >> IO_DRV_BIT)

#define IO_CFG_PU                   BIT(8)
#define IO_CFG_PD                   BIT(9)
#define IO_CFG_SR                   BIT(10)
#define IO_CFG_DEF                  BIT(12)
#define IO_CFG_MASK                 (IO_DRV_MASK | IO_CFG_SR)

#define IO_MUX_FUNC1                0x1
#define IO_MUX_FUNC2                0x2
#define IO_MUX_MASK                 (0xF | IO_CFG_PU | IO_CFG_PD | IO_CFG_SR | IO_CFG_DEF)

#define BOOT_FLAG_MASK              (0x3 << 2)
#define BOOT_FROM_EMMC_FLAG         (0x3 << 2)
#define EMMC_BOOT_8BIT              BIT(11)
#define BOOT_EMMC_8BIT_MASK         (BOOT_FLAG_MASK | EMMC_BOOT_8BIT)

#define EMMC_OTP_STAT               0x101E001C
#define EMMC_INPUT_SEL              BIT(17)

#define REG_SAVE_HCS                0x11020300

#define MMC_QBOOT_PARAM_ADDR0       (0x11020138)
#define MMC_QBOOT_PARAM_ADDR1       (0x1102014c)

/* -------------------------------------------------------------------- */
/* bga package: mmc timing parameters */
/* -------------------------------------------------------------------- */
static u32 g_mmc_io_cfg_bga[][EMMC_IO_TYPE_NUM] = { /* CLK CMD DATA RST DQS */
	[MMC_LEGACY] = {
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_3)
	},
	[MMC_HS] = {
		io_cfg_drv_str_sel(TM_LVL_12),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_3)
	},
	[MMC_HS_200] = {
		io_cfg_drv_str_sel(TM_LVL_3),
		io_cfg_drv_str_sel(TM_LVL_9),
		io_cfg_drv_str_sel(TM_LVL_9),
		io_cfg_drv_str_sel(TM_LVL_3)
	},
	[MMC_HS_400] = {
		io_cfg_drv_str_sel(TM_LVL_0),
		io_cfg_drv_str_sel(TM_LVL_0),
		io_cfg_drv_str_sel(TM_LVL_0),
		io_cfg_drv_str_sel(TM_LVL_3),
		io_cfg_drv_str_sel(TM_LVL_0)
	}
};

static u32 g_mmc_phase_cfg_bga[][PHASE_TYPE_MAX] = { /* drv, sampl phase */
	[MMC_LEGACY]    = { PHASE_LVL_16, PHASE_LVL_0 },
	[MMC_HS]        = { PHASE_LVL_20, PHASE_LVL_4 },
	[MMC_HS_200]    = { PHASE_LVL_20, PHASE_LVL_0 },
	[MMC_HS_400]    = { PHASE_LVL_8, PHASE_LVL_0 }
};

/* -------------------------------------------------------------------- */
/* qfn package: mmc timing parameters */
/* -------------------------------------------------------------------- */
static u32 g_mmc_io_cfg_qfn[][EMMC_IO_TYPE_NUM] = { /* CLK CMD DATA RST DQS */
	[MMC_LEGACY] = {
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_3)
	},
	[MMC_HS] = {
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_3)
	},
	[MMC_HS_200] = {
		io_cfg_drv_str_sel(TM_LVL_0),
		io_cfg_drv_str_sel(TM_LVL_7),
		io_cfg_drv_str_sel(TM_LVL_7),
		io_cfg_drv_str_sel(TM_LVL_3)
	}
};

static u32 g_mmc_phase_cfg_qfn[][PHASE_TYPE_MAX] = { /* drv, sampl phase */
	[MMC_LEGACY]    = { PHASE_LVL_16, PHASE_LVL_0 },
	[MMC_HS]        = { PHASE_LVL_16, PHASE_LVL_4 },
	[MMC_HS_200]    = { PHASE_LVL_20, PHASE_LVL_0 }
};

/* -------------------------------------------------------------------- */
/* sdio0: timing parameters */
/* -------------------------------------------------------------------- */
static u32 g_sdio0_io_cfg[][SDIO_IO_TYPE_NUM] = { /* CLK CMD DATA */
	[MMC_LEGACY] = {
		io_cfg_drv_str_sel(TM_LVL_12),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_14)
	},
	[SD_HS] = {
		io_cfg_drv_str_sel(TM_LVL_10),
		io_cfg_drv_str_sel(TM_LVL_14),
		io_cfg_drv_str_sel(TM_LVL_14)
	}
};

static u32 g_sdio0_phase_cfg[][PHASE_TYPE_MAX] = { /* drv, sampl phase */
	[MMC_LEGACY]    = { PHASE_LVL_16, PHASE_LVL_0 },
	[SD_HS]         = { PHASE_LVL_20, PHASE_LVL_4 }
};

/* -------------------------------------------------------------------- */
/* sdio1: timing parameters */
/* -------------------------------------------------------------------- */
static u32 g_sdio1_io_cfg[][SDIO_IO_TYPE_NUM] = { /* CLK CMD DATA */
	[MMC_LEGACY] = {
		io_cfg_drv_str_sel(TM_LVL_3),
		io_cfg_drv_str_sel(TM_LVL_3),
		io_cfg_drv_str_sel(TM_LVL_3)
	},
	[SD_HS] = {
		io_cfg_drv_str_sel(TM_LVL_3),
		io_cfg_drv_str_sel(TM_LVL_3),
		io_cfg_drv_str_sel(TM_LVL_3)
	}
};

static u32 g_sdio1_phase_cfg[][PHASE_TYPE_MAX] = { /* drv, sampl phase */
	[MMC_LEGACY]    = { PHASE_LVL_16, PHASE_LVL_0 },
	[SD_HS]         = { PHASE_LVL_24, PHASE_LVL_4 }
};

static unsigned int g_mmc_io_ofs_bga[] = {
	REG_MMC_CLK_IO, REG_MMC_CMD_IO,
	REG_MMC_D0_IO, REG_MMC_D1_IO,
	REG_MMC_D2_IO, REG_MMC_D3_IO,
	REG_MMC_D4_IO, REG_MMC_D5_IO,
	REG_MMC_D6_IO, REG_MMC_D7_IO
};

static unsigned int g_mmc_io_ofs_qfn[] = {
	REG_MMC_CLK_IO_QFN, REG_MMC_CMD_IO_QFN,
	REG_MMC_D0_IO_QFN, REG_MMC_D1_IO_QFN,
	REG_MMC_D2_IO_QFN, REG_MMC_D3_IO_QFN
};

static unsigned int g_sdio0_io_ofs[] = {
	REG_SDIO0_CLK_IO, REG_SDIO0_CMD_IO,
	REG_SDIO0_D0_IO, REG_SDIO0_D1_IO,
	REG_SDIO0_D2_IO, REG_SDIO0_D3_IO
};

static unsigned int g_sdio1_io_ofs[] = {
	REG_SDIO1_CLK_IO, REG_SDIO1_CMD_IO,
	REG_SDIO1_D0_IO, REG_SDIO1_D1_IO,
	REG_SDIO1_D2_IO, REG_SDIO1_D3_IO
};

static inline bool priv_is_bga_package(struct sdhci_host *host)
{
	return (readl(EMMC_OTP_STAT) & EMMC_INPUT_SEL);
}

static inline bool priv_is_emmc_boot(struct sdhci_host *host)
{
	unsigned int sys_stat;

	sys_stat = readl(SYS_CTRL_REG_BASE + REG_SYSSTAT);

	return ((sys_stat & BOOT_FLAG_MASK) == BOOT_FROM_EMMC_FLAG);
}

u32 plat_get_bus_width(struct sdhci_host *host)
{
	unsigned int sys_stat;
	unsigned int bus_width = MMC_BUS_WIDTH_4BIT;

	if (!priv_is_bga_package(host))
		return MMC_BUS_WIDTH_4BIT;

	sys_stat = readl(SYS_CTRL_REG_BASE + REG_SYSSTAT);
	if ((sys_stat & BOOT_EMMC_8BIT_MASK) == BOOT_EMMC_8BIT_MASK)
		bus_width = MMC_BUS_WIDTH_8BIT;

	return bus_width;
}

void plat_power_cycle(struct sdhci_host *host)
{
	__maybe_unused u8 ctrl;

	if ((host->index == MMC_PORT_0) && priv_is_emmc_boot(host)) {
		return;
	}

	ctrl = sdhci_readb(host, SDHCI_POWER_CONTROL);
	sdhci_writeb(host, 0, SDHCI_POWER_CONTROL);
	mdelay(100); // 100ms delay for power cycle
	ctrl |= SDHCI_POWER_ON;
	sdhci_writeb(host, ctrl, SDHCI_POWER_CONTROL);
}

static void priv_set_drv_str(struct sdhci_host *host, u32 offset, u32 drv_str)
{
	unsigned int reg;
	uintptr_t base_addr = REG_MMC_IO_CFG_BASE + offset;

	if (host->index == MMC_PORT_0)
		base_addr = REG_MMC_IO_CFG_BASE + offset;
	else
		base_addr = REG_SDIO1_IO_CFG_BASE + offset;

	reg = readl(base_addr);
	reg &= ~IO_CFG_MASK;
	reg |= drv_str;
	writel(reg, base_addr);
}

static enum bus_mode priv_mmc_transfer_mode(struct sdhci_host *host)
{
	enum bus_mode mode = host->mmc->selected_mode;

	if ((host->index == MMC_PORT_0) && priv_is_emmc_boot(host)) {
		/* HS52 and HS mode share one timing parameter */
		mode = (mode == MMC_HS_52) ? MMC_HS : mode;
		/* MMC_HS_400_ES and MMC_HS_400 mode share one timing parameter */
		mode = (mode == MMC_HS_400_ES) ? MMC_HS_400 : mode;
	} else {
		/* SD card only support SD2.0 */
		mode = (mode > SD_HS) ? MMC_LEGACY : mode;
	}

	return mode;
}

void plat_set_ioconfig(struct sdhci_host *host)
{
	unsigned int i, bus_width;
	u32 *offset = NULL;
	u32 *timing = NULL;
	enum bus_mode mode;

	bus_width = host->mmc->bus_width;
	mode = priv_mmc_transfer_mode(host);

	if ((host->index == MMC_PORT_0) && priv_is_emmc_boot(host)) {
		offset = priv_is_bga_package(host) ? g_mmc_io_ofs_bga : g_mmc_io_ofs_qfn;
		timing = priv_is_bga_package(host) ? g_mmc_io_cfg_bga[mode] : g_mmc_io_cfg_qfn[mode];

		priv_set_drv_str(host, REG_MMC_RSTN_IO, timing[MMC_IO_TYPE_RST]);

		if (mode == MMC_HS_400)
			priv_set_drv_str(host, REG_MMC_DQS_IO, timing[MMC_IO_TYPE_DS]);
	} else if (host->index == MMC_PORT_0) { /* sdio0 */
		offset = g_sdio0_io_ofs;
		timing = g_sdio0_io_cfg[mode];
	} else { /* sdio1 */
		offset = g_sdio1_io_ofs;
		timing = g_sdio1_io_cfg[mode];
	}

	priv_set_drv_str(host, offset[MMC_IO_TYPE_CLK], timing[MMC_IO_TYPE_CLK]);
	priv_set_drv_str(host, offset[MMC_IO_TYPE_CMD], timing[MMC_IO_TYPE_CMD]);
	for (i = MMC_IO_TYPE_DATA; i < (bus_width + MMC_IO_TYPE_DATA); i++)
		priv_set_drv_str(host, offset[i], timing[MMC_IO_TYPE_DATA]);
}

void plat_set_drv_sample_phase(struct sdhci_host *host)
{
	unsigned int drv_phase = 0;
	unsigned int sample_phase;
	enum bus_mode mode;

	mode = priv_mmc_transfer_mode(host);

	if ((host->index == MMC_PORT_0) && priv_is_emmc_boot(host)) {
		if (priv_is_bga_package(host)) {
			sample_phase = g_mmc_phase_cfg_bga[mode][PHASE_TYPE_SAMPLE];
			drv_phase = g_mmc_phase_cfg_bga[mode][PHASE_TYPE_DRIVER];
		} else {
			sample_phase = g_mmc_phase_cfg_qfn[mode][PHASE_TYPE_SAMPLE];
			drv_phase = g_mmc_phase_cfg_qfn[mode][PHASE_TYPE_DRIVER];
		}
	} else if (host->index == MMC_PORT_0) { /* sdio0 */
		sample_phase = g_sdio0_phase_cfg[mode][PHASE_TYPE_SAMPLE];
		drv_phase = g_sdio0_phase_cfg[mode][PHASE_TYPE_DRIVER];
	} else { /* sdio1 */
		sample_phase = g_sdio1_phase_cfg[mode][PHASE_TYPE_SAMPLE];
		drv_phase = g_sdio1_phase_cfg[mode][PHASE_TYPE_DRIVER];
	}

	plat_set_tx_drv_phase(host, drv_phase);
	plat_enbale_rx_sample_tuning(host);

	if (mode >= UHS_SDR50) {
		struct sdhci_nebula *nebula = host->priv;
		sample_phase = nebula->tuning_phase;
	}

	plat_set_rx_sample_phase(host, sample_phase);

	udelay(25); /* delay 25 us */
}

void plat_clk_set_rate(struct sdhci_host *host, unsigned int clk)
{
	unsigned int sel, reg;
	struct sdhci_nebula *nebula = host->priv;
	static unsigned int clk_mux[] = {
		CLK_400K, CLK_25M, CLK_50M, CLK_100M, CLK_150M, CLK_200M };

	for (sel = ARRAY_SIZE(clk_mux) - 1; sel > 0; sel--) {
		if (clk >= clk_mux[sel])
			break;
	}

	reg = readl(nebula->crg_base);
	reg &= ~COMM_CLK_SEL_MASK;
	reg |= (sel & COMM_CLK_SEL_WIDTH) << COMM_CLK_SEL_SHIFT;
	writel(reg, nebula->crg_base);
}

static void priv_set_emmc_pin_mux(unsigned int offset, unsigned int pin_mux)
{
	unsigned int reg;
	const uintptr_t io_addr = REG_MMC_IO_CFG_BASE + offset;

	reg = readl(io_addr);
	reg &= ~IO_MUX_MASK;
	reg |= pin_mux;
	writel(reg, io_addr);
}

static void priv_emmc_pin_mux_config(struct sdhci_host *host)
{
	unsigned int bus_width, i, pinmux_func;
	u32 *offset = priv_is_bga_package(host) ? g_mmc_io_ofs_bga : g_mmc_io_ofs_qfn;

	bus_width = plat_get_bus_width(host);

	priv_set_emmc_pin_mux(offset[MMC_IO_TYPE_CLK], IO_MUX_FUNC1 | IO_CFG_PD | IO_CFG_DEF);

	pinmux_func = priv_is_bga_package(host) ? IO_MUX_FUNC2 : IO_MUX_FUNC1;
	priv_set_emmc_pin_mux(offset[MMC_IO_TYPE_CMD], pinmux_func | IO_CFG_PU | IO_CFG_DEF);

	for (i = MMC_IO_TYPE_DATA; i < (bus_width + MMC_IO_TYPE_DATA); i++)
		priv_set_emmc_pin_mux(offset[i], pinmux_func | IO_CFG_PU | IO_CFG_DEF);

	priv_set_emmc_pin_mux(REG_MMC_RSTN_IO, IO_MUX_FUNC2 | IO_CFG_PU | IO_CFG_DEF);

	if (bus_width == MMC_BUS_WIDTH_8BIT)
		priv_set_emmc_pin_mux(REG_MMC_DQS_IO, IO_MUX_FUNC2 | IO_CFG_PD | IO_CFG_DEF);
}

void plat_sdhci_init(struct sdhci_host *host)
{
	struct sdhci_nebula *nebula = host->priv;

	if ((host->index == MMC_PORT_0) && priv_is_emmc_boot(host)) {
		printf("ocr from bootrom 0x%08x\n", nebula->ocr_from_bootrom);
		writel(0, REG_SAVE_HCS); /* clear flag reg */
		priv_emmc_pin_mux_config(host);
	}

	plat_mmc_crg_init(host);

	mdelay(10); /* delay 10 ms */
}

void plat_host_pre_init(struct sdhci_host *host)
{
	struct sdhci_nebula *nebula = host->priv;

	nebula->max_clk = CLK_150M;
	nebula->crg_base = (void *)(uintptr_t)(CRG_MMC_BASE + host->index * CRG_MMC_OFFSET);
#ifdef CONFIG_MMC_QUICKBOOT
	if (host->index == MMC_PORT_0) {
		nebula->qboot_param0_addr = (void *)MMC_QBOOT_PARAM_ADDR0;
		nebula->qboot_param1_addr = (void *)MMC_QBOOT_PARAM_ADDR1;
		nebula->ocr_from_bootrom = readl(REG_SAVE_HCS);
		/* burn image should disabled quickboot, no emmc boot would be SD card media */
		if ((readl(REG_START_FLAG) == START_MAGIC) || !priv_is_emmc_boot(host))
			mmc_disable_fast_boot(host);
	}
#endif
}

#ifdef CONFIG_MMC_QUICKBOOT
void plat_get_io_drv_str_info(struct sdhci_host *host)
{
	emmc_param_gen0_u param0;
	const uintptr_t io_addr = REG_MMC_IO_CFG_BASE;
	struct sdhci_nebula *nebula = host->priv;
	u32 *offset = priv_is_bga_package(host) ? g_mmc_io_ofs_bga : g_mmc_io_ofs_qfn;

	param0.u32 = readl(nebula->qboot_param0_addr);

	param0.bits.emmc_cmd_drv = io_extract_drv_str(readl(io_addr + offset[MMC_IO_TYPE_CLK]));
	param0.bits.emmc_cmd_sl = !!(readl(io_addr + offset[MMC_IO_TYPE_CLK]) & IO_CFG_SR);
	param0.bits.emmc_clk_drv = io_extract_drv_str(readl(io_addr + offset[MMC_IO_TYPE_CMD]));
	param0.bits.emmc_clk_sl = !!(readl(io_addr + offset[MMC_IO_TYPE_CMD]) & IO_CFG_SR);
	param0.bits.emmc_data_drv = io_extract_drv_str(readl(io_addr + offset[MMC_IO_TYPE_DATA]));
	param0.bits.emmc_data_sl = !!(readl(io_addr + offset[MMC_IO_TYPE_DATA]) & IO_CFG_SR);

	writel(param0.u32, nebula->qboot_param0_addr);
}
#endif
