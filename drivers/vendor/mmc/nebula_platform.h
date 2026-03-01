/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#ifndef _NEBULA_PLATFORM_H
#define _NEBULA_PLATFORM_H

/* export by platform */
void plat_sdhci_init(struct sdhci_host *host);
void plat_sdhci_extra_init(struct sdhci_host *host);
void plat_phy_init(struct sdhci_host *host);
void plat_prepare_tuning(struct sdhci_host *host);
void plat_disable_card_clk(struct sdhci_host *host);
void plat_enable_card_clk(struct sdhci_host *host);
void plat_disable_internal_clk(struct sdhci_host *host);
void plat_enable_internal_clk(struct sdhci_host *host);
void plat_enbale_rx_sample_tuning(struct sdhci_host *host);
u32 plat_get_rx_sample_phase(struct sdhci_host *host);
void plat_set_rx_sample_phase(struct sdhci_host *host, u32 phase);
void plat_sdhci_set_clock(struct sdhci_host *host, unsigned int clk);
void plat_power_cycle(struct sdhci_host *host);
void plat_set_enhanced_strobe(struct sdhci_host *host, bool enable);
u32 plat_get_bus_width(struct sdhci_host *host);
u32 plat_get_max_clk(struct sdhci_host *host);
void plat_wait_sample_dll_slave_ready(struct sdhci_host *host);
u32 plat_get_clk(struct sdhci_host *host);
u32 plat_get_clk_phase(struct sdhci_host *host);
void plat_get_io_drv_str_info(struct sdhci_host *host);
void plat_host_pre_init(struct sdhci_host *host);

#endif /* _NEBULA_PLATFORM_H */
