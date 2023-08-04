/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017, 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include "fsl_sd.h"
#include "fsl_debug_console.h"
#include "ff.h"
#include "diskio.h"
#include "fsl_sd_disk.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "sdmmc_config.h"
#include "fsl_pca9420.h"
#include "fsl_power.h"
#include "fsl_ostimer.h"

// flac header
#include <stdlib.h>
#include "share/compat.h"
#include "FLAC/stream_decoder.h"
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"

#include "virtual_fs_api.h"
#include "inter_cpu_communication.h"
#include "evaluation_config.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static status_t sdcardWaitCardInsert(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/
static FATFS g_fileSystem; /* File system object */

static pca9420_handle_t pca9420Handle;


// Add your test music file name here without .wav suffix
char filelist[TEST_MUSIC_NUMS][40] = {
			"test_music_filename",

	};

#ifndef DSP_RUN
static FIL gs_logfile;
extern uint32_t SystemCoreClock;
static volatile uint64_t gs_encodeStartTimestamp;
static volatile uint64_t gs_encodeEndTimestamp;

#define READSIZE 1024

//static unsigned total_samples = 0; /* can use a 32-bit number due to WAVE size limitations */
static FLAC__byte buffer[READSIZE/*samples*/ * 2/*bytes_per_sample*/ * 2/*channels*/]; /* we read the WAVE data into here */
static FLAC__int32 pcm[READSIZE/*samples*/ * 2/*channels*/];

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
static void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data);

static FLAC__uint64 total_samples = 0;
static unsigned sample_rate = 0;
static unsigned channels = 0;
static unsigned bps = 0;

static uint8_t dummy_buf[10*1024];
static uint8_t dummy_buf2[10*1024];


/*******************************************************************************
 * Code
 ******************************************************************************/
static FLAC__bool write_little_endian_uint16(FIL *f, FLAC__uint16 x)
{
    return
        f_putc(x, f) != EOF &&		// yulong
        f_putc(x >> 8, f) != EOF
    ;
}

static FLAC__bool write_little_endian_int16(FIL *f, FLAC__int16 x)
{
	return write_little_endian_uint16(f, (FLAC__uint16)x);
}

static FLAC__bool write_little_endian_uint32(FIL *f, FLAC__uint32 x)
{
    return
        f_putc(x, f) != EOF &&			// yulong
        f_putc(x >> 8, f) != EOF &&
        f_putc(x >> 16, f) != EOF &&
        f_putc(x >> 24, f) != EOF
    ;

}

static int libFLAC_decode(char *inputfile, char *outputfile)
{
	FLAC__bool ok = true;
	FLAC__StreamDecoder *decoder = 0;
	FLAC__StreamDecoderInitStatus init_status;
	FIL fout;


	if((f_open(&fout, outputfile, (FA_WRITE | FA_CREATE_ALWAYS))) != FR_OK) {
		PRINTF("ERROR: opening %s for output\n", outputfile);
		return 1;
	}

	if((decoder = FLAC__stream_decoder_new()) == NULL) {
		PRINTF("ERROR: allocating decoder\n");
		f_close(&fout);
		return 1;
	}

	(void)FLAC__stream_decoder_set_md5_checking(decoder, true);

	init_status = FLAC__stream_decoder_init_file(decoder, inputfile, write_callback, metadata_callback, error_callback, /*client_data=*/&fout);
	if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		PRINTF("ERROR: initializing decoder: %s\n", FLAC__StreamDecoderInitStatusString[init_status]);
		ok = false;
	}

	if(ok) {
		ok = FLAC__stream_decoder_process_until_end_of_stream(decoder);
		PRINTF("decoding: %s\n", ok? "succeeded" : "FAILED");
		PRINTF("   state: %s\n", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(decoder)]);
	}

	FLAC__stream_decoder_delete(decoder);
	f_close(&fout);

	return 0;
}


FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	FIL *f = (FIL*)client_data;
	const FLAC__uint32 total_size = (FLAC__uint32)(total_samples * channels * (bps/8));
	size_t i;

	(void)decoder;

	if(total_samples == 0) {
		PRINTF("ERROR: this example only works for FLAC files that have a total_samples count in STREAMINFO\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	//if(channels != 2 || bps != 16) {
	//	PRINTF("ERROR: this example only supports 16bit stereo streams\n");
	//	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	//}
	//if(frame->header.channels != 2) {
	//	PRINTF("ERROR: This frame contains %u channels (should be 2)\n", frame->header.channels);
	//	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	//}
	if(buffer [0] == NULL) {
		PRINTF("ERROR: buffer [0] is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	//if(buffer [1] == NULL) {
	//	PRINTF("ERROR: buffer [1] is NULL\n");
	//	return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	//}

	// yulong
	UINT bw;
	/* write WAVE header before we write the first frame */
	if((frame->header.number.sample_number == 0) && (virtual_fs_api_get_flac_outputfile_writemode() == OUTPUTFILE_WRITE_TO_SDCARD)) {
		if(
			//f_write("RIFF", 1, 4, f) < 4 ||          
			f_write (f, "RIFF", 4, &bw) != FR_OK ||        
			!write_little_endian_uint32(f, total_size + 36) ||
			//fwrite("WAVEfmt ", 1, 8, f) < 8 ||           
			f_write(f, "WAVEfmt ", 8, &bw) != FR_OK ||        
			!write_little_endian_uint32(f, 16) ||
			!write_little_endian_uint16(f, 1) ||
			!write_little_endian_uint16(f, (FLAC__uint16)channels) ||
			!write_little_endian_uint32(f, sample_rate) ||
			!write_little_endian_uint32(f, sample_rate * channels * (bps/8)) ||
			!write_little_endian_uint16(f, (FLAC__uint16)(channels * (bps/8))) || /* block align */
			!write_little_endian_uint16(f, (FLAC__uint16)bps) ||
			//fwrite("data", 1, 4, f) < 4 ||            
			f_write(f, "data", 4, &bw) != FR_OK ||
			!write_little_endian_uint32(f, total_size)
		) {
			PRINTF("ERROR: write error\n");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
	}

	/* write decoded PCM samples */
    if(virtual_fs_api_get_flac_outputfile_writemode() == OUTPUTFILE_WRITE_TO_SRAM)
    {
        if(channels == 1){
            memcpy(dummy_buf, &buffer[0][0], sizeof(FLAC__int16)*frame->header.blocksize);
        }
        else if(channels == 2)
        {
            memcpy(dummy_buf, &buffer[0][0], sizeof(FLAC__int16)*frame->header.blocksize);
            memcpy(dummy_buf2, &buffer[1][0], sizeof(FLAC__int16)*frame->header.blocksize);
        }
    }
    else
    {
        for(i = 0; i < frame->header.blocksize; i++) {
            if(channels == 1)
            {
                if(
                    !write_little_endian_int16(f, (FLAC__int16)buffer[0][i]) //||  /* mono */
                ) {
                    PRINTF("ERROR: write error\n");
                    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
                }
            }
            else if(channels == 2)
            {
                if(
                    !write_little_endian_int16(f, (FLAC__int16)buffer[0][i]) ||  /* left channel */
                    !write_little_endian_int16(f, (FLAC__int16)buffer[1][i])     /* right channel */
                ) {
                    PRINTF("ERROR: write error\n");
                    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
                }
            }
        }
    }

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	(void)decoder, (void)client_data;

	/* print some stats */
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		/* save for later */
		total_samples = metadata->data.stream_info.total_samples;
		sample_rate = metadata->data.stream_info.sample_rate;
		channels = metadata->data.stream_info.channels;
		bps = metadata->data.stream_info.bits_per_sample;

		/*PRINTF("sample rate    : %u Hz\n", sample_rate);
		PRINTF("channels       : %u\n", channels);
		PRINTF("bits per sample: %u\n", bps);
		PRINTF("total samples  : %u\n", total_samples);*/
	}
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;

	//PRINTF("Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}



static int my_EncodeDemo(char *inputfile, char *outputfile, uint32_t compresslevel)
{
	FLAC__bool ok = true;
	FLAC__StreamEncoder *encoder = 0;
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata *metadata[2];
	FLAC__StreamMetadata_VorbisComment_Entry entry;
	FIL fin;
	unsigned sample_rate = 0;
	unsigned channels = 0;
	unsigned bps = 0;


	if(f_open(&fin, inputfile, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
		PRINTF("ERROR: opening %s for output\n", inputfile);
		return 1;
	}
	

	//UINT br;
	/* read wav header and validate it */
	if(
		//fread(buffer, 1, 44, fin) != 44 ||
		//f_read(&fin, buffer, 44, &br) != FR_OK ||
		virtual_fs_api_readbuf_autoMovePr(buffer, 44) != 0 ||
		memcmp(buffer, "RIFF", 4) ||
		//memcmp(buffer+8, "WAVEfmt \020\000\000\000\001\000\002\000", 16) ||  // 
		memcmp(buffer+8, "WAVEfmt", 7) ||
		//memcmp(buffer+32, "\004\000\020\000data", 8)
		memcmp(buffer+36, "data", 4)
	) {
		PRINTF("ERROR: invalid/unsupported WAVE file, only 16bps stereo WAVE in canonical form allowed\n");
		//yulong
		for(int iter=0; iter<44; iter++)
		{
			PRINTF("0x%02x ", buffer[iter]);
			if(iter % 4 == 0)
				PRINTF("\n");
		}
		
		f_close(&fin);
		return 1;
	}
	sample_rate = ((((((unsigned)buffer[27] << 8) | buffer[26]) << 8) | buffer[25]) << 8) | buffer[24];
	channels =  (buffer[23] << 8) | buffer[22]; //2;  --yulong
	bps = (buffer[35] << 8) | buffer[34]; //16;
	total_samples = (((((((unsigned)buffer[43] << 8) | buffer[42]) << 8) | buffer[41]) << 8) | buffer[40]) / channels / (bps/8);

	PRINTF("wav sample_rate:%u, totalk_samples:%u\n", sample_rate, total_samples);
	/* allocate the encoder */
	if((encoder = FLAC__stream_encoder_new()) == NULL) {
		PRINTF("ERROR: allocating encoder\n");
		f_close(&fin);
		return 1;
	}

	ok &= FLAC__stream_encoder_set_verify(encoder, true);
	ok &= FLAC__stream_encoder_set_compression_level(encoder, compresslevel); //5);
	ok &= FLAC__stream_encoder_set_channels(encoder, channels);
	ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, bps);
	ok &= FLAC__stream_encoder_set_sample_rate(encoder, sample_rate);
	ok &= FLAC__stream_encoder_set_total_samples_estimate(encoder, total_samples);

	/* now add some metadata; we'll add some tags and a padding block */
	if(ok) {
		if(
			(metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT)) == NULL ||
			(metadata[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)) == NULL ||
			/* there are many tag (vorbiscomment) functions but these are convenient for this particular use: */
			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ARTIST", "Some Artist") ||
			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false) || /* copy=false: let metadata object take control of entry's allocated string */
			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "YEAR", "1984") ||
			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false)
		) {
			PRINTF("ERROR: out of memory or tag error\n");
			ok = false;
		} else {

			metadata[1]->length = 1234; /* set the padding length */

			ok = FLAC__stream_encoder_set_metadata(encoder, metadata, 2);

                }
	}

	/* initialize encoder */
	if(ok) {
		init_status = FLAC__stream_encoder_init_file(encoder, outputfile, progress_callback, /*client_data=*/NULL);  //yulong
		if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
			PRINTF("ERROR: initializing encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
			ok = false;
		}
	}

	/* read blocks of samples from WAVE file and feed to encoder */
	if(ok) {
		size_t left = (size_t)total_samples;
		while(ok && left) {
			size_t need = (left>READSIZE? (size_t)READSIZE : (size_t)left);

			//if(f_read(&fin, buffer, need*channels*(bps/8), &br) !=  FR_OK)
			if(virtual_fs_api_readbuf_autoMovePr(buffer, need*channels*(bps/8)) == 1)
			{				
				PRINTF("ERROR: reading from WAVE file\n");
				ok = false;
			}
			else {
				/* convert the packed little-endian 16-bit PCM samples from WAVE into an interleaved FLAC__int32 buffer for libFLAC */
				size_t i;
				for(i = 0; i < need*channels; i++) {
					/* inefficient but simple and works on big- or little-endian machines */
					pcm[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)buffer[2*i+1] << 8) | (FLAC__int16)buffer[2*i]);
				}
				/* feed samples to encoder */
				ok = FLAC__stream_encoder_process_interleaved(encoder, pcm, need);
			}
			left -= need;
		}
	}

	ok &= FLAC__stream_encoder_finish(encoder);

	PRINTF("encoding: %s\n", ok? "succeeded" : "FAILED");
	//PRINTF("   state: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);

	/* now that encoding is finished, the metadata can be freed */
	FLAC__metadata_object_delete(metadata[0]);
	FLAC__metadata_object_delete(metadata[1]);

	FLAC__stream_encoder_delete(encoder);
	//f_close(&fin);

	return 0;
}


