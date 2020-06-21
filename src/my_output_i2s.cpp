/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Arduino.h>
#include "my_output_i2s.h"
#include "memcpy_audio.h"

audio_block_t * MyAudioOutputI2S::block_left_1st = NULL;
audio_block_t * MyAudioOutputI2S::block_right_1st = NULL;
audio_block_t * MyAudioOutputI2S::block_left_2nd = NULL;
audio_block_t * MyAudioOutputI2S::block_right_2nd = NULL;
uint16_t  MyAudioOutputI2S::block_left_offset = 0;
uint16_t  MyAudioOutputI2S::block_right_offset = 0;
bool MyAudioOutputI2S::update_responsibility = false;
DMAChannel MyAudioOutputI2S::dma(false);
//DMAMEM __attribute__((aligned(32))) static uint32_t i2s_tx_buffer[AUDIO_BLOCK_SAMPLES];
DMAMEM __attribute__((aligned(32))) static uint32_t i2s_tx_buffer[AUDIO_BLOCK_SAMPLES * 2]; // Audio sample 16bit -> 32bit

#if defined(__IMXRT1062__)
#include "utility/imxrt_hw.h"
#endif

uint8_t MyAudioOutputI2S::volume = 65; // 0 ~ 100;

static const uint32_t vol_table[101] = {
    0, 4, 8, 12, 16, 20, 24, 27, 29, 31, 
    34, 37, 40, 44, 48, 52, 57, 61, 67, 73, 
    79, 86, 94, 102, 111, 120, 131, 142, 155, 168, 
    183, 199, 217, 236, 256, 279, 303, 330, 359, 390, // vol_table[34] = 256;
    424, 462, 502, 546, 594, 646, 703, 764, 831, 904, 
    983, 1069, 1163, 1265, 1376, 1496, 1627, 1770, 1925, 2094, 
    2277, 2476, 2693, 2929, 3186, 3465, 3769, 4099, 4458, 4849, 
    5274, 5736, 6239, 6785, 7380, 8026, 8730, 9495, 10327, 11232, 
    12216, 13286, 14450, 15716, 17093, 18591, 20220, 21992, 23919, 26015, 
    28294, 30773, 33470, 36403, 39592, 43061, 46835, 50938, 55402, 60256, 
    65536
};

void my_memcpy_tointerleaveLR(int8_t vol, int32_t *dst, const int16_t *srcL, const int16_t *srcR)
{
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES/2; i++) {
        dst[i*2+0] = (int32_t) srcL[i] * vol_table[vol];
        dst[i*2+1] = (int32_t) srcR[i] * vol_table[vol];
    }
}

void my_memcpy_tointerleaveL(int8_t vol, int32_t *dst, const int16_t *srcL)
{
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES/2; i++) {
        dst[i*2+0] = (int32_t) srcL[i] * vol_table[vol];
        dst[i*2+1] = 0;
    }
}

void my_memcpy_tointerleaveR(int8_t vol, int32_t *dst, const int16_t *srcR)
{
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES/2; i++) {
        dst[i*2+0] = 0;
        dst[i*2+1] = (int32_t) srcR[i] * vol_table[vol];
    }
}

void MyAudioOutputI2S::begin(void)
{
	dma.begin(true); // Allocate the DMA channel first

	block_left_1st = NULL;
	block_right_1st = NULL;

	config_i2s();

#if defined(KINETISK)
	CORE_PIN22_CONFIG = PORT_PCR_MUX(6); // pin 22, PTC1, I2S0_TXD0

	dma.TCD->SADDR = i2s_tx_buffer;
	dma.TCD->SOFF = 2;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
	dma.TCD->NBYTES_MLNO = 2;
	dma.TCD->SLAST = -sizeof(i2s_tx_buffer);
	dma.TCD->DADDR = (void *)((uint32_t)&I2S0_TDR0 + 2);
	dma.TCD->DOFF = 0;
	dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
	dma.TCD->DLASTSGA = 0;
	dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_TX);
	dma.enable();

	I2S0_TCSR = I2S_TCSR_SR;
	I2S0_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;

