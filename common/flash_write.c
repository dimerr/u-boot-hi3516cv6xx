/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2020-2023. All rights reserved.
 */

#include "flash_write.h"

#include <securec.h>
#include <common.h>
#if defined (CONFIG_FMC_SPI_NOR)
#include <spi_flash.h>
#endif
#if defined (CONFIG_FMC_SPI_NAND)
#include <nand.h>
#endif
#if defined(CONFIG_SUPPORT_EMMC_BOOT)
#include <mmc.h>
#endif

#if defined(CONFIG_CMD_SF) || defined(CONFIG_CMD_NAND)
static void (*schedule_notify)(unsigned long offset, unsigned long len, unsigned long off_start) = NULL;
#endif

#ifdef CONFIG_CMD_SF
static struct spi_flash *spinor_flash;

static int spinor_flash_init(void)
{
	spinor_flash = spi_flash_probe(0, 0, 0, 0);
	return 0;
}

static int spi_flash_erase_op(struct spi_flash *flash, unsigned long offset, unsigned long len)
{
	int ret;
	struct mtd_info_ex *spiflash_info = get_spiflash_info();
	unsigned long erase_start, erase_len, erase_step;

	erase_start = offset;
	erase_len   = len;
	erase_step  = spiflash_info->erasesize;

	while (len > 0) {
		if (len < erase_step)
			erase_step = len;

		ret = spi_flash_erase(flash, (u32)offset, erase_step);
		if (ret)
			return 1;

		len -= erase_step;
		offset += erase_step;
		/* notify real time schedule */
		if (schedule_notify != NULL)
			schedule_notify(offset, erase_len, erase_start);
	}

	return ret;
}

static int spinor_flash_erase(unsigned long offset, unsigned long len)
{
	if (spinor_flash == NULL)
		spinor_flash_init();
	return spi_flash_erase_op(spinor_flash, offset, len);
}

static int spi_flash_write_op(struct spi_flash *flash, unsigned long offset, unsigned long len, unsigned char *buf)
{
	int ret;
	unsigned long write_start, write_len, write_step;
	unsigned char *pbuf = buf;
	struct mtd_info_ex *spiflash_info = get_spiflash_info();

	write_start = offset;
	write_len   = len;
	write_step  = spiflash_info->erasesize;

	while (len > 0) {
		if (len < write_step)
			write_step = len;

		ret = flash->write(flash, offset, write_step, pbuf);
		if (ret)
			break;

		offset += write_step;
		pbuf   += write_step;
		len    -= write_step;
		/* notify real time schedule */
		if (schedule_notify != NULL)
			schedule_notify(offset, write_len, write_start);
	}

	return ret;
}

static int spinor_flash_write(unsigned long offset, unsigned long len, unsigned char *buf)
{
	return spi_flash_write_op(spinor_flash, offset, len, (unsigned char *)buf);
}

#else
static int spinor_flash_init(void)
{
	return -1;
}

static int spinor_flash_erase(unsigned long offset, unsigned long len)
{
	return -1;
}

static int spinor_flash_write(unsigned long offset, unsigned long len, unsigned char *buf)
{
	return -1;
}
#endif

#ifdef CONFIG_CMD_NAND
static struct mtd_info *nand_flash;

static int nand_flash_init(void)
{
	nand_flash = nand_info[0];
	return 0;
}

static int nand_flash_erase(unsigned long offset, unsigned long len)
{
	int ret;
	unsigned long erase_len;
	unsigned long erase_step;
	unsigned long length;
	nand_erase_options_t opts;

	(void)memset_s(&opts, sizeof(opts), 0, sizeof(opts));

	length = len;
	erase_step = nand_flash->erasesize;
	erase_len = length;
	opts.length  = erase_step;
	opts.offset = offset;
	opts.quiet = 1;

	while (length > 0) {
		if (length < erase_step)
			erase_step = length;

		ret = nand_erase_opts(nand_flash, &opts);
		if (ret)
			return 1;

		length -= erase_step;
		opts.offset += erase_step;
		/* notify real time schedule */
		if (schedule_notify != NULL)
			schedule_notify(opts.offset, erase_len, offset);
	}

	return ret;
}

