/*
 * Copyright (c) 2013 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2019, 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <xtensa/config/core.h>
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "fsl_mu.h"
#include "pin_mux.h"
#include "board_hifi4.h"
// add for profile
#include <xtensa/xt_profiling.h>
#include "virtual_fs_api.h"
#include "inter_cpu_communication.h"
#include "time.h"
#include "fsl_ostimer.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "share/compat.h"
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"
#include "FLAC/stream_decoder.h"

#define READSIZE 1024
static FLAC__uint64 total_samples = 0;
static unsigned sample_rate = 0;
static unsigned channels = 0;
static unsigned bps = 0;
static uint32_t dummy_buf;
static uint8_t dummy_buf1[10*1024];
static uint8_t dummy_buf2[10*1024];

//static unsigned total_samples = 0; /* can use a 32-bit number due to WAVE size limitations */
static FLAC__byte buffer[READSIZE/*samples*/ * 2/*bytes_per_sample*/ * 2/*channels*/]; /* we read the WAVE data into here */
static FLAC__int32 pcm[READSIZE/*samples*/ * 2/*channels*/];


static volatile uint64_t gs_encodeStartTimestamp;
static volatile uint64_t gs_encodeEndTimestamp;
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DECODE_NO_WRITE_FILE 1


/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void LED_INIT();
void LED_TOGGLE();
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
static void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data);

/*******************************************************************************
 * Code
 ******************************************************************************/
void LED_INIT()
{
    CLOCK_EnableClock(kCLOCK_HsGpio0);
    RESET_PeripheralReset(kHSGPIO0_RST_SHIFT_RSTn);
    LED_RED_INIT(LOGIC_LED_OFF);
}
void LED_TOGGLE()
{
    LED_RED_TOGGLE();
}

/*!
 * @brief Function to create delay for Led blink.
 */
void delay(void)
{
    volatile uint32_t i = 0;
    for (i = 0; i < 5000000; ++i)
    {
        __NOP();
    }
}



// Encode
int my_EncodeDemo(char *inputfile, char *outputfile, uint32_t compresslevel)
{
	FLAC__bool ok = true;
	FLAC__StreamEncoder *encoder = 0;
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata *metadata[2];
	FLAC__StreamMetadata_VorbisComment_Entry entry;
	//FIL fin;
	unsigned sample_rate = 0;
	unsigned channels = 0;
	unsigned bps = 0;
	uint32_t total_samples = 0;


	/*
	if(f_open(&fin, inputfile, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
		PRINTF("ERROR: opening %s for output\n", inputfile);
		return 1;
	}
	*/
	//CM33InitEncodeFileStructByIndex(compresslevel);
	//return 0;

	//UINT br;
	/* read wav header and validate it */
	if(
		//fread(buffer, 1, 44, fin) != 44 ||
		//f_read(&fin, buffer, 44, &br) != FR_OK ||
		//GetDataFromCm33SDCardBlockingMode(buffer, 44) != 0 ||
		virtual_fs_api_readbuf_autoMovePr(buffer, 44) != 0 ||
		memcmp(buffer, "RIFF", 4) ||
		//memcmp(buffer+8, "WAVEfmt \020\000\000\000\001\000\002\000", 16) ||  //
		memcmp(buffer+8, "WAVEfmt \020", 8) ||
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

		//f_close(&fin);
		closeCM33SDFileBlockingMode();
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
		//f_close(&fin);
		closeCM33SDFileBlockingMode();
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

			//if(f_read(&fin, buffer, need*channels*(bps/8), &br) !=  FR_OK)	// yulong
			//if(GetDataFromCm33SDCardBlockingMode(buffer, need*channels*(bps/8)) != 0)
			if(virtual_fs_api_readbuf_autoMovePr(buffer, need*channels*(bps/8)) != 0)
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

	//PRINTF("encoding: %s\n", ok? "succeeded" : "FAILED");
	//PRINTF("   state: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);
	PRINTF("Heap addr:%x\n", sbrk(0));


	/* now that encoding is finished, the metadata can be freed */
	FLAC__metadata_object_delete(metadata[0]);
	FLAC__metadata_object_delete(metadata[1]);

	FLAC__stream_encoder_delete(encoder);

	//f_close(&fin);
	//closeCM33SDFileBlockingMode();

	return 0;
}

void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data)
{
	(void)encoder, (void)client_data;

	//PRINTF("wrote %u bytes, %u/ %u samples, %u %u frames\n", bytes_written, samples_written, total_samples, frames_written, total_frames_estimate);
	//PRINTF("%u, %u\n", total_samples, total_frames_estimate);
}