#elif defined(__IMXRT1062__)
	CORE_PIN7_CONFIG  = 3;  //1:TX_DATA0
	dma.TCD->SADDR = i2s_tx_buffer;
	//dma.TCD->SOFF = 2;
	dma.TCD->SOFF = 4; // Audio sample 16bit -> 32bit
	//dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(DMA_TCD_ATTR_SIZE_32BIT) | DMA_TCD_ATTR_DSIZE(DMA_TCD_ATTR_SIZE_32BIT); // Audio sample 16bit -> 32bit
	//dma.TCD->NBYTES_MLNO = 2;
	dma.TCD->NBYTES_MLNO = 4; // Audio sample 16bit -> 32bit
	dma.TCD->SLAST = -sizeof(i2s_tx_buffer);
	dma.TCD->DOFF = 0;
	//dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
	dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 4; // Audio sample 16bit -> 32bit
	dma.TCD->DLASTSGA = 0;
	//dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
	dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 4; // Audio sample 16bit -> 32bit
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	//dma.TCD->DADDR = (void *)((uint32_t)&I2S1_TDR0 + 2);
	dma.TCD->DADDR = (void *)((uint32_t)&I2S1_TDR0 + 0); // Audio sample 16bit -> 32bit
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_TX);
	dma.enable();

	I2S1_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE;
	I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;
#endif
	update_responsibility = update_setup();
	dma.attachInterrupt(isr);
}

