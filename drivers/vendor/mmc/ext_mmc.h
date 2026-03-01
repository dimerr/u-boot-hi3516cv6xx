/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#ifndef _EXT_MMC_H_
#define _EXT_MMC_H_

/*
 * EXT_CSD field definitions
 */
#define EXT_CSD_WR_REL_PARAM_EN     BIT(1)
#define EXT_CSD_WR_REL_PARAM_HCR    BIT(0)

#define EXT_CSD_RST_N_EN_MASK       0x3
#define EXT_CSD_RST_N_ENABLED       BIT(0) /* RST_n is enabled on card */

#define EXT_CSD_WR_REL_VALUE        (0x1f)

void emmc_set_cid(unsigned int *cid);
unsigned int *emmc_get_cid(void);
int mmc_abort_tuning(struct mmc *mmc, u32 opcode);

#endif