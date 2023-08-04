/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "inter_cpu_communication.h"
#include "fsl_common.h"
#include "fsl_mu.h"
#include "dsp_support.h"

#include "ff.h"
#include <stdio.h>
#include "fsl_debug_console.h"
#include "virtual_fs_api.h"
#include "evaluation_config.h"
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
static uint8_t buffer[1024/*samples*/ * 2/*bytes_per_sample*/ * 2/*channels*/]; 
extern char filelist[][40];

static uint32_t g_msgSend[MSG_LENGTH];
//static uint32_t g_msgRecv[MSG_LENGTH];
/*******************************************************************************
 * Code
 ******************************************************************************/

#if 0
/*!
 * @brief Function to fill the g_msgSend array.
 * This function set the g_msgSend values 0, 1, 2, 3...
 */
static void FillMsgSend(void)
{
    uint32_t i;
    for (i = 0U; i < MSG_LENGTH; i++)
    {
        g_msgSend[i] = i;
    }
}

/*!
 * @brief Function to clear the g_msgRecv array.
 * This function set g_msgRecv to be 0.
 */
static void ClearMsgRecv(void)
{
    uint32_t i;
    for (i = 0U; i < MSG_LENGTH; i++)
    {
        g_msgRecv[i] = 0U;
    }
}

/*!
 * @brief Function to validate the received messages.
 * This function compares the g_msgSend and g_msgRecv, if they are the same, this
 * function returns true, otherwise returns false.
 */
static bool ValidateMsgRecv(void)
{
    uint32_t i;
    for (i = 0U; i < MSG_LENGTH; i++)
    {
        PRINTF("Send: %d. Receive %d\r\n", g_msgSend[i], g_msgRecv[i]);

        if (g_msgRecv[i] != g_msgSend[i])
        {
            return false;
        }
    }
    return true;
}
#endif

static uint8_t readSDdataCopyToSram(FIL *fin, uint32_t len)
{
    // 1.read fatfs sd card file to buffer
#if FLAC_READ_SDCARD    
    UINT br;
	if(f_read(fin, buffer, len, &br) != FR_OK)
    {
        PRINTF("readSDdataCopyToSram f_read error.\n");
        return 1;
    }
        
    // 2.copy data to share sram
    uint8_t *pr = NULL;
    pr = (uint8_t *)CM33WRITE_DSPREAD_ADDR;
    
    memcpy(pr, buffer, len);
#else
    static uint8_t count=0;
    UINT br;
    
    if(count < 2)
    {
        count++;
        if(f_read(fin, buffer, len, &br) != FR_OK)
        {
            PRINTF("readSDdataCopyToSram f_read error.\n");
            return 1;
        }
        
        // 2.copy data to share sram
        uint8_t *pr = NULL;
        pr = (uint8_t *)CM33WRITE_DSPREAD_ADDR;

        //memcpy(pr, testbuff, len);
        memcpy(pr, buffer, len);
    }
#endif    
    return 0;
}


static uint8_t readSramdataWriteToSD(FIL *fil, uint32_t len)
{
    uint8_t *pr = NULL;
    pr = (uint8_t *)CM33READ_DSPWRITE_ADDR;
    
#if FLAC_WRITE_TO_SDCARD
    // 1.read share sram data write SD card file
    UINT bw;
    if(f_write(fil, pr, len, &bw) != FR_OK)
    {
        PRINTF("readSramdataWriteToSD write file error\n");
        return 1;
    }    
    return 0;
#else
    return 0;   // for test, not write to sd card.
#endif    
}



static void sendAckResult(uint8_t res)
{
    g_msgSend[0] = res;

    MU_SendMsg(APP_MU, CHN_MU_REG_NUM, g_msgSend[0]);
}


/*
 *  @brief: open input file by num
 *
 *  @param: *fil:input file handler
 *
 *  		 num:file index
 *
 * */