void MyAudioOutputI2S::isr(void)
{
#if defined(KINETISK) || defined(__IMXRT1062__)
	// int16_t *dest;
	int32_t *dest; // Audio sample 16bit -> 32bit
	audio_block_t *blockL, *blockR;
	uint32_t saddr, offsetL, offsetR;

	saddr = (uint32_t)(dma.TCD->SADDR);
	dma.clearInterrupt();
	if (saddr < (uint32_t)i2s_tx_buffer + sizeof(i2s_tx_buffer) / 2) {
		// DMA is transmitting the first half of the buffer
		// so we must fill the second half
		//dest = (int16_t *)&i2s_tx_buffer[AUDIO_BLOCK_SAMPLES/2];
		dest = (int32_t *)&i2s_tx_buffer[AUDIO_BLOCK_SAMPLES]; // Audio sample 16bit -> 32bit
		if (MyAudioOutputI2S::update_responsibility) AudioStream::update_all();
	} else {
		// DMA is transmitting the second half of the buffer
		// so we must fill the first half
		//dest = (int16_t *)i2s_tx_buffer;
		dest = (int32_t *)i2s_tx_buffer; // Audio sample 16bit -> 32bit
	}

	blockL = MyAudioOutputI2S::block_left_1st;
	blockR = MyAudioOutputI2S::block_right_1st;
	offsetL = MyAudioOutputI2S::block_left_offset;
	offsetR = MyAudioOutputI2S::block_right_offset;

	if (blockL && blockR) {
		//memcpy_tointerleaveLR(dest, blockL->data + offsetL, blockR->data + offsetR);
		my_memcpy_tointerleaveLR(volume, dest, blockL->data + offsetL, blockR->data + offsetR); // Audio sample 16bit -> 32bit
		offsetL += AUDIO_BLOCK_SAMPLES / 2;
		offsetR += AUDIO_BLOCK_SAMPLES / 2;
	} else if (blockL) {
		//memcpy_tointerleaveL(dest, blockL->data + offsetL);
		my_memcpy_tointerleaveL(volume, dest, blockL->data + offsetL); // Audio sample 16bit -> 32bit
		offsetL += AUDIO_BLOCK_SAMPLES / 2;
	} else if (blockR) {
		//memcpy_tointerleaveR(dest, blockR->data + offsetR);
		my_memcpy_tointerleaveR(volume, dest, blockR->data + offsetR); // Audio sample 16bit -> 32bit
		offsetR += AUDIO_BLOCK_SAMPLES / 2;
	} else {
		//memset(dest,0,AUDIO_BLOCK_SAMPLES * 2);
		memset(dest,0,AUDIO_BLOCK_SAMPLES * 4); // Audio sample 16bit -> 32bit
	}

	arm_dcache_flush_delete(dest, sizeof(i2s_tx_buffer) / 2);

	if (offsetL < AUDIO_BLOCK_SAMPLES) {
		MyAudioOutputI2S::block_left_offset = offsetL;
	} else {
		MyAudioOutputI2S::block_left_offset = 0;
		AudioStream::release(blockL);
		MyAudioOutputI2S::block_left_1st = MyAudioOutputI2S::block_left_2nd;
		MyAudioOutputI2S::block_left_2nd = NULL;
	}
	if (offsetR < AUDIO_BLOCK_SAMPLES) {
		MyAudioOutputI2S::block_right_offset = offsetR;
	} else {
		MyAudioOutputI2S::block_right_offset = 0;
		AudioStream::release(blockR);
		MyAudioOutputI2S::block_right_1st = MyAudioOutputI2S::block_right_2nd;
		MyAudioOutputI2S::block_right_2nd = NULL;
	}
#else
	const int16_t *src, *end;
	int16_t *dest;
	audio_block_t *block;
	uint32_t saddr, offset;

	saddr = (uint32_t)(dma.CFG->SAR);
	dma.clearInterrupt();
	if (saddr < (uint32_t)i2s_tx_buffer + sizeof(i2s_tx_buffer) / 2) {
		// DMA is transmitting the first half of the buffer
		// so we must fill the second half
		dest = (int16_t *)&i2s_tx_buffer[AUDIO_BLOCK_SAMPLES/2];
		end = (int16_t *)&i2s_tx_buffer[AUDIO_BLOCK_SAMPLES];
		if (MyAudioOutputI2S::update_responsibility) AudioStream::update_all();
	} else {
		// DMA is transmitting the second half of the buffer
		// so we must fill the first half
		dest = (int16_t *)i2s_tx_buffer;
		end = (int16_t *)&i2s_tx_buffer[AUDIO_BLOCK_SAMPLES/2];
	}

	block = MyAudioOutputI2S::block_left_1st;
	if (block) {
		offset = MyAudioOutputI2S::block_left_offset;
		src = &block->data[offset];
		do {
			*dest = *src++;
			dest += 2;
		} while (dest < end);
		offset += AUDIO_BLOCK_SAMPLES/2;
		if (offset < AUDIO_BLOCK_SAMPLES) {
			MyAudioOutputI2S::block_left_offset = offset;
		} else {
			MyAudioOutputI2S::block_left_offset = 0;
			AudioStream::release(block);
			MyAudioOutputI2S::block_left_1st = MyAudioOutputI2S::block_left_2nd;
			MyAudioOutputI2S::block_left_2nd = NULL;
		}
	} else {
		do {
			*dest = 0;
			dest += 2;
		} while (dest < end);
	}
	dest -= AUDIO_BLOCK_SAMPLES - 1;
	block = MyAudioOutputI2S::block_right_1st;
	if (block) {
		offset = MyAudioOutputI2S::block_right_offset;
		src = &block->data[offset];
		do {
			*dest = *src++;
			dest += 2;
		} while (dest < end);
		offset += AUDIO_BLOCK_SAMPLES/2;
		if (offset < AUDIO_BLOCK_SAMPLES) {
			MyAudioOutputI2S::block_right_offset = offset;
		} else {
			MyAudioOutputI2S::block_right_offset = 0;
			AudioStream::release(block);
			MyAudioOutputI2S::block_right_1st = MyAudioOutputI2S::block_right_2nd;
			MyAudioOutputI2S::block_right_2nd = NULL;
		}
	} else {
		do {
			*dest = 0;
			dest += 2;
		} while (dest < end);
	}
#endif
}

