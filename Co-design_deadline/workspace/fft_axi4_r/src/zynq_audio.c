/*
 * Copyright (c) 2009-2012 Xilinx, Inc.  All rights reserved.
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "audio.h"
#include "oled.h"
#include "sleep.h"
#include <stdlib.h>
#include "xparameters.h"
#include "xbasic_types.h"
#include "xstatus.h"
#include "fft_axi4_r.h"
//void print(char *str);

#include <math.h>
#define timer_base 0xF8F00000
#define rounds 2000
/***********************************************************
Timer Registers
************************************************************/
static volatile int *timer_counter_l=(volatile int *)(timer_base+0x200);
static volatile int *timer_counter_h=(volatile int *)(timer_base+0x204);
static volatile int *timer_ctrl=(volatile int *)(timer_base+0x208);

int main()
{
	Xint16 audio_data[128];
	unsigned int *window_buffer=(unsigned int *)calloc(128*8, sizeof(unsigned int));
	unsigned int *noise_buf=(unsigned int *)calloc(128, sizeof(unsigned int));
	unsigned int *mag_buf=( unsigned int *)calloc(128, sizeof(unsigned int));
	u8 *avg_buffer=(u8 *)calloc(128, sizeof(u8));
	Xil_Out32(OLED_BASE_ADDR,0xff);
	OLED_Init();			//oled init
	IicConfig(XPAR_XIICPS_0_DEVICE_ID);
	AudioPllConfig(); //enable core clock for ADAU1761
	AudioConfigure();
	 //Initialise the timer for performance monitoring
	init_timer(timer_ctrl, timer_counter_l, timer_counter_h);
	Xuint32 baseaddr = 0x6C200000 ;
	xil_printf("ADAU1761 configured\n\r");

	/*
	 * perform continuous read and writes from the codec that result in a loopback
	 * from Line in to Line out
	 */
	register unsigned int noiseDeterminePhase = 1;
	register unsigned int  noiseRounds = 0;
	FFT_AXI4_R_mWriteMemory(baseaddr+4, 0);
	while(1)
	{
		register unsigned int  j, i;

		if(noiseRounds >= rounds && noiseDeterminePhase){
			noiseDeterminePhase = 0;
		}
		else{
			noiseRounds++;
		}
		FFT_AXI4_R_mWriteMemory(baseaddr+4, 1);
		get_audio(audio_data);
		if(!noiseDeterminePhase){
			start_timer(timer_ctrl);
		}
	/**********************************************	 FPGA WRITE LOGIIC *************************************************************************/
		while(!	FFT_AXI4_R_mReadMemory(baseaddr+12));
		for (i=0; i<=126; i=i+2){
					FFT_AXI4_R_mWriteMemory(baseaddr, (audio_data[i]<<16 | (audio_data[i+1]&0xffff) ));
				}
	/**********************************************	 Poling for data availability *************************************************************************/
		while(!	FFT_AXI4_R_mReadMemory(baseaddr+8));
		j = 0;
	/**********************************************	 FPGA read logic *************************************************************************/	
			for (j=0; j<64; j=j+1){
				if(j==0){
					mag_buf[j]   =  FFT_AXI4_R_mReadMemory(baseaddr);
				}
				else {
					mag_buf[j]   =  FFT_AXI4_R_mReadMemory(baseaddr);
					mag_buf[128-j]   =  mag_buf[j];
				}
			}

			magnitude(mag_buf, window_buffer, 128);
			averaging(window_buffer,avg_buffer, 128, noiseDeterminePhase,noise_buf);
			FFT_AXI4_R_mWriteMemory(baseaddr+4, 0);
  		  	if(!noiseDeterminePhase){
				stop_timer(timer_ctrl);
				xil_printf("Communication time %d us\n\r", (*timer_counter_l)/333);
				OLED_Clear();
				OLED_Equalizer_128(avg_buffer);
  		  	}
	}
    return 0;
}
