/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef VIRTUAL_FS_SPI_H
#define VIRTUAL_FS_SPI_H

#include <stdint.h>
#include <stdbool.h>

// input music save addr.
#define CM33DSP_MUSIC_SOURCE_ADDR_START 0x202a0000 //0x202BD000
#define CM33DSP_MUSIC_SOURCE_ADDR_END   0x2047fff0 //0x2047FFFF
#define CM33DSP_MUSIC_SOURCE_LENGTH_ADDR 0x2047fff4

// define flac output file write mode
typedef enum writemode{
    OUTPUTFILE_WRITE_TO_SRAM,
    OUTPUTFILE_WRITE_TO_SDCARD,
}TE_FLAC_WRITE_MODE;


void virtual_fs_api_set_flac_outputfile_writemode(TE_FLAC_WRITE_MODE mode);
TE_FLAC_WRITE_MODE virtual_fs_api_get_flac_outputfile_writemode(void);


void virtual_fs_api_resetReadPr(void);
uint8_t virtual_fs_api_readbuf(uint8_t* data, uint32_t len);
uint8_t virtual_fs_api_copyMusic2Sram(char *inputfile);
uint8_t virtual_fs_api_readbuf_autoMovePr(uint8_t *data, uint32_t len);


// decoder need file api
uint8_t virtual_fs_api_seek(uint32_t absolute_byte_offset);
uint32_t virtual_fs_api_tell(void);
uint32_t virtual_fs_api_size(void);
bool virtual_fs_api_eof(void);


#endif