void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data)
{
	(void)encoder, (void)client_data;

	//PRINTF("wrote %u bytes, %u/ %u samples, %u %u frames\n", bytes_written, samples_written, total_samples, frames_written, total_frames_estimate);
	//PRINTF("%u, %u\n", total_samples, total_frames_estimate);
}
#endif


#if 0
static uint8_t ostest_buf[1314];
static void sdPerformance_test(void)
{
    uint8_t iter=0, yter=0;
    char logtxt[40];
    UINT bw;

    while(1)
    {
        gs_encodeStartTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER0); //get_TimerValue();
        
        // 1. file open           
        sprintf(logtxt, "%s_%u.txt", "cm33_encoder3", yter++);
        PRINTF("file name is %s\n", logtxt);
        if(f_open(&gs_logfile, logtxt, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            PRINTF("ERROR: opening %s for input\n", logtxt);
        }
        // 2.loop write file
        memset(ostest_buf, 0x00, sizeof(ostest_buf));

        /*if(f_write(&gs_logfile, ostest_buf, sizeof(ostest_buf)/20, &bw) != FR_OK) {  // yulong
            PRINTF("write %s, fail.\n", logtxt);
        }
        if(f_write(&gs_logfile, ostest_buf, sizeof(ostest_buf)/10, &bw) != FR_OK) {  // yulong
            PRINTF("write %s, fail.\n", logtxt);
        }*/
        for(iter=0; iter<200; iter++)
        {
            if(f_write(&gs_logfile, ostest_buf, sizeof(ostest_buf), &bw) != FR_OK) {  // yulong
                PRINTF("write %s, fail.\n", logtxt);
            }
        }
        /*f_lseek(&gs_logfile, 10);
        if(f_write(&gs_logfile, ostest_buf, sizeof(ostest_buf)/2, &bw) != FR_OK) {  // yulong
            PRINTF("write %s, fail.\n", logtxt);
        }
        if(f_write(&gs_logfile, ostest_buf, sizeof(ostest_buf)/10, &bw) != FR_OK) {  // yulong
            PRINTF("write %s, fail.\n", logtxt);
        }*/
        
        // 3.close file
        f_close(&gs_logfile);

        gs_encodeEndTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER0);
        
        if(gs_encodeEndTimestamp <= gs_encodeStartTimestamp)
            PRINTF("something error here! start:%llu, end:%llu\r\n", gs_encodeStartTimestamp, gs_encodeEndTimestamp);
        PRINTF("%llums\n", (gs_encodeEndTimestamp-gs_encodeStartTimestamp)/(SystemCoreClock/1000));
    }
}
#endif


