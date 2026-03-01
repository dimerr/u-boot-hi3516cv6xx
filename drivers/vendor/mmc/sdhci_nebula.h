/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#ifndef _SDHCI_NEBULA_H
#define _SDHCI_NEBULA_H

typedef enum {
	MMC_BUS_WIDTH_1BIT = 1,
	MMC_BUS_WIDTH_4BIT = 4,
	MMC_BUS_WIDTH_8BIT = 8
} mmc_bus_width_u;

typedef enum {
	MMC_IO_TYPE_CLK,
	MMC_IO_TYPE_CMD,
	MMC_IO_TYPE_DATA,
	MMC_IO_TYPE_RST,
	MMC_IO_TYPE_DS,
} mmc_io_type;

typedef enum {
	MMC_TYPE_MMC,
	MMC_TYPE_SD,
	MMC_TYPE_MAX,
} mmc_type;

typedef enum {
	MMC_PORT_0,
	MMC_PORT_1,
	MMC_PORT_2,
	MMC_PORT_MAX,
} mmc_port;

typedef enum {
	PHASE_TYPE_DRIVER,
	PHASE_TYPE_SAMPLE,
	PHASE_TYPE_MAX,
} phase_type;

struct sdhci_nebula {
	bool is_tuning;
	bool is_quickboot;
	void *crg_base;
	void *qboot_param0_addr;
	void *qboot_param1_addr;
	u32 priv_cap;
	u32 priv_quirk;
#define NEBULA_QUIRK_FPGA                 (1 << 0) /* FPGA board */
#define NEBULA_QUIRK_SAMPLE_TURNING       (1 << 1) /* for not support edge turning */
#define NEBULA_QUIRK_RESET_AFTER_SET_XFER_MODE      (1 << 2) /* Workaround for STAR */
	u32 type;
	u32 tuning_phase;
	ulong host_init_time;
	u32 ocr_from_bootrom;
	u32 max_clk;
};

#ifdef CONFIG_DM_MMC
struct ext_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
	struct sdhci_nebula nebula;
};
#endif

#define WAIT_MAX_TIMES      20

#define EMMC_IO_TYPE_NUM    5
#define SDIO_IO_TYPE_NUM    3

#define MIN_FREQ	            400000
#define NOT_FOUND	            (-1)
#define PHASE_SCALE	            32
#define EDGE_TUNING_PHASE_STEP	4

/* avaliable frequency */
#define CLK_400K                400000
#define CLK_25M                 25000000
#define CLK_50M                 50000000
#define CLK_100M                100000000
#define CLK_150M                150000000
#define CLK_168M                168000000
#define CLK_196M                196000000
#define CLK_200M                200000000

/* Huanglong extended host controller registers. */
#define SDHCI_CTRL_HOST_VER4_ENABLE	    0x1000
#define  SDHCI_CLOCK_PLL_EN	            0x0008
#define  SDHCI_CTRL_64BIT_ADDR	        0x2000
#define  SDHCI_CAN_DO_ADMA3	            0x08000000

#define SDHCI_AXI_MBIIU_CTRL	        0x510
#define  SDHCI_GM_WR_OSRC_LMT	        0x03000000
#define  SDHCI_GM_RD_OSRC_LMT	        0x00030000
#define  SDHCI_UNDEFL_INCR_EN	        0x00000001

#define SDHCI_EMMC_CTRL		            0x52C
#define  SDHCI_CARD_IS_EMMC	            0x1
#define  SDHCI_ENH_STROBE_EN	        0x100

#define SDHCI_EMMC_HW_RESET	            0x534

#define SDHCI_AT_CTRL		            0x540
#define  SDHCI_SAMPLE_EN	            0x10

#define SDHCI_AT_STAT		            0x544
#define  SDHCI_PHASE_SEL_MASK	        0x000000FF

#define SDHCI_MULTI_CYCLE	            0x54C
#define  SDHCI_FOUND_EDGE	            BIT(11)
#define  SDHCI_EDGE_DETECT_EN	        BIT(8)
#define  SDHCI_DOUT_EN_F_EDGE	        BIT(6)
#define  SDHCI_DATA_DLY_EN	            BIT(3)
#define  SDHCI_CMD_DLY_EN	            BIT(2)

void sdhci_nebula_print_mmcinfo(struct mmc *mmc);
void sdhci_nebula_reset(struct sdhci_host *host, u8 mask);

#endif /* _SDHCI_NEBULA_H */