void MyAudioOutputI2S::update(void)
{
	// null audio device: discard all incoming data
	//if (!active) return;
	//audio_block_t *block = receiveReadOnly();
	//if (block) release(block);

	audio_block_t *block;
	block = receiveReadOnly(0); // input 0 = left channel
	if (block) {
		__disable_irq();
		if (block_left_1st == NULL) {
			block_left_1st = block;
			block_left_offset = 0;
			__enable_irq();
		} else if (block_left_2nd == NULL) {
			block_left_2nd = block;
			__enable_irq();
		} else {
			audio_block_t *tmp = block_left_1st;
			block_left_1st = block_left_2nd;
			block_left_2nd = block;
			block_left_offset = 0;
			__enable_irq();
			release(tmp);
		}
	}
	block = receiveReadOnly(1); // input 1 = right channel
	if (block) {
		__disable_irq();
		if (block_right_1st == NULL) {
			block_right_1st = block;
			block_right_offset = 0;
			__enable_irq();
		} else if (block_right_2nd == NULL) {
			block_right_2nd = block;
			__enable_irq();
		} else {
			audio_block_t *tmp = block_right_1st;
			block_right_1st = block_right_2nd;
			block_right_2nd = block;
			block_right_offset = 0;
			__enable_irq();
			release(tmp);
		}
	}
}

void MyAudioOutputI2S::volume_up(void)
{
    if (volume < 100) volume++;
}

void MyAudioOutputI2S::volume_down(void)
{
    if (volume > 0) volume--;
}

#if defined(KINETISK) || defined(KINETISL)
// MCLK needs to be 48e6 / 1088 * 256 = 11.29411765 MHz -> 44.117647 kHz sample rate
//
#if F_CPU == 96000000 || F_CPU == 48000000 || F_CPU == 24000000
  // PLL is at 96 MHz in these modes
  #define MCLK_MULT 2
  #define MCLK_DIV  17
#elif F_CPU == 72000000
  #define MCLK_MULT 8
  #define MCLK_DIV  51
#elif F_CPU == 120000000
  #define MCLK_MULT 8
  #define MCLK_DIV  85
#elif F_CPU == 144000000
  #define MCLK_MULT 4
  #define MCLK_DIV  51
#elif F_CPU == 168000000
  #define MCLK_MULT 8
  #define MCLK_DIV  119
#elif F_CPU == 180000000
  #define MCLK_MULT 16
  #define MCLK_DIV  255
  #define MCLK_SRC  0
#elif F_CPU == 192000000
  #define MCLK_MULT 1
  #define MCLK_DIV  17
#elif F_CPU == 216000000
  #define MCLK_MULT 12
  #define MCLK_DIV  17
  #define MCLK_SRC  1
#elif F_CPU == 240000000
  #define MCLK_MULT 2
  #define MCLK_DIV  85
  #define MCLK_SRC  0
#elif F_CPU == 256000000
  #define MCLK_MULT 12
  #define MCLK_DIV  17
  #define MCLK_SRC  1
#elif F_CPU == 16000000
  #define MCLK_MULT 12
  #define MCLK_DIV  17
#else
  #error "This CPU Clock Speed is not supported by the Audio library";
#endif

#ifndef MCLK_SRC
#if F_CPU >= 20000000
  #define MCLK_SRC  3  // the PLL
#else
  #define MCLK_SRC  0  // system clock
#endif
#endif
#endif


