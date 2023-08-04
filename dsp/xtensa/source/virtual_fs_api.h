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
#define CM33DSP_MUSIC_SOURCE_ADDR_START 0x202A0000 //0x202BD000
#define CM33DSP_MUSIC_SOURCE_ADDR_END   0x2047ffff
#define CM33DSP_MUSIC_SOURCE_LENGTH_ADDR 0x2047fff4

// encoder need file api
void virtual_fs_api_resetReadPr(void);
uint8_t virtual_fs_api_readbuf(uint8_t* data, uint32_t len);
uint8_t virtual_fs_api_copyMusic2Sram(char *inputfile);

// decoder need file api
uint8_t virtual_fs_api_seek(uint32_t absolute_byte_offset);
uint32_t virtual_fs_api_tell(void);
uint32_t virtual_fs_api_size(void);
bool virtual_fs_api_eof(void);
uint8_t virtual_fs_api_readbuf_autoMovePr(uint8_t *data, uint32_t len);

#endif

