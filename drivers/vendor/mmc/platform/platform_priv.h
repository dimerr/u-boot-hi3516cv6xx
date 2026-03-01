/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#ifndef _MMC_PLATFORM_COMM_H_
#define _MMC_PLATFORM_COMM_H_

#define COMM_SRST_REQ                (BIT(16) | BIT(17) | BIT(18))
#define COMM_CLK_EN                  BIT(0) | BIT(1)
#define COMM_CLK_SEL_SHIFT           24
#define COMM_CLK_SEL_WIDTH           0x7
#define COMM_CLK_SEL_MASK            (COMM_CLK_SEL_WIDTH << COMM_CLK_SEL_SHIFT)

#define COMM_MMC_P4_DLL_OFFSET       0x4
#define COMM_P4_DLL_SRST_REQ         BIT(1)

#define COMM_MMC_DRV_DLL_OFFSET      0x8
#define COMM_DRV_PHASE_SEL_SHIFT     15
#define COMM_DRV_PHASE_SEL_WIDTH     0x1F
#define COMM_DRV_PHASE_SEL_MASK      (COMM_DRV_PHASE_SEL_WIDTH << COMM_DRV_PHASE_SEL_SHIFT)

#define COMM_MMC_STAT_OFFSET         0x18
#define COMM_SAM_DLL_READY           BIT(12)
#define COMM_DS_DLL_READY            BIT(10)
#define COMM_P4_DLL_LOCKED           BIT(9)

#define PWR_CTRL_BY_MISC             BIT(0)
#define PWR_CTRL_BY_MISC_EN          BIT(4)
#define COMM_PWR_CRTL_EN             (PWR_CTRL_BY_MISC | PWR_CTRL_BY_MISC_EN)
#define IO_MODE_SEL_1V8              BIT(1)
#define PWRSW_SEL_1V8                BIT(5)
#define COMM_PWR_CRTL_1V8            (IO_MODE_SEL_1V8 | PWRSW_SEL_1V8)

#define COMM_WAIT_TIMEOUT_US         1000

void plat_p4_dll_reset_assert(struct sdhci_host *host);
void plat_p4_dll_reset_deassert(struct sdhci_host *host);
void plat_wait_p4_dll_lock(struct sdhci_host *host);
void plat_wait_ds_dll_ready(struct sdhci_host *host);
void plat_mmc_crg_init(struct sdhci_host *host);
void plat_set_tx_drv_phase(struct sdhci_host *host, unsigned int phase);
void plat_clk_set_rate(struct sdhci_host *host, unsigned int clk);
void plat_set_drv_sample_phase(struct sdhci_host *host);
void plat_set_ioconfig(struct sdhci_host *host);

#endif