#ifndef DSP_RUN
 /*
 * brief: cm33 encode test start
 * param: (*filelist)[40]: need decode file name 
 *          filenum: file totle num
 *          *logfilename: output log file name
 * ret: 0: ok
 *      1: fail
 */
static uint8_t flac_cm33_encode_performance_test_start(char (*filelist)[40], uint8_t filenum, char *logfilename)
{
    FRESULT error;
    UINT bw;
	char strbuf[40] = "";
    char inputfilename[40];
	char outputfilename[40];
    char logtxt[40];
	uint8_t compresslevel=0, fileiter=0;

    // 1. open log file
	error = f_open(&gs_logfile, logfilename, FA_WRITE | FA_CREATE_ALWAYS);
	if (error)
    {
        PRINTF("f_open failed. %d\r\n", error);
		return 1;
    }
		
    // 2. loop decode file
	for(fileiter = 0; fileiter < filenum; fileiter++)	
	{
        // 2.1 write file name to log
		sprintf(strbuf, "%s\n", filelist[fileiter]);
		if(f_write(&gs_logfile, strbuf, strlen(strbuf)+1, &bw) == FR_OK) {	// yulong

		}

        // 2.2 prepare inputfile
		sprintf(inputfilename, "%s", filelist[fileiter]);
		strcat(inputfilename, ".wav");
		PRINTF("\n\n\nFile No.#%u, encoder input:%s\r\n", fileiter, inputfilename);
 
		if(virtual_fs_api_copyMusic2Sram(inputfilename) == 1)
		{
			PRINTF("virtual_fs_api_copyMusic2Sram error! %s\n", inputfilename);
			return 1;
		}
		
        // 2.3 loop encode file to flac
		for(compresslevel=0; compresslevel<=8; compresslevel++)
		{
            // 2.3.1 prepare outputfile 
            if(virtual_fs_api_get_flac_outputfile_writemode() == OUTPUTFILE_WRITE_TO_SDCARD)
            {            
			    sprintf(outputfilename, "%s%u", filelist[fileiter], compresslevel);
            }
            else
            {
                sprintf(outputfilename, "dummy");    
            }
            strcat(outputfilename, ".flac");

			// 2.3.2 reset share memeroy read pointer.
            virtual_fs_api_resetReadPr();			
			PRINTF("#compresslevel:%u Start encode. %s\r\n", compresslevel, outputfilename);

            // 2.3.3 start encode and measure time.
			gs_encodeStartTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER0);
			my_EncodeDemo(inputfilename, outputfilename, compresslevel);
			gs_encodeEndTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER0);
            if(gs_encodeEndTimestamp <= gs_encodeStartTimestamp)
                PRINTF("something error here! start:%llu, end:%llu\r\n", gs_encodeStartTimestamp, gs_encodeEndTimestamp);
            
            PRINTF("End encode. %llu(ms)\r\n", (gs_encodeEndTimestamp-gs_encodeStartTimestamp)/(SystemCoreClock/1000));
			// write data to log file.
			memset(strbuf, 0x00, sizeof(strbuf));
			sprintf(strbuf, "%llu\n", (gs_encodeEndTimestamp-gs_encodeStartTimestamp)/(SystemCoreClock/1000));
			if(f_write(&gs_logfile, strbuf, strlen(strbuf)+1, &bw) != FR_OK) {
				PRINTF("write %s, fail.\n", logtxt);
			}				
		}
	}
	f_close(&gs_logfile);

    return 0;
}

