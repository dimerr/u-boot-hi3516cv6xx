/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2023. All rights reserved.
 *
 */

#include <mmc.h>
#include <securec.h>

#define EMMC_CID_SIZE 4
unsigned int g_emmc_cid[EMMC_CID_SIZE];

int mmc_abort_tuning(struct mmc *mmc, u32 opcode)
{
	int ret;
	struct mmc_cmd cmd;

	/*
	 * eMMC specification specifies that CMD12 can be used to stop a tuning
	 * command, but SD specification does not, so do nothing unless it is
	 * eMMC.
	 */
	if (opcode != MMC_CMD_SEND_TUNING_BLOCK_HS200)
		return 0;

	cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_R1;

	ret = mmc_send_cmd(mmc, &cmd, NULL);

	return ret;
}

void emmc_set_cid(unsigned int *cid)
{
	int ret;

	ret = memcpy_s(g_emmc_cid, sizeof(g_emmc_cid), cid, sizeof(g_emmc_cid));
	if (ret != EOK) {
		printf("memcpy_s emmc_cid failed\n");
		return;
	}
}

unsigned int *emmc_get_cid(void)
{
	return g_emmc_cid;
}