static FLAC__bool write_little_endian_uint16(FILE *f, FLAC__uint16 x)
{
#if DECODE_NO_WRITE_FILE
    //f_lseek (f, 2);
	memcpy(&dummy_buf, &x, sizeof(x));
    //PRINTF("%u", x);
	return true;
#else
	return
		f_putc(x, f) != EOF &&		// yulong
		f_putc(x >> 8, f) != EOF
	;
#endif
}
#if !DECODE_NO_WRITE_FILE
static FLAC__bool write_little_endian_int16(FILE *f, FLAC__int16 x)
{
	return write_little_endian_uint16(f, (FLAC__uint16)x);
}
#endif

static FLAC__bool write_little_endian_uint32(FILE *f, FLAC__uint32 x)
{
#if DECODE_NO_WRITE_FILE
    //f_lseek (f, 4);
	memcpy(&dummy_buf, &x, sizeof(x));
    //PRINTF("%u", x);
	return true;
#else
	return
		f_putc(x, f) != EOF &&			// yulong
		f_putc(x >> 8, f) != EOF &&
		f_putc(x >> 16, f) != EOF &&
		f_putc(x >> 24, f) != EOF
	;
#endif
}

int libFLAC_decode(char *inputfile, char *outputfile)
{
	FLAC__bool ok = true;
	FLAC__StreamDecoder *decoder = 0;
	FLAC__StreamDecoderInitStatus init_status;
	FILE fout;


	/*if((f_open(&fout, outputfile, (FA_WRITE | FA_CREATE_ALWAYS))) != FR_OK) {
		PRINTF("ERROR: opening %s for output\n", outputfile);
		return 1;
	}*/

	if((decoder = FLAC__stream_decoder_new()) == NULL) {
		PRINTF("ERROR: allocating decoder\n");
		//f_close(&fout);
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
	//f_close(&fout);

	return 0;
}

__attribute__((optimize ("-O0"))) FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	FILE *f = (FILE*)client_data;
	const FLAC__uint32 total_size = (FLAC__uint32)(total_samples * channels * (bps/8));
	//size_t i;

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
	//UINT bw;
	/* write WAVE header before we write the first frame */
	if(frame->header.number.sample_number == 0) {
		if(
			//f_write("RIFF", 1, 4, f) < 4 ||
			//f_write (f, "RIFF", 4, &bw) != FR_OK ||
			!write_little_endian_uint32(f, total_size + 36) ||
			//fwrite("WAVEfmt ", 1, 8, f) < 8 ||
			//f_write(f, "WAVEfmt ", 8, &bw) != FR_OK ||
			!write_little_endian_uint32(f, 16) ||
			!write_little_endian_uint16(f, 1) ||
			!write_little_endian_uint16(f, (FLAC__uint16)channels) ||
			!write_little_endian_uint32(f, sample_rate) ||
			!write_little_endian_uint32(f, sample_rate * channels * (bps/8)) ||
			!write_little_endian_uint16(f, (FLAC__uint16)(channels * (bps/8))) || /* block align */
			!write_little_endian_uint16(f, (FLAC__uint16)bps) ||
			//fwrite("data", 1, 4, f) < 4 ||
			//f_write(f, "data", 4, &bw) != FR_OK ||
			!write_little_endian_uint32(f, total_size)
		) {
			PRINTF("ERROR: write error\n");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
	}

	/* write decoded PCM samples */
#if DECODE_NO_WRITE_FILE
	if(channels == 1){
		memcpy(dummy_buf1, &buffer[0][0], sizeof(FLAC__int16)*frame->header.blocksize);
	}
	else if(channels == 2)
	{
		memcpy(dummy_buf1, &buffer[0][0], sizeof(FLAC__int16)*frame->header.blocksize);
		memcpy(dummy_buf2, &buffer[1][0], sizeof(FLAC__int16)*frame->header.blocksize);
	}
#else
	for(i = 0; i < frame->header.blocksize; i++) {
		if(
			!write_little_endian_int16(f, (FLAC__int16)buffer[0][i]) ||  /* left channel */
			!write_little_endian_int16(f, (FLAC__int16)buffer[1][i])     /* right channel */
		) {
			PRINTF("ERROR: write error\n");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
	}
#endif
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

		//PRINTF("sample rate    : %u Hz\n", sample_rate);
		//PRINTF("channels       : %u\n", channels);
		//PRINTF("bits per sample: %u\n", bps);
		//PRINTF("total samples  : %u\n", total_samples);
	}
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;

	//PRINTF("Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}



/*!
 * @brief Main function
 */
int main(void)
{
	uint32_t testMusicNums = 0;

    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitDebugConsole();  // yulong add

    /* Initialize LED */
    LED_INIT();

    /* MUB init */
    MU_Init(APP_MU);

    /* Send flag to Core 0 to indicate Core 1 has startup */
    MU_SetFlags(APP_MU, BOOT_FLAG);

    /* Clear the g_msgRecv array before receive */
    ClearMsgRecv();

	testMusicNums = getMusicFileNumsBlockingMode();
	//xt_profile_config_clear();

	// profile branch penalty cycles
	//xt_profile_config_counter(XTPERF_CNT_BRANCH_PENALTY, XTPERF_MASK_BRANCH_PENALTY, 0, 1024);

	// profile uncached instruction fetch stalls
	//xt_profile_config_counter(XTPERF_CNT_I_STALL, 60XTPERF_MASK_I_STALL_UNCACHED_FETCH, 0, 4096);

	//xt_profile_init();
	// start profiling
	//xt_profile_enable();

	CLOCK_AttachClk(kHCLK_to_OSTIMER_CLK); // use high resolution clk src. kLPOSC_to_OSTIMER_CLK);
	OSTIMER_Init(OSTIMER1);

    uint64_t timerTicks = OSTIMER_GetCurrentTimerValue(OSTIMER1);
	PRINTF("not timerTicks is %llu\n", timerTicks);

	char inputfile[] = "dummy_input.txt";
	char outputfile[] = "dummy_output.txt";


	// Main func switch(debug use):
	// 		0: decode
	// 		1: encode

//#if 1
	uint32_t compresslevel = 0;
	uint32_t fileIter=0;
	for(fileIter=0; fileIter<testMusicNums; fileIter++)  // file num
	{
		if(CM33InitEncodeFileStructByIndex(fileIter) == TE_ACK_ERROR)  // prepare data
		{
			PRINTF("HiFi4 encode init file fail!stop running.\n");
			while(1);
		}
		for(compresslevel=0; compresslevel <= 8; compresslevel++)
		{
			virtual_fs_api_resetReadPr();			// reset pos

			PRINTF("start encoder #%u: compress level:%u\n", fileIter, compresslevel);    // start encoder
			gs_encodeStartTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER1);
			my_EncodeDemo(inputfile, outputfile, compresslevel);
			gs_encodeEndTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER1);
 		    if(gs_encodeEndTimestamp <= gs_encodeStartTimestamp)
 		    	PRINTF("something error here! start:%llu, end:%llu\r\n", gs_encodeStartTimestamp, gs_encodeEndTimestamp);

			PRINTF("encode consume time.%llu ms\n", (gs_encodeEndTimestamp-gs_encodeStartTimestamp)/300000);
		}
	}
	PRINTF("encode finish\n");
//#else
	uint32_t iter = 0, xter=0;
	for(xter=0; xter<testMusicNums; xter++)
	{
		for(iter=0; iter <= 8; iter++)
		{
			if(CM33InitDecodeFileStructByIndex(iter | xter<<4) == TE_ACK_ERROR)  // prepare data
			{
				PRINTF("HiFi4 decode init file fail! stop running.\n");
				while(1);
			}
			virtual_fs_api_resetReadPr();					  // reset pr
			PRINTF("start decoder #%u:compress level:%u\n", xter, iter);    // start decoder
			gs_encodeStartTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER1);
			libFLAC_decode(inputfile, outputfile);
			gs_encodeEndTimestamp = OSTIMER_GetCurrentTimerValue(OSTIMER1);
			PRINTF("decode consume time.%llu ms\n", (gs_encodeEndTimestamp-gs_encodeStartTimestamp)/300000);
		}
	}
	PRINTF("decode finish\n");
//#endif

	// profiling use func
	/*if(xt_profile_save_and_reset())  // Maybe take long time here.
	{
		while(1);
	}*/
	//mallinfo();
	//malloc_max_footprint();
    while (1)
    {
        delay();
        LED_TOGGLE();
    }
}