void MyAudioOutputI2S::config_i2s(void)
{
#if defined(KINETISK) || defined(KINETISL)
	SIM_SCGC6 |= SIM_SCGC6_I2S;
	SIM_SCGC7 |= SIM_SCGC7_DMA;
	SIM_SCGC6 |= SIM_SCGC6_DMAMUX;

	// if either transmitter or receiver is enabled, do nothing
	if (I2S0_TCSR & I2S_TCSR_TE) return;
	if (I2S0_RCSR & I2S_RCSR_RE) return;

	// enable MCLK output
	I2S0_MCR = I2S_MCR_MICS(MCLK_SRC) | I2S_MCR_MOE;
	while (I2S0_MCR & I2S_MCR_DUF) ;
	I2S0_MDR = I2S_MDR_FRACT((MCLK_MULT-1)) | I2S_MDR_DIVIDE((MCLK_DIV-1));

	// configure transmitter
	I2S0_TMR = 0;
	I2S0_TCR1 = I2S_TCR1_TFW(1);  // watermark at half fifo size
	I2S0_TCR2 = I2S_TCR2_SYNC(0) | I2S_TCR2_BCP | I2S_TCR2_MSEL(1)
		| I2S_TCR2_BCD | I2S_TCR2_DIV(1);
	I2S0_TCR3 = I2S_TCR3_TCE;
	I2S0_TCR4 = I2S_TCR4_FRSZ(1) | I2S_TCR4_SYWD(31) | I2S_TCR4_MF
		| I2S_TCR4_FSE | I2S_TCR4_FSP | I2S_TCR4_FSD;
	I2S0_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

	// configure receiver (sync'd to transmitter clocks)
	I2S0_RMR = 0;
	I2S0_RCR1 = I2S_RCR1_RFW(1);
	I2S0_RCR2 = I2S_RCR2_SYNC(1) | I2S_TCR2_BCP | I2S_RCR2_MSEL(1)
		| I2S_RCR2_BCD | I2S_RCR2_DIV(1);
	I2S0_RCR3 = I2S_RCR3_RCE;
	I2S0_RCR4 = I2S_RCR4_FRSZ(1) | I2S_RCR4_SYWD(31) | I2S_RCR4_MF
		| I2S_RCR4_FSE | I2S_RCR4_FSP | I2S_RCR4_FSD;
	I2S0_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

	// configure pin mux for 3 clock signals
	CORE_PIN23_CONFIG = PORT_PCR_MUX(6); // pin 23, PTC2, I2S0_TX_FS (LRCLK)
	CORE_PIN9_CONFIG  = PORT_PCR_MUX(6); // pin  9, PTC3, I2S0_TX_BCLK
	CORE_PIN11_CONFIG = PORT_PCR_MUX(6); // pin 11, PTC6, I2S0_MCLK

#elif defined(__IMXRT1062__)

	CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);

	// if either transmitter or receiver is enabled, do nothing
	if (I2S1_TCSR & I2S_TCSR_TE) return;
	if (I2S1_RCSR & I2S_RCSR_RE) return;
//PLL:
	int fs = AUDIO_SAMPLE_RATE_EXACT;
	// PLL between 27*24 = 648MHz und 54*24=1296MHz
	int n1 = 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
	int n2 = 1 + (24000000 * 27) / (fs * 256 * n1);

	double C = ((double)fs * 256 * n1 * n2) / 24000000;
	int c0 = C;
	int c2 = 10000;
	int c1 = C * c2 - (c0 * c2);
	set_audioClock(c0, c1, c2);

	// clear SAI1_CLK register locations
	CCM_CSCMR1 = (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI1_CLK_SEL_MASK))
		   | CCM_CSCMR1_SAI1_CLK_SEL(2); // &0x03 // (0,1,2): PLL3PFD0, PLL5, PLL4
	CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
		   | CCM_CS1CDR_SAI1_CLK_PRED(n1-1) // &0x07
		   | CCM_CS1CDR_SAI1_CLK_PODF(n2-1); // &0x3f

	// Select MCLK
	IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1
		& ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK))
		| (IOMUXC_GPR_GPR1_SAI1_MCLK_DIR | IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(0));

	CORE_PIN23_CONFIG = 3;  //1:MCLK
	CORE_PIN21_CONFIG = 3;  //1:RX_BCLK
	CORE_PIN20_CONFIG = 3;  //1:RX_SYNC

	int rsync = 0;
	int tsync = 1;

	I2S1_TMR = 0;
	//I2S1_TCSR = (1<<25); //Reset
	I2S1_TCR1 = I2S_TCR1_RFW(1);
	I2S1_TCR2 = I2S_TCR2_SYNC(tsync) | I2S_TCR2_BCP // sync=0; tx is async;
		    | (I2S_TCR2_BCD | I2S_TCR2_DIV((1)) | I2S_TCR2_MSEL(1));
	I2S1_TCR3 = I2S_TCR3_TCE;
	I2S1_TCR4 = I2S_TCR4_FRSZ((2-1)) | I2S_TCR4_SYWD((32-1)) | I2S_TCR4_MF
		    | I2S_TCR4_FSD | I2S_TCR4_FSE | I2S_TCR4_FSP;
	I2S1_TCR5 = I2S_TCR5_WNW((32-1)) | I2S_TCR5_W0W((32-1)) | I2S_TCR5_FBT((32-1));

	I2S1_RMR = 0;
	//I2S1_RCSR = (1<<25); //Reset
	I2S1_RCR1 = I2S_RCR1_RFW(1);
	I2S1_RCR2 = I2S_RCR2_SYNC(rsync) | I2S_RCR2_BCP  // sync=0; rx is async;
		    | (I2S_RCR2_BCD | I2S_RCR2_DIV((1)) | I2S_RCR2_MSEL(1));
	I2S1_RCR3 = I2S_RCR3_RCE;
	I2S1_RCR4 = I2S_RCR4_FRSZ((2-1)) | I2S_RCR4_SYWD((32-1)) | I2S_RCR4_MF
		    | I2S_RCR4_FSE | I2S_RCR4_FSP | I2S_RCR4_FSD;
	I2S1_RCR5 = I2S_RCR5_WNW((32-1)) | I2S_RCR5_W0W((32-1)) | I2S_RCR5_FBT((32-1));

