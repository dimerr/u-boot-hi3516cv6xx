/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#ifndef _MMC_IMPORT_H_
#define _MMC_IMPORT_H_

#include <mmc.h>

/* export by drivers/mmc */
int mmc_select_mode(struct mmc *mmc, enum bus_mode mode);
int mmc_startup_v4(struct mmc *mmc);
int mmc_get_capabilities(struct mmc *mmc);
int mmc_select_mode_and_width(struct mmc *mmc, uint card_caps);
int mmc_send_status(struct mmc *mmc, unsigned int *status);

#endif