/*
 * brief: cm33 decode test start
 * param: (*filelist)[40]: need decode file name 
 *          filenum: file totle num
 *          *logfilename: output log file name
 * ret: 0: ok
 *      1: fail
 */
static uint8_t flac_cm33_decode_performance_test_start(char (*filelist)[40], uint8_t filenum, char *logfilename)
{
    FRESULT error;
    UINT bw;
	char strbuf[40] = "";
    char inputfilename[40];
	char outputfilename[40];
    char logtxt[40];
	uint8_t compresslevel=0, fileiter=0;

    // 1. open log file
	error = f_open(&gs_logfile, logfilename, FA_WRITE | FA_CREATE_ALWAYS);
	if (error)
    {
        PRINTF("f_open failed. %d\r\n", error);
		return 1;
    }

    // 2. loop decode file    
	for(fileiter = 0; fileiter < filenum; fileiter++)
	{
        // 2.1 write file name to log
		sprintf(strbuf, "%s\n", filelist[fileiter]);
		if(f_write(&gs_logfile, strbuf, strlen(strbuf)+1, &bw) != FR_OK) {  // yulong
            PRINTF("ERROR: f_write %s for input\n", logtxt);
            return 1;
		}
		PRINTF("\n\n\n\nFile No.#%u\n", fileiter);

        // 2.2 loop decode 9 compresslevel file to wav.
		for(compresslevel = 0; compresslevel < 9; compresslevel++)
		{
            // 2.2.1 prepare input&output file
			sprintf(inputfilename, "%s%u", filelist[fileiter], compresslevel);
			strcat(inputfilename, ".flac");
			
            if(virtual_fs_api_get_flac_outputfile_writemode() == OUTPUTFILE_WRITE_TO_SDCARD)
			    sprintf(outputfilename, "%s%u", filelist[fileiter], compresslevel);
            else if(virtual_fs_api_get_flac_outputfile_writemode() == OUTPUTFILE_WRITE_TO_SRAM)                
			    sprintf(outputfilename, "dummy");	// write to sram, use dummy file(avoid modify needed file)

			strcat(outputfilename, ".wav");			
			PRINTF("input:%s, output:%s\r\n", inputfilename, outputfilename);

            // 2.2.2 copy flac to share memeroy
			if(virtual_fs_api_copyMusic2Sram(inputfilename) == 1)
			{
				PRINTF("virtual_fs_api_copyMusic2Sram error! %s\n", inputfilename);
				return 1;
			}
			virtual_fs_api_resetReadPr();

            // 2.2.3 start decode and measure time.
			PRINTF("#%u Start decode. \r\n", compresslevel);	
			gs_encodeStartTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER0); //get_TimerValue();
			libFLAC_decode(inputfilename, outputfilename);
			gs_encodeEndTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER0);
			PRINTF("End decode.%llu(ms)\r\n", (gs_encodeEndTimestamp-gs_encodeStartTimestamp)/(SystemCoreClock/1000));

			// write data to log file.
			memset(strbuf, 0x00, sizeof(strbuf));
			sprintf(strbuf, "%llu\n", (gs_encodeEndTimestamp-gs_encodeStartTimestamp)/(SystemCoreClock/1000));
			if(f_write(&gs_logfile, strbuf, strlen(strbuf), &bw) != FR_OK) {  // yulong
				PRINTF("write %s, fail.\n", logtxt);
			}
		}
	}
	f_close(&gs_logfile);	

    return 0;
}
#endif



