/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2020-2023. All rights reserved.
 */
	.section .image,#alloc
	.globl	input_data
    /*gzip source addr must be 16 bytes aligned*/
    .balign 16
input_data:
	.incbin   "image_data.gzip"
	.globl    input_data_end
input_data_end:
