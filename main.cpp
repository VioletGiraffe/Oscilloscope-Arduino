#include <arduino-utils.hpp>
#include "sdcard.h"

#include <Arduino.h>
#include <SPI.h>

#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>

#undef min
#undef max
#include <algorithm>
#include <limits>
#include <stdint.h>

// It has to be defined to avoid linker error 
void loop() {}

constexpr int XP=8,XM=A2,YP=A3,YM=9; //ID=0x9341
constexpr int TS_LEFT=120,TS_RT=900,TS_TOP=70,TS_BOT=920;

MCUFRIEND_kbv tft;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

#define MINPRESSURE 200
#define MAXPRESSURE 1000

template<uint16_t R, uint16_t G, uint16_t B>
constexpr uint16_t color = ((R & 0xF8u) << 8) | ((G & 0xFCu) << 3) | (B >> 3u);

constexpr auto BLACK = color<0, 0, 0>;
constexpr auto YELLOW = color<255, 128, 0>;

constexpr uint32_t BUFFER_SIZE = 1000;
uint16_t buf[BUFFER_SIZE];

force_inline void setupDisplay()
{
	tft.reset();
	tft.begin(tft.readID());
	tft.fillScreen(YELLOW);

	tft.fillRect(10, 10, 50, 100, BLACK);
}

force_inline void setupADC()
{
	pinMode(A7, INPUT);
	pmc_enable_periph_clk(ID_ADC); // To use peripheral, we must enable clock distributon to it

	// 21 MHz = F_CPU (84 MHz) / 4, prescaler = 1. Normal startup is acceptable at this frequency.
	// This is below the maximum of 22 MHz as per datasheet.
	adc_init(ADC, F_CPU, 21000000UL, ADC_STARTUP_NORM);
	adc_disable_interrupt(ADC, 0xFFFFFFFF); // TODO: why is this needed?
	adc_disable_anch(ADC);
	adc_set_resolution(ADC, ADC_12_BITS); // Should be 12 by default after reset, but better be prudent.
	adc_configure_power_save(ADC, 0, 0); // Disable sleep
	adc_configure_timing(ADC, 0, ADC_SETTLING_TIME_3, 1); // Set timings - standard values
	adc_set_channel_input_gain(ADC, ADC_CHANNEL_0, ADC_GAINVALUE_1);
	adc_disable_channel_input_offset(ADC, ADC_CHANNEL_0);
	//adc_set_bias_current(ADC, 1); // Bias current - maximum performance over current consumption - This register can only be written if the WPEN bit is cleared in “ADC Write Protect Mode Register” on page 1353.
	
	adc_stop_sequencer(ADC); // not using it
	adc_disable_tag(ADC); // it has to do with sequencer, not using it
	adc_disable_ts(ADC); // disable temperature sensor

	adc_disable_channel_differential_input(ADC, ADC_CHANNEL_0);
	adc_configure_trigger(ADC, ADC_TRIG_SW, 1); // triggering from software, freerunning mode
	adc_disable_all_channel(ADC);
	adc_enable_channel(ADC, ADC_CHANNEL_0); // just one channel enabled
	adc_start(ADC);
}

uint16_t adcSamplesBuffer1[2048], adcSamplesBuffer2[2048];

void cam_dma_start()
{
	int x;
	long val;

	// hard coding bit positions, number channels etc for clarity.
	// (1) find a free channel
	val = DMAC->DMAC_CHSR;
	for (x = 5; (val & (1 << x)) > 0 && x >= 0; x--);


	if (x >= 0)
	{
		// got a channel
		val = DMAC->DMAC_EBCISR;						   // (2) clear any pending interrupts
		DMAC->DMAC_CH_NUM[x].DMAC_SADDR = REG_ADC_LCDR;   // (3a) Source Addr = ADC
		DMAC->DMAC_CH_NUM[x].DMAC_DADDR = (RwReg)adcSamplesBuffer1; // (3b) Destination addr (buffer)
		DMAC->DMAC_CH_NUM[x].DMAC_DSCR = 0;				   // (3c) Next descriptor, not needed
		// (3d) Program DMAC_CTRLAx, DMAC_CTRLBx and DMAC_CFGx
		DMAC->DMAC_CH_NUM[x].DMAC_CTRLA = MAX_FRAME;			 // full buffer,single rd/wr,byte rd/wr,not done
		DMAC->DMAC_CH_NUM[x].DMAC_CTRLB = (2 << 21) | (2 << 24); // Periph to mem FC,Fix src addr
		DMAC->DMAC_CH_NUM[x].DMAC_CFG =
	}
}

// This local loop function should theoretically loop quicker than the standard Arduino one
force_inline void fastLoop()
{
	// uint16_t max = 0, min = std::numeric_limits<uint16_t>::max();

	// for (uint32_t i = 0; i < BUFFER_SIZE; i++)
	// {
	// 	max = std::max(max, buf[i]);
	// 	min = std::min(min, buf[i]);
	// }

	// Serial.println(String(min) + ' ' + max);
}

void setup()
{
	setupADC();
	setupDisplay();

	Serial.begin(115200);

	for(;;)
		fastLoop();
}