/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef INTER_CPU_COMMUNICATION_H
#define INTER_CPU_COMMUNICATION_H

#include <stdint.h>
#include "fsl_mu.h"
#include "fsl_debug_console.h"

//uint8_t sharebuffer_cm33write_dspread[4096] __at(0x471000);
//uint8_t sharebuffer_cm33read_dspwrite[4096] @0x472000;
#define CM33WRITE_DSPREAD_ADDR 0x20001000 //0x20471000
#define CM33READ_DSPWRITE_ADDR 0x20002000 //0x20472000


#define APP_MU MUB
/* Flag indicates Core Boot Up*/
#define BOOT_FLAG 0x01U

/* Channel transmit and receive register */
#define CHN_MU_REG_NUM 0U

/* How many message is used to test message sending */
#define MSG_LENGTH 32U


// Yulong add.
#define CMD_MSG_LENGTH		2
#define CMD_ACK_MSG_LENGTH	1

enum transfermode
{
	TE_MODE_READ,
	TE_MODE_WRITE,
    TE_MODE_CLOSE,      // CLOSE FILE
    TE_MODE_SEEK,       // SEEK FILE POINTER
	TE_MODE_TELL,

	TE_MODE_TEST_DECODE,
	TE_MODE_TEST_ENCODE,
	TE_MODE_GET_MUSICNUM,
};

enum acktype
{
	TE_ACK_OK,
	TE_ACK_ERROR,
};


uint8_t GetDataFromCm33SDCardBlockingMode(uint8_t *buf, uint32_t len);
uint8_t SendDataToCm33SDCardBlockingMode(uint8_t *buf, uint32_t len);
uint8_t seekCM33SDFileBlockingMode(uint32_t pos);
uint32_t tellCM33SDFileSizeBlockingMode(void);
uint32_t getMusicFileNumsBlockingMode(void);
uint8_t closeCM33SDFileBlockingMode(void);
uint8_t CM33InitEncodeFileStructByIndex(uint32_t level);
uint8_t CM33InitDecodeFileStructByIndex(uint32_t level);
void ClearMsgRecv(void);

#endif /*  */
