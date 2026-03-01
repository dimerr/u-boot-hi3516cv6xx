/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2020-2023. All rights reserved.
 */

#include "cmd_timestamp.h"
#include "asm/barriers.h"
#include "string.h"
#include "command.h"
#include "securec.h"

/*
 * timestamps storage arrangement
 * |--8 bytes--|--Timestamp Item--|--Timestamp Item--| ...
 * |Magic|Count|name |line |stamp |name |line |stamp | ...
 */

typedef struct {
	char name[TIMESTAMP_NAME_LEN];
	unsigned int line;
	unsigned int stamp;
} timestamp_item;

static unsigned long long get_timestamp_us(void)
{
	unsigned int l_value;
	unsigned int h_value;
	unsigned long long value;

	*((volatile unsigned int *)(SYS_CTRL_REG_BASE + SYSCNT_UPDATE_OFFSET)) = SYSCNT_UPDATE_OFFSET;
	l_value = *((volatile unsigned int *)(SYS_CTRL_REG_BASE + SYSCNT_REG0_OFFSET));
	h_value = *((volatile unsigned int *)(SYS_CTRL_REG_BASE + SYSCNT_REG1_OFFSET));

	value = ((unsigned long long)h_value << HIGH_BIT_SHIFT) + l_value;

	return value / SYSCNT_FREQ_24MHZ;
}

static bool timestamp_enough_memory_check(unsigned int count)
{
	bool is_enough = false;
	unsigned int count_addr = TIME_RECORD_ADDR + TIMESTAMP_ITEM_OFFSET + count * sizeof(timestamp_item);
	if (count_addr < TIMESTAMP_MAX_ADDR) {
		is_enough = true;
	}
	return is_enough;
}

static void timestamp_clear_all_once_only(void)
{
	static bool flag = false;

	if (flag) {
		return;
	}

	// should clear the memory area once that used to store timestamp logs at first.
	const int timestamp_mem_count = TIMESTAMP_MAX_ADDR - TIME_RECORD_ADDR;
	bool ret = memset_s((void *)TIME_RECORD_ADDR, timestamp_mem_count, 0xFF, timestamp_mem_count - 1);
	if (ret != EOK) {
		printf("memset_s error: %d!\n", ret);
		return;
	}

	flag = true;
}

bool timestamp_record_once_sram(unsigned int count, unsigned int offset, const char *name, unsigned int line)
{
	if (!timestamp_enough_memory_check(count + 1)) {
		printf("not enough memory to store the timestamp log from the sram!\n");
		return false;
	}

	timestamp_item *item = (timestamp_item *)(TIME_RECORD_ADDR + TIMESTAMP_ITEM_OFFSET);
	int name_len = strlen(name) + 1;
	if (name_len > TIMESTAMP_NAME_LEN - 1) {
		name_len = TIMESTAMP_NAME_LEN - 1;
	}
	int ret = memcpy_s(item[count].name, TIMESTAMP_NAME_LEN - 1, name, name_len);
	if (ret != EOK) {
		printf("memcpy_s error: %d!\n", ret);
		return false;
	}
	item[count].name[TIMESTAMP_NAME_LEN - 1] = '\0';
	item[count].line = line;
	// get the timestamp from sram stored during the booting stage of bootrom/gsl
	unsigned long sram_timestamp_addr =
		(unsigned long)(SYSCNT_AREA_START_ADDR + offset * SYS_CNT_RAM_UNIT + SYS_CNT_UNIT_LOW_OFFSET);
	unsigned int l_value = *((volatile unsigned int *)sram_timestamp_addr);
	item[count].stamp = l_value / SYS_CNT_FREQ_24MHZ;
	return true;
}

