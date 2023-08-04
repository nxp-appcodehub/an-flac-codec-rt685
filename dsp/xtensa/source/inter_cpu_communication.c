/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "inter_cpu_communication.h"

static uint32_t g_msgRecv[MSG_LENGTH];

/*
 *  @brief:get sd card file data(deprecated)
 *
 *  @param: *buf: get buf addr
 *
 *  		 len:length
 *
 *	@ret: 0:success
 *		  1:fail
 *
 * */
uint8_t GetDataFromCm33SDCardBlockingMode(uint8_t *buf, uint32_t len)
{
	uint32_t i;
	// 0.fill cmd buffer
	g_msgRecv[0] = TE_MODE_READ;
	g_msgRecv[1] = len;

	// 1. send read+len use MU.
	for (i = 0U; i < CMD_MSG_LENGTH; i++)
	{
		MU_SendMsg(APP_MU, CHN_MU_REG_NUM, g_msgRecv[i]);
	}
	// 2. wait ack data comeback
	for (i = 0U; i < CMD_ACK_MSG_LENGTH; i++)
	{
		g_msgRecv[i] = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
	}

	if(g_msgRecv[0] != TE_ACK_OK)
	{
		PRINTF("GetDataFromCm33SDCardBlockingMode get ack error!\r\n");
		return 1;
	}
	// 3. read share sram
	uint8_t *pr;

	pr = (uint8_t *)CM33WRITE_DSPREAD_ADDR;
	memcpy(buf, pr, len);

	return 0;
}

/*
 *  @brief: dsp save data to sharemem, and notify cm33 to save
 *
 *  @param: *buf: the data need to be save
 *
 *  		 len: data length
 *
 * */
uint8_t SendDataToCm33SDCardBlockingMode(uint8_t *buf, uint32_t len)
{
	// 1.write data to share sram
	uint8_t *pr;
	uint32_t i;

	pr = (uint8_t *)CM33READ_DSPWRITE_ADDR;
	memcpy(pr, buf, len);
	// 2. send write+len use MU.
	g_msgRecv[0] = TE_MODE_WRITE;
	g_msgRecv[1] = len;

	// 1. send read+len use MU.
	for (i = 0U; i < CMD_MSG_LENGTH; i++)
	{
		MU_SendMsg(APP_MU, CHN_MU_REG_NUM, g_msgRecv[i]);
	}
	// 3. wait data comeback?(need wait here?)
	for (i = 0U; i < CMD_ACK_MSG_LENGTH; i++)
	{
		g_msgRecv[i] = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
	}

	if(g_msgRecv[0] != TE_ACK_OK)
	{
		PRINTF("SendDataToCm33SDCardBlockingMode get ack error!\r\n");
		return 1;
	}

	return 0;
}



/*
 *  @brief: seek file
 *
 *  @param: pos: seek pos
 *
 * */
uint8_t seekCM33SDFileBlockingMode(uint32_t pos)
{
	uint8_t i;
	// 1. full buffer
	g_msgRecv[0] = TE_MODE_SEEK;
	g_msgRecv[1] = pos;

	// 2. send seek+pos use MU.
	for (i = 0U; i < CMD_MSG_LENGTH; i++)
	{
		MU_SendMsg(APP_MU, CHN_MU_REG_NUM, g_msgRecv[i]);
	}
	// 3. wait data comeback?(need wait here?)
	for (i = 0U; i < CMD_ACK_MSG_LENGTH; i++)
	{
		g_msgRecv[i] = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
	}

	if(g_msgRecv[0] != TE_ACK_OK)
	{
		PRINTF("seekCM33SDFileBlockingMode get ack error!\r\n");
		return 1;
	}

	return 0;
}



/*
 *  @brief: tell cm33 sd file size
 *
 *  @ret:size
 *
 * */
uint32_t tellCM33SDFileSizeBlockingMode(void)
{
	uint8_t i;
	// 1. full buffer
	g_msgRecv[0] = TE_MODE_TELL;
	g_msgRecv[1] = 0;

	// 2. send tell use MU.
	for (i = 0U; i < CMD_MSG_LENGTH; i++)
	{
		MU_SendMsg(APP_MU, CHN_MU_REG_NUM, g_msgRecv[i]);
	}
	// 3. use mu get tell file size.
	//for (i = 0U; i < CMD_ACK_MSG_LENGTH; i++)
	{
		g_msgRecv[0] = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
	}

	return g_msgRecv[0];
}




/*
 *  @brief: get test music file nums
 *
 *  @ret:size
 *
 * */
uint32_t getMusicFileNumsBlockingMode(void)
{
	uint8_t i;
	// 1. full buffer
	g_msgRecv[0] = TE_MODE_GET_MUSICNUM;
	g_msgRecv[1] = 0;

	// 2. send cmd use MU.
	for (i = 0U; i < CMD_MSG_LENGTH; i++)
	{
		MU_SendMsg(APP_MU, CHN_MU_REG_NUM, g_msgRecv[i]);
	}
	// 3. use mu get tell file size.
	//for (i = 0U; i < CMD_ACK_MSG_LENGTH; i++)
	{
		g_msgRecv[0] = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
	}

	return g_msgRecv[0];
}

/*
 *  @brief: close cm33 file
 *
 *  @param: NONE
 *
 * */
uint8_t closeCM33SDFileBlockingMode(void)
{
	uint8_t i;

	g_msgRecv[0] = TE_MODE_CLOSE;
	g_msgRecv[1] = 0;	// reservea
	// 1. send read+len use MU.
	for (i = 0U; i < CMD_MSG_LENGTH; i++)
	{
		MU_SendMsg(APP_MU, CHN_MU_REG_NUM, g_msgRecv[i]);
	}

	return 0;
}

/*
 *  @brief: request M33 core prepare encode file
 *
 *  @param: fileiter:file list iter.
 *
 * */
uint8_t CM33InitEncodeFileStructByIndex(uint32_t fileiter)
{
	uint8_t i;

	g_msgRecv[0] = TE_MODE_TEST_ENCODE;
	g_msgRecv[1] = fileiter;

	// 1. send encode+level use MU.
	for (i = 0U; i < CMD_MSG_LENGTH; i++)
	{
		MU_SendMsg(APP_MU, CHN_MU_REG_NUM, g_msgRecv[i]);
	}

	// 2.read back result
	for (i = 0U; i < CMD_ACK_MSG_LENGTH; i++)
	{
		g_msgRecv[0] = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
	}

	return g_msgRecv[0];
}


/*
 *  @brief: request M33 core prepare decode file
 *
 *  @param: fileiter_and_level:file iter and iter
 *
 * */
uint8_t CM33InitDecodeFileStructByIndex(uint32_t fileiter_and_level)
{
	uint8_t i;

	g_msgRecv[0] = TE_MODE_TEST_DECODE;
	g_msgRecv[1] = fileiter_and_level;

	// 1. send encode+level use MU.
	for (i = 0U; i < CMD_MSG_LENGTH; i++)
	{
		MU_SendMsg(APP_MU, CHN_MU_REG_NUM, g_msgRecv[i]);
	}

	// 2.read back result
	for (i = 0U; i < CMD_ACK_MSG_LENGTH; i++)
	{
		g_msgRecv[0] = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
	}

	return g_msgRecv[0];
}

/*!
 * @brief Function to clear the g_msgRecv array.
 * This function set g_msgRecv to be 0.
 */
void ClearMsgRecv(void)
{
    uint32_t i;
    for (i = 0U; i < MSG_LENGTH; i++)
    {
        g_msgRecv[i] = 0U;
    }
}
