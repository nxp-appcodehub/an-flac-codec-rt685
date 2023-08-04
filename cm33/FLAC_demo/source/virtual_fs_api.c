/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "virtual_fs_api.h"
#include "fsl_debug_console.h"
#include "ff.h"

static FIL gs_inputfile;


#define READSIZE 4096
static uint8_t buffer[READSIZE];

static uint8_t *sramReadAddr = NULL;  	// file read pr
static uint32_t filesize = 0;			// save file length
static TE_FLAC_WRITE_MODE sfilewrite2SDMode = OUTPUTFILE_WRITE_TO_SDCARD;

/*
 * flag: 0:file write to sram
 *		 1:file write to sd card
 */
void virtual_fs_api_set_flac_outputfile_writemode(TE_FLAC_WRITE_MODE mode)
{
	sfilewrite2SDMode = mode;
}


TE_FLAC_WRITE_MODE virtual_fs_api_get_flac_outputfile_writemode(void)
{
	return sfilewrite2SDMode;
}


void virtual_fs_api_resetReadPr(void)
{
	// 1.init pos
	sramReadAddr = (uint8_t *)CM33DSP_MUSIC_SOURCE_ADDR_START;

	// 2.get size
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


uint8_t virtual_fs_api_copyMusic2Sram(char *inputfilename)
{	
	uint8_t *sramWriteAddr = NULL;
	
	// 1.open file
	if(f_open(&gs_inputfile, inputfilename, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
		PRINTF("ERROR: open SD card %s for input failed.\n", inputfilename);
		PRINTF("Please check sd card file.\n");
		return 1;
	}

	// 2.check file size
	uint32_t filesize = f_size(&gs_inputfile);
	PRINTF("input:%s, filesize is %u\r\n", inputfilename, filesize);
	if(CM33DSP_MUSIC_SOURCE_ADDR_END - CM33DSP_MUSIC_SOURCE_ADDR_START < filesize)
	{
		PRINTF("Error: file too large!\n");
		return 1;
	}
	// 3.save file size to mem
	sramWriteAddr = (uint8_t *)CM33DSP_MUSIC_SOURCE_LENGTH_ADDR;
	memcpy(sramWriteAddr, &filesize, sizeof(filesize));	

	// 4.copy file to sram
	uint32_t readsize=0;
	UINT br;
	sramWriteAddr = (uint8_t *)CM33DSP_MUSIC_SOURCE_ADDR_START;
	
	while(filesize > 0)
	{
		readsize = filesize>READSIZE?READSIZE:filesize;
		if(f_read(&gs_inputfile, buffer, readsize, &br) !=  FR_OK)
		{
			PRINTF("ERROR: read file error!\n");
			return 1;
		}
		else
		{
			//PRINTF("writeaddr:%x, len:%u\n", sramWriteAddr, readsize);
			memcpy(sramWriteAddr, buffer, readsize);
			sramWriteAddr += readsize;	
			filesize -= readsize;
		}
	}
	
	// 5.check
	

	// 6.close
	f_close(&gs_inputfile);

	return 0;
}




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


// check if reach end of file
bool virtual_fs_api_eof(void)
{
	return virtual_fs_api_tell()+1 >= filesize ? true : false;
}