static status_t sdcardWaitCardInsert(void)
{
    BOARD_SD_Config(&g_sd, NULL, BOARD_SDMMC_SD_HOST_IRQ_PRIORITY, NULL);

    /* SD host init function */
    if (SD_HostInit(&g_sd) != kStatus_Success)
    {
        PRINTF("\r\nSD host init fail\r\n");
        return kStatus_Fail;
    }

    /* wait card insert */
    if (SD_PollingCardInsert(&g_sd, kSD_Inserted) == kStatus_Success)
    {
        PRINTF("\r\nCard inserted.\r\n");
        /* power off card */
        SD_SetCardPower(&g_sd, false);
        /* power on the card */
        SD_SetCardPower(&g_sd, true);
    }
    else
    {
        PRINTF("\r\nCard detect fail.\r\n");
        return kStatus_Fail;
    }

    return kStatus_Success;
}

/*
static void ostimer_test(void)
{
    gs_encodeEndTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER0);
    PRINTF("Before reset timerstamp:%llu\r\n", gs_encodeEndTimestamp);

    // reset os event timer
    // *(uint32_t *)(0x40020000 + 0x10) |= 0x01 << 27;  // OS Event Timer reset control
    // *(uint32_t *)(0x40020000 + 0x10) &= ~(0x01 << 27);
    *(uint32_t *)(0x40020000 + 0x40) = 0x01 << 27;
    *(uint32_t *)(0x40020000 + 0x70) = 0x01 << 27;

    // reget value
    gs_encodeEndTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER0);
    PRINTF("after reset timerstamp:%llu\r\n", gs_encodeEndTimestamp);
    
    sdPerformance_test();
    while(1);
}*/

/*!
 * @brief Main function
 */