#endif
}


/******************************************************************/

void MyAudioOutputI2Sslave::begin(void)
{

	dma.begin(true); // Allocate the DMA channel first

	block_left_1st = NULL;
	block_right_1st = NULL;

	MyAudioOutputI2Sslave::config_i2s();

#if defined(KINETISK)
	CORE_PIN22_CONFIG = PORT_PCR_MUX(6); // pin 22, PTC1, I2S0_TXD0
	dma.TCD->SADDR = i2s_tx_buffer;
	dma.TCD->SOFF = 2;
	dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
	dma.TCD->NBYTES_MLNO = 2;
	dma.TCD->SLAST = -sizeof(i2s_tx_buffer);
	dma.TCD->DADDR = (void *)((uint32_t)&I2S0_TDR0 + 2);
	dma.TCD->DOFF = 0;
	dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
	dma.TCD->DLASTSGA = 0;
	dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_TX);
	dma.enable();

	I2S0_TCSR = I2S_TCSR_SR;
	I2S0_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;

#elif defined(__IMXRT1062__)
	CORE_PIN7_CONFIG  = 3;  //1:TX_DATA0
	dma.TCD->SADDR = i2s_tx_buffer;
	//dma.TCD->SOFF = 2;
	dma.TCD->SOFF = 4; // Audio sample 16bit -> 32bit
	//dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(DMA_TCD_ATTR_SIZE_32BIT) | DMA_TCD_ATTR_DSIZE(DMA_TCD_ATTR_SIZE_32BIT); // Audio sample 16bit -> 32bit
	//dma.TCD->NBYTES_MLNO = 2;
	dma.TCD->NBYTES_MLNO = 4; // Audio sample 16bit -> 32bit
	dma.TCD->SLAST = -sizeof(i2s_tx_buffer);
	dma.TCD->DOFF = 0;
	//dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
	dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 4; // Audio sample 16bit -> 32bit
	dma.TCD->DLASTSGA = 0;
	//dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
	dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 4; // Audio sample 16bit -> 32bit
	//dma.TCD->DADDR = (void *)((uint32_t)&I2S1_TDR0 + 2);
	dma.TCD->DADDR = (void *)((uint32_t)&I2S1_TDR0 + 0); // Audio sample 16bit -> 32bit
	dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
	dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_TX);
	dma.enable();

	I2S1_RCSR |= I2S_RCSR_RE | I2S_RCSR_BCE;
	I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;
#endif

	update_responsibility = update_setup();
	dma.attachInterrupt(isr);
}