static bool timestamp_record_once(unsigned int count, const char *name, unsigned int line)
{
	if (!timestamp_enough_memory_check(count + 1)) {
		printf("not enough memory to store the timestamp log from the uboot callings!\n");
		return false;
	}

	timestamp_item *item = (timestamp_item *)(TIME_RECORD_ADDR + TIMESTAMP_ITEM_OFFSET);
	int name_len = strlen(name) + 1;
	if (name_len > TIMESTAMP_NAME_LEN - 1) {
		name_len = TIMESTAMP_NAME_LEN - 1;
	}
	int ret = memcpy_s(item[count].name, TIMESTAMP_NAME_LEN - 1, name, name_len);
	if (ret != EOK) {
		printf("memcpy_s error: %d!\n", ret);
		return false;
	}
	item[count].name[TIMESTAMP_NAME_LEN - 1] = '\0';
	item[count].line = line;
	// get the timestamp from timer directly for the booting stage of uboot.
	item[count].stamp = (unsigned int)get_timestamp_us();
	return true;
}

static bool timestamp_save_sram_timestamp_once_only(void)
{
	static bool flag = false;
	unsigned int count = 0;
	unsigned int l_value;
	unsigned int h_value;

	if (flag)
		return flag;

	l_value = *((volatile unsigned int *)(SYSCNT_AREA_START_ADDR + SYS_CNT_UNIT_LOW_OFFSET));
	h_value = *((volatile unsigned int *)(SYSCNT_AREA_START_ADDR + SYS_CNT_UNIT_HIGH_OFFSET));

	if (l_value != TIMESTAMP_MAGIC_VALUE || h_value != TIMESTAMP_MAGIC_VALUE) {
		printf("sram head is invalid [0x%x][0x%x]\n", l_value, h_value);
		return flag;
	}

	*((volatile unsigned int *)(TIME_RECORD_ADDR + TIMESTAMP_MAGIC_OFFSET)) = TIMESTAMP_MAGIC_VALUE;
	*((volatile unsigned int *)(TIME_RECORD_ADDR + TIMESTAMP_COUNT_OFFSET)) = 0;

	count = timestamp_sram_record();
	*((volatile unsigned int *)(TIME_RECORD_ADDR + TIMESTAMP_COUNT_OFFSET)) = count;
	flag = true;
	return flag;
}

void timestamp_mark(const char *name, unsigned int line)
{
	unsigned int value;
	unsigned int count;

	if (TIME_RECORD_ADDR >= TIMESTAMP_MAX_ADDR) {
		printf("[info] invalid TIME_RECORD_ADDR (0x%x) used.\n", TIME_RECORD_ADDR);
		return;
	}

	if (TIME_RECORD_ADDR < TIMESTAMP_MIN_ADDR) {
		printf("[info] invalid TIME_RECORD_ADDR (0x%x) used.\n", TIME_RECORD_ADDR);
		return;
	}

	timestamp_clear_all_once_only();

	bool timerecord_ddr_init = timestamp_save_sram_timestamp_once_only();
	if (!timerecord_ddr_init) {
		printf("[info] read sram time failed! continue to record other stage's timestamp.\n");
	}

	value = *((volatile unsigned int *)(TIME_RECORD_ADDR + TIMESTAMP_MAGIC_OFFSET));
	if (value == TIMESTAMP_MAGIC_VALUE) {
		count = *((volatile unsigned int *)(TIME_RECORD_ADDR + TIMESTAMP_COUNT_OFFSET));
	} else {
		count = 0;
		*((volatile unsigned int *)(TIME_RECORD_ADDR + TIMESTAMP_MAGIC_OFFSET)) = TIMESTAMP_MAGIC_VALUE;
	}

	if (count == TIMESTAMP_COUNT_MAX) {
		count = 0;
	}

	bool ret = timestamp_record_once(count, name, line);
	if (!ret) {
		printf("can't record timestamp for name(%s)\n", name);
		return;
	}

	*((volatile unsigned int *)(TIME_RECORD_ADDR + TIMESTAMP_COUNT_OFFSET)) = count + 1;
}