static uint8_t getDecodeFileStructByIndexNum(FIL *filIn, FIL *filOut, uint32_t num)
{
    char inputfilename[40];
    
    sprintf(inputfilename, "%s%u", filelist[num>>4], num&0x0f);
	strcat(inputfilename, ".flac");
    PRINTF("input:%s\r\n", inputfilename);
#if 0    
    char outputfilename[40];

    sprintf(outputfilename, "%s%u", "dsp_decode_level_", num);
	strcat(outputfilename, ".wav");
        
    PRINTF("input:%s, output:%s\r\n", inputfilename, outputfilename);
    
    if(f_open(filIn, inputfilename, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        PRINTF("ERROR: opening %s for input\n", inputfilename);
        return 1;
	}
        
    if(f_open(filOut, outputfilename, (FA_WRITE | FA_CREATE_ALWAYS)) != FR_OK) {
		PRINTF("ERROR: opening %s for output\n", outputfilename);
		return 1;
	}
#endif

	// 2.copy file to share mem
	if(virtual_fs_api_copyMusic2Sram(inputfilename) == 1)
	{
		PRINTF("Decode init virtual_fs_api_copyMusic2Sram fail, %s\n", inputfilename);
	}

    return 0;
}


/*
 *  @brief: open output file by num
 *
 *  @param: *fil:ouput file handler
 *
 *  		 num:file index
 *
 * */
static uint8_t getEncodeFileStructByIndexNum(FIL *fil, FIL *filOut, uint32_t num)
{
#if 0    
    char inputfilename[30];
    char outputfilename[30];
    
    sprintf(inputfilename, "%s", "Test2");
    strcat(inputfilename, ".wav");

#if FLAC_WRITE_TO_SDCARD    
    sprintf(outputfilename, "%s%u", "dsp_encode(sm)_level_", num);
    strcat(outputfilename, ".flac");
#else
    sprintf(outputfilename, "%s%s", "dummy", ".flac");
#endif
        
    PRINTF("input:%s, output:%s\r\n", inputfilename, outputfilename);
    
    if(f_open(fil, inputfilename, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
		PRINTF("ERROR: opening %s for input\n", inputfilename);
		return 1;
	}
    
    if(f_open(filOut, outputfilename, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        PRINTF("ERROR: opening %s for input\n", outputfilename);
        return 1;
	}
    
    gs_encodeStartTimestamp = get_TimerValue();
    
    return 0;
#else
    char inputfilename[40];
	
	sprintf(inputfilename, "%s", filelist[num]);
	strcat(inputfilename, ".wav");  
        
    PRINTF("CM33: input:%s\r\n", inputfilename);
  
	// 2.copy file to share mem
	if(virtual_fs_api_copyMusic2Sram(inputfilename) == 1)
	{
		PRINTF("Encode init virtual_fs_api_copyMusic2Sram fail, %s\n", inputfilename);
        return 1;
	}

    return 0;
#endif
}


void getMUMsg_DealCMD(void)
{
    FIL fin;
    FIL fout;
    TE_MU_CMD getCmd;
    uint32_t len=0, index;

    /* Fill the g_msgSend array before send */
    //FillMsgSend();
    /* Clear the g_msgRecv array before receive */
    //ClearMsgRecv();
    
    // get dsp cmd, and deal
    while (1)
    {
        // polling get cmd and deal.
        // PRINTF("Waiting get mu...\n");
        getCmd = (TE_MU_CMD)MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
        //PRINTF("MU get cmd #%u\n", getCmd);
        switch(getCmd)
        {
            case TE_MODE_READ:       // READ FILE
                len = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
                if(readSDdataCopyToSram(&fin, len) == 1)
                {
                    sendAckResult(TE_ACK_ERROR);
                }
                else
                {
                    sendAckResult(TE_ACK_OK);
                }
            break;
            
            
            case TE_MODE_WRITE:      // WRITE FILE
               len = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
               if(readSramdataWriteToSD(&fout, len) == 1)
               {
                   sendAckResult(TE_ACK_ERROR);
               }
               else
               {
                   sendAckResult(TE_ACK_OK);
               }
            break;
        
            case TE_MODE_CLOSE:      // CLOSE FILE
                len = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);  // dummy
                if(f_close(&fout) != FR_OK || f_close(&fin) != FR_OK)
                {
                    PRINTF("close file error!\n");
                }
                else
                {                    
                    PRINTF("close file ok!\n");
                }
            break;
            
            case TE_MODE_SEEK:
                len = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
                if(f_lseek (&fout, (FSIZE_t)len) != FR_OK)
                {
                    sendAckResult(TE_ACK_ERROR);
                }    
                else
                {
                    sendAckResult(TE_ACK_OK);
                }
            break;
            
            case TE_MODE_TELL:
                len = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);  // dummy
                //PRINTF("TE_MODE_TELL len %u\n", f_tell(&fout));
                MU_SendMsg(APP_MU, CHN_MU_REG_NUM, f_tell(&fout));                
            break;
            
        case TE_MODE_TEST_DECODE:
             index = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
             if(getDecodeFileStructByIndexNum(&fin, &fout, index) == 1)
             {
                   sendAckResult(TE_ACK_ERROR);
             }
             else
             {
                   sendAckResult(TE_ACK_OK);
             }
            break;
            
        case TE_MODE_TEST_ENCODE:
             index = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);
             if(getEncodeFileStructByIndexNum(&fin, &fout, index) == 1)
             {
                sendAckResult(TE_ACK_ERROR);
             }
             else
             {
                sendAckResult(TE_ACK_OK);
             }
            break;
            
        case TE_MODE_GET_MUSICNUM:
            len = MU_ReceiveMsg(APP_MU, CHN_MU_REG_NUM);  // dummy
            MU_SendMsg(APP_MU, CHN_MU_REG_NUM, (uint32_t)TEST_MUSIC_NUMS);
        break;

          default:
          PRINTF("MU get unsupport CMD, 0x%x\n", getCmd);
          break;
        }
    }
}


void start_dsp(void)
{
	 /* Clear MUA reset */
    RESET_PeripheralReset(kMU_RST_SHIFT_RSTn);

    /* MUA init */
    MU_Init(APP_MU);

    /* Print the initial banner */
    PRINTF("\r\nMU example polling!\r\n");

    /* Copy DSP image to RAM and start DSP core. */
    BOARD_DSP_Init();

    /* Wait DSP core is Boot Up */
    while (BOOT_FLAG != MU_GetFlags(APP_MU))
    {
    }
    
    //PRINTF("\r\nDSP boot success!\r\n");
}