void MyAudioOutputI2Sslave::config_i2s(void)
{
#if defined(KINETISK)
	SIM_SCGC6 |= SIM_SCGC6_I2S;
	SIM_SCGC7 |= SIM_SCGC7_DMA;
	SIM_SCGC6 |= SIM_SCGC6_DMAMUX;

	// if either transmitter or receiver is enabled, do nothing
	if (I2S0_TCSR & I2S_TCSR_TE) return;
	if (I2S0_RCSR & I2S_RCSR_RE) return;

	// Select input clock 0
	// Configure to input the bit-clock from pin, bypasses the MCLK divider
	I2S0_MCR = I2S_MCR_MICS(0);
	I2S0_MDR = 0;

	// configure transmitter
	I2S0_TMR = 0;
	I2S0_TCR1 = I2S_TCR1_TFW(1);  // watermark at half fifo size
	I2S0_TCR2 = I2S_TCR2_SYNC(0) | I2S_TCR2_BCP;

	I2S0_TCR3 = I2S_TCR3_TCE;
	I2S0_TCR4 = I2S_TCR4_FRSZ(1) | I2S_TCR4_SYWD(31) | I2S_TCR4_MF
		| I2S_TCR4_FSE | I2S_TCR4_FSP;

	I2S0_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

	// configure receiver (sync'd to transmitter clocks)
	I2S0_RMR = 0;
	I2S0_RCR1 = I2S_RCR1_RFW(1);
	I2S0_RCR2 = I2S_RCR2_SYNC(1) | I2S_TCR2_BCP;

	I2S0_RCR3 = I2S_RCR3_RCE;
	I2S0_RCR4 = I2S_RCR4_FRSZ(1) | I2S_RCR4_SYWD(31) | I2S_RCR4_MF
		| I2S_RCR4_FSE | I2S_RCR4_FSP | I2S_RCR4_FSD;

	I2S0_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

	// configure pin mux for 3 clock signals
	CORE_PIN23_CONFIG = PORT_PCR_MUX(6); // pin 23, PTC2, I2S0_TX_FS (LRCLK)
	CORE_PIN9_CONFIG  = PORT_PCR_MUX(6); // pin  9, PTC3, I2S0_TX_BCLK
	CORE_PIN11_CONFIG = PORT_PCR_MUX(6); // pin 11, PTC6, I2S0_MCLK

#elif defined(__IMXRT1062__)

	CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);

	// if either transmitter or receiver is enabled, do nothing
	if (I2S1_TCSR & I2S_TCSR_TE) return;
	if (I2S1_RCSR & I2S_RCSR_RE) return;

	// not using MCLK in slave mode - hope that's ok?
	//CORE_PIN23_CONFIG = 3;  // AD_B1_09  ALT3=SAI1_MCLK
	CORE_PIN21_CONFIG = 3;  // AD_B1_11  ALT3=SAI1_RX_BCLK
	CORE_PIN20_CONFIG = 3;  // AD_B1_10  ALT3=SAI1_RX_SYNC
	IOMUXC_SAI1_RX_BCLK_SELECT_INPUT = 1; // 1=GPIO_AD_B1_11_ALT3, page 868
	IOMUXC_SAI1_RX_SYNC_SELECT_INPUT = 1; // 1=GPIO_AD_B1_10_ALT3, page 872

	// configure transmitter
	I2S1_TMR = 0;
	I2S1_TCR1 = I2S_TCR1_RFW(1);  // watermark at half fifo size
	I2S1_TCR2 = I2S_TCR2_SYNC(1) | I2S_TCR2_BCP;
	I2S1_TCR3 = I2S_TCR3_TCE;
	I2S1_TCR4 = I2S_TCR4_FRSZ(1) | I2S_TCR4_SYWD(31) 
        | I2S_TCR4_MF | I2S_TCR4_FSE | I2S_TCR4_FSP | I2S_RCR4_FSD;
	I2S1_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

	// configure receiver
	I2S1_RMR = 0;
	I2S1_RCR1 = I2S_RCR1_RFW(1);
	I2S1_RCR2 = I2S_RCR2_SYNC(0) | I2S_TCR2_BCP;
	I2S1_RCR3 = I2S_RCR3_RCE;
	I2S1_RCR4 = I2S_RCR4_FRSZ(1) | I2S_RCR4_SYWD(31) | I2S_RCR4_MF
		| I2S_RCR4_FSE | I2S_RCR4_FSP;
	I2S1_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

#endif
}
