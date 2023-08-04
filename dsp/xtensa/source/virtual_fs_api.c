/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "virtual_fs_api.h"
#include "stddef.h"
#include <string.h>

//#define READSIZE 4096
//static uint8_t buffer[READSIZE];

static uint8_t *sramReadAddr = NULL;
static uint32_t filesize = 0;

void virtual_fs_api_resetReadPr(void)
{
	sramReadAddr = (uint8_t *)CM33DSP_MUSIC_SOURCE_ADDR_START;

	virtual_fs_api_size();
}



uint8_t virtual_fs_api_readbuf_autoMovePr(uint8_t *data, uint32_t len)
{
	if(data == NULL)
		return 1;
	// check file error
	if(virtual_fs_api_eof())
		return 1;

	uint32_t readlen = len>virtual_fs_api_size()-virtual_fs_api_tell() ? virtual_fs_api_size()-virtual_fs_api_tell() : len;
	
	memcpy(data, sramReadAddr, readlen);
	sramReadAddr += readlen;

	return 0;
}

// for decode add function

// for decode add function
uint8_t virtual_fs_api_seek(uint32_t absolute_byte_offset)
{
	sramReadAddr = (uint8_t *)CM33DSP_MUSIC_SOURCE_ADDR_START + absolute_byte_offset;
	if(sramReadAddr > (uint8_t *)CM33DSP_MUSIC_SOURCE_ADDR_END)
	{
		return 1;
	}
	return 0;
}


uint32_t virtual_fs_api_tell(void)
{
	uint32_t res=0;

	res = sramReadAddr - (uint8_t *)CM33DSP_MUSIC_SOURCE_ADDR_START;

	return res;
}


// get file size
uint32_t virtual_fs_api_size(void)
{
	uint8_t *sramReadAddr = NULL;

	sramReadAddr = (uint8_t *)CM33DSP_MUSIC_SOURCE_LENGTH_ADDR;
	memcpy(&filesize, sramReadAddr, sizeof(filesize));

	return filesize;
}


// 判断文件是否结尾
bool virtual_fs_api_eof(void)
{
	return virtual_fs_api_tell()+1 >= filesize ? true : false;
}


