/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef INTER_CPU_COMMUNICATION_H
#define INTER_CPU_COMMUNICATION_H

#include <stdint.h>

#define CMD_MSG_LENGTH		2
#define CMD_ACK_MSG_LENGTH	1

typedef enum transfermode
{
	TE_MODE_READ,       // READ FILE  0
	TE_MODE_WRITE,      // WRITE FILE 1
    TE_MODE_CLOSE,      // CLOSE FILE 2
    TE_MODE_SEEK,       // SEEK FILE POINTER 3
    TE_MODE_TELL,       // 4
    
    TE_MODE_TEST_DECODE,
    TE_MODE_TEST_ENCODE,
    TE_MODE_GET_MUSICNUM

}TE_MU_CMD;

enum acktype
{
	TE_ACK_OK,
	TE_ACK_ERROR,
};

// 0x2040 0000 0x2047 FFFF 0x3040 0000 0x3047 FFFF Shared RAM on CM33 data bus, partitions 28 to 29
#define CM33WRITE_DSPREAD_ADDR 0x20001000 //0x20471000
#define CM33READ_DSPWRITE_ADDR 0x20002000 //0x20472000

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define APP_MU MUA
/* Flag indicates Core Boot Up*/
#define BOOT_FLAG 0x01U

/* Channel transmit and receive register */
#define CHN_MU_REG_NUM 0U

/* How many message is used to test message sending */
#define MSG_LENGTH 32U


#define FLAC_WRITE_TO_SDCARD 1
#define FLAC_READ_SDCARD     1

void start_dsp(void);
void getMUMsg_DealCMD(void);

#endif