static int nand_flash_write(unsigned long offset, unsigned long len, unsigned char *buf)
{
	int ret;
	unsigned long offset_notify;
	unsigned long write_start;
	unsigned long write_len;
	unsigned long write_step;
	size_t length;
	unsigned char *pbuf = buf;

	if (offset == 0) {
		/* Make sure the length is block size algin */
		length = len & (nand_flash->erasesize - 1) ? (size_t)(len +
			(nand_flash->erasesize - len % nand_flash->erasesize)) : len;
		write_step  = nand_flash->erasesize;
	} else {
		/* Make sure the length is writesize algin */
		length = len & (nand_flash->erasesize - 1) ? (size_t)(len +
			(nand_flash->writesize - len % nand_flash->writesize)) : len;
		write_step  = nand_flash->writesize;
	}

	write_start = offset;
	offset_notify = offset;
	write_len   = length;

	while (length > 0) {
		size_t block_offset = offset & (nand_flash->erasesize - 1);
		if (nand_flash->_block_isbad(nand_flash, offset & ~
			(nand_flash->erasesize - 1))) {
			printf("Skip bad block 0x%08llx\n", offset & ~(loff_t)(nand_flash->erasesize - 1));
			offset += nand_flash->erasesize - block_offset;
			continue;
		}

		size_t *rw_size = (size_t *)&write_step;
		struct mtd_info *mtd = get_nand_dev_by_index(0);
		ret = nand_write(mtd, offset, rw_size, pbuf);
		if (ret) {
			printf("NAND write to offset %lx failed %d\n", offset, ret);
			break;
		}
		offset += write_step;
		pbuf   += write_step;
		length -= write_step;
		offset_notify += write_step;
		/* notify real time schedule */
		if (schedule_notify != NULL)
			schedule_notify(offset_notify, write_len, write_start);
	}

	return ret;
}

#else
static int nand_flash_init(void)
{
	return -1;
}

static int nand_flash_erase(unsigned long offset, unsigned long len)
{
	return -1;
}

static int nand_flash_write(unsigned long offset, unsigned long len, unsigned char *buf)
{
	return -1;
}
#endif


#ifdef CONFIG_SUPPORT_EMMC_BOOT
static int mmc_save_init(void)
{
	struct mmc *mmc = find_mmc_device(0);
	if (mmc == NULL) {
		printf("%s:find mmc device failed\n", __func__);
		return -1;
	}

	(void)mmc_init(mmc);

	return 0;
}

static int mmc_save_write(unsigned long offset, unsigned long len, unsigned char *buf)
{
	struct mmc *mmc = find_mmc_device(0);
	if (mmc == NULL) {
		printf("%s:find mmc device failed\n", __func__);
		return -1;
	}
	if (len % MMC_MAX_BLOCK_LEN)
		blk_dwrite(mmc_get_blk_desc(mmc), (offset >> EMMC_BLOCK_SHIFT), (len >> EMMC_BLOCK_SHIFT) + 1, buf);
	else
		blk_dwrite(mmc_get_blk_desc(mmc), (offset >> EMMC_BLOCK_SHIFT), (len >> EMMC_BLOCK_SHIFT), buf);

	return 0;
}

#else
static int mmc_save_init(void)
{
	return -1;
}

static int mmc_save_write(unsigned long offset, unsigned long len, unsigned char *buf)
{
	return -1;
}
#endif

int do_flash_init(void)
{
	int ret = 0;
	switch (get_boot_media()) {
	case BOOT_MEDIA_EMMC:
		ret = mmc_save_init();
		break;
	case BOOT_MEDIA_NAND:
		ret = nand_flash_init();
		break;
	case BOOT_MEDIA_SPIFLASH:
		ret = spinor_flash_init();
		break;
	default:
		printf("[%s, %d] invalid boot media\n", __func__, __LINE__);
		return -1;
	}
	if (ret < 0)
		printf("[%s, %d] error, ret=%d\n", __func__, __LINE__, ret);
	return ret;
}

int do_flash_erase(unsigned long offset, size_t size)
{
	int ret = 0;
	switch (get_boot_media()) {
	case BOOT_MEDIA_EMMC:
		break;
	case BOOT_MEDIA_NAND:
		ret = nand_flash_erase(offset, size);
		break;
	case BOOT_MEDIA_SPIFLASH:
		ret = spinor_flash_erase(offset, size);
		break;
	default:
		printf("[%s, %d] invalid boot media\n", __func__, __LINE__);
		return -1;
	}
	if (ret < 0)
		printf("[%s, %d] error, ret=%d\n", __func__, __LINE__, ret);
	return ret;
}

int do_flash_write(unsigned long offset, uint8_t *src_addr, unsigned int size)
{
	int ret;
	if (src_addr == NULL) {
		printf("[%s] invalid src_addr\n", __func__);
		return -1;
	}

	switch (get_boot_media()) {
	case BOOT_MEDIA_EMMC:
		ret = mmc_save_write(offset, size, src_addr);
		break;
	case BOOT_MEDIA_NAND:
		ret = nand_flash_write(offset, size, src_addr);
		break;
	case BOOT_MEDIA_SPIFLASH:
		ret = spinor_flash_write(offset, size, src_addr);
		break;
	default:
		printf("[%s, %d] invalid boot media\n", __func__, __LINE__);
		return -1;
	}
	if (ret < 0)
		printf("[%s, %d] error, ret=%d\n", __func__, __LINE__, ret);
	return ret;
}