static int do_timestamp_print(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	unsigned int i;
	unsigned int value;
	unsigned int count;

	printf("TIME_RECORD_ADDR=0x%x\n", TIME_RECORD_ADDR);

	value = *((unsigned int *)(TIME_RECORD_ADDR + TIMESTAMP_MAGIC_OFFSET));
	if (value != TIMESTAMP_MAGIC_VALUE) {
		printf("no timestamp items in ddr addr[0x%x]\n", TIME_RECORD_ADDR);
		return -1;
	}
	count = *((unsigned int *)(TIME_RECORD_ADDR + TIMESTAMP_COUNT_OFFSET));
	printf("count=%d\n", count);
	timestamp_item *item = (timestamp_item *)(TIME_RECORD_ADDR + TIMESTAMP_ITEM_OFFSET);
	for (i = 0; i < count; i++) {
		printf("time stamp[%-3u] = %-8uus line: %-5u name: %s\n", i, item[i].stamp, item[i].line, item[i].name);
	}
	return 0;
}

static int do_timestamp_unit_test(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const int timestamp_mem_count = TIMESTAMP_MAX_ADDR - TIME_RECORD_ADDR;
	bool ret = false;
	printf("TEST CASE 1: test enviroment checking\n");
	printf("TIME_RECORD_ADDR is 0x%x, TIMESTAMP_MAX_ADDR is 0x%x, TIMESTAMP_MIN_ADDR is 0x%x\n",
		TIME_RECORD_ADDR,
		TIMESTAMP_MAX_ADDR,
		TIMESTAMP_MIN_ADDR);
	timestamp_print();

	printf("TEST CASE 2: normal test, timestamp TEST, expect TEST, check whether TEST timestamp print or not\n");
	// clear the timestamp memory area at first.
	ret = memset_s((void *)TIME_RECORD_ADDR, timestamp_mem_count, 0xFF, timestamp_mem_count - 1);
	if (ret != EOK) {
		printf("memset_s error! test end\n");
		return 0;
	}
	timestamp_mark("TEST", 1);
	timestamp_print();

	printf("TEST CASE 3: long name test, over than 64\n");
	// clear the timestamp memory area at first.
	ret = memset_s((void *)TIME_RECORD_ADDR, timestamp_mem_count, 0xFF, timestamp_mem_count - 1);
	if (ret != EOK) {
		printf("memset_s error! test end\n");
		return 0;
	}
	timestamp_mark("TEST65TEST65TEST65TEST65TEST65TEST65TEST65TEST65TEST65TEST65TEST65TEST65TEST65", 1);
	timestamp_print();

	printf("TEST CASE 4: less than times(TIMESTAMP_COUNT_MAX:%d) test\n", TIMESTAMP_COUNT_MAX);
	// clear the timestamp memory area at first.
	ret = memset_s((void *)TIME_RECORD_ADDR, timestamp_mem_count, 0xFF, timestamp_mem_count - 1);
	if (ret != EOK) {
		printf("memset_s error! test end\n");
		return 0;
	}
	for (int i = 0; i < TIMESTAMP_COUNT_MAX; i++) {
		timestamp_mark("TEST", i);
	}
	timestamp_print();

	printf("TEST CASE 5: less than two times(TIMESTAMP_COUNT_MAX:%d) test\n", TIMESTAMP_COUNT_MAX);
	// clear the timestamp memory area at first.
	ret = memset_s((void *)TIME_RECORD_ADDR, timestamp_mem_count, 0xFF, timestamp_mem_count - 1);
	if (ret != EOK) {
		printf("memset_s error! test end\n");
		return 0;
	}
	for (int i = 0; i < (TIMESTAMP_COUNT_MAX<<1); i++) {
		timestamp_mark("TEST", i);
	}
	timestamp_print();
	return 0;
}

void timestamp_print(void)
{
	do_timestamp_print(NULL, 0, 0, NULL);
}

U_BOOT_CMD(timestamp, 1, 0, do_timestamp_print, "print timestamp items in ddr, must define TIME_RECORD_ADDR", "\n");
U_BOOT_CMD(timestamp_unit_test, 1, 0, do_timestamp_unit_test, "timestamp unit test", "\n");