int main(void)
{
    FRESULT error;
    const TCHAR driverNumberBuffer[3U] = {SDDISK + '0', ':', '/'};

    pca9420_config_t pca9420Config;
    pca9420_modecfg_t pca9420ModeCfg[2];
    uint32_t i;

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();
    
    /*Make sure USDHC ram buffer has power up*/
    POWER_DisablePD(kPDRUNCFG_APD_USDHC0_SRAM);
    POWER_DisablePD(kPDRUNCFG_PPD_USDHC0_SRAM);
    POWER_DisablePD(kPDRUNCFG_PD_LPOSC);
    POWER_ApplyPD();

    /* PMIC PCA9420 */
    CLOCK_AttachClk(kSFRO_to_FLEXCOMM15);
    BOARD_PMIC_I2C_Init();
    PCA9420_GetDefaultConfig(&pca9420Config);
    pca9420Config.I2C_SendFunc    = BOARD_PMIC_I2C_Send;
    pca9420Config.I2C_ReceiveFunc = BOARD_PMIC_I2C_Receive;
    PCA9420_Init(&pca9420Handle, &pca9420Config);
    for (i = 0; i < ARRAY_SIZE(pca9420ModeCfg); i++)
    {
        PCA9420_GetDefaultModeConfig(&pca9420ModeCfg[i]);
    }
    pca9420ModeCfg[0].ldo2OutVolt = kPCA9420_Ldo2OutVolt3V300;
    pca9420ModeCfg[1].ldo2OutVolt = kPCA9420_Ldo2OutVolt1V800;
    PCA9420_WriteModeConfigs(&pca9420Handle, kPCA9420_Mode0, &pca9420ModeCfg[0], ARRAY_SIZE(pca9420ModeCfg));

    /* SDIO0 */
    /* usdhc depend on 32K clock also */
    CLOCK_AttachClk(kLPOSC_DIV32_to_32KHZWAKE_CLK);
    CLOCK_AttachClk(kAUX0_PLL_to_SDIO0_CLK);
    CLOCK_SetClkDiv(kCLOCK_DivSdio0Clk, 1);

    PRINTF("\r\nFLAC example to demonstrate how to use FLAC with CM33&HiFi4 core.\r\n");

    PRINTF("\r\nPlease insert a card into board.\r\n");

    if (sdcardWaitCardInsert() != kStatus_Success)
    {
        return -1;
    }

    if (f_mount(&g_fileSystem, driverNumberBuffer, 0U))
    {
        PRINTF("Mount volume failed.\r\n");
        return -1;
    }
    
#if (FF_FS_RPATH >= 2U)
    error = f_chdrive((char const *)&driverNumberBuffer[0U]);
    if (error)
    {
        PRINTF("Change drive failed.\r\n");
        return -1;
    }
#endif

#if FF_USE_MKFS
    BYTE work[FF_MAX_SS];
    PRINTF("\r\nMake file system......The time may be long if the card capacity is big.\r\n");
    if (f_mkfs(driverNumberBuffer, 0, work, sizeof work))
    {
        PRINTF("Make file system failed. \r\n");
        return -1;
    }
#endif /* FF_USE_MKFS */

    // init ostimer for time measure.
	CLOCK_AttachClk(kHCLK_to_OSTIMER_CLK); //); //kLPOSC_to_OSTIMER_CLK 1MHz
    
    *(uint32_t *)(0x40020000 + 0x40) = 0x01 << 27;
    *(uint32_t *)(0x40020000 + 0x70) = 0x01 << 27;
    
	OSTIMER_Init(OSTIMER0);

    uint64_t timerTicks = OSTIMER_GetCurrentTimerValue(OSTIMER0);
	PRINTF("Now timerTicks is %llu\n", timerTicks);

    //sdPerformance_test();
    //while(1);
    //ostimer_test();   

#ifndef DSP_RUN
    char logfilelist[][40] = {
            "cm33_encode_log_write2SD.txt",
            "cm33_decode_log_write2SD.txt",
            "cm33_encode_log_write2SRAM1.txt",
            "cm33_decode_log_write2SRAM1.txt",
    };
    
    // STEP1: CM33 ENCODE and DECODE with file write to SD card
    virtual_fs_api_set_flac_outputfile_writemode(OUTPUTFILE_WRITE_TO_SDCARD);
    if(flac_cm33_encode_performance_test_start(filelist, TEST_MUSIC_NUMS, logfilelist[0]))
	{
		PRINTF("CM33 encode to SD card fail!\n");
		while(1);
	}
    if(flac_cm33_decode_performance_test_start(filelist, TEST_MUSIC_NUMS, logfilelist[1]))
	{
		PRINTF("CM33 decode to SD card fail!\n");
		while(1);
	}

    // STEP2: CM33 ENCODE and DECODE with file write to SRAM
    virtual_fs_api_set_flac_outputfile_writemode(OUTPUTFILE_WRITE_TO_SRAM);
    if(flac_cm33_encode_performance_test_start(filelist, TEST_MUSIC_NUMS, logfilelist[2]))
	{
		PRINTF("CM33 encode to SRAM fail!\n");
		while(1);
	}
    if(flac_cm33_decode_performance_test_start(filelist, TEST_MUSIC_NUMS, logfilelist[3]))
	{
		PRINTF("CM33 decode to SRAM fail!\n");
		while(1);
	}
#else
    // STEP1: DSP START, ENCODE and DECODE with file write to SRAM
    start_dsp();
	getMUMsg_DealCMD();
#endif

    PRINTF("Finish!\n");
    while(1);
}
