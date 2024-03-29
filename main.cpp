#include <arduino-utils.hpp>
#include "sdcard.h"
#include "ADC/adchandler.hpp"

#include <Arduino.h>
#include <SPI.h>

//#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h>

#undef min
#undef max
#include <algorithm>
#include <array>
#include <limits>
#include <stdint.h>

// It has to be defined to avoid linker error 
void loop() {}

//
// Touch screen
//
constexpr int XP=8,XM=A2,YP=A3,YM=9; //ID=0x9341
constexpr int TS_LEFT=120,TS_RT=900,TS_TOP=70,TS_BOT=920;

 //MCUFRIEND_kbv tft;
// TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

auto tft = Adafruit_ST7735(10, 9, -1);

#define MINPRESSURE 200
#define MAXPRESSURE 1000

#define COLOR_ORDER_BGR

#ifndef COLOR_ORDER_BGR
template<uint16_t R, uint16_t G, uint16_t B>
constexpr uint16_t color = ((R & 0xF8u) << 8) | ((G & 0xFCu) << 3) | (B >> 3u);
#else
template<uint16_t R, uint16_t G, uint16_t B>
constexpr uint16_t color = ((B & 0xF8u) << 8) | ((G & 0xFCu) << 3) | (R >> 3u);
#endif

//
// ADC
//

AdcHandler adcHandler;

force_inline void setupDisplay()
{
	tft.initR(INITR_MINI160x80);  // Init ST7735S mini display
	tft.setRotation(3);

	// MCUFriend setup code
	// tft.reset();
	// Serial.println(tft.readID(), 16);
	// tft.begin(tft.readID());
	// tft.fillScreen(YELLOW);
}

force_inline void setupADC()
{
	// Setting ADC interrupt priority to 0, all others to 1 so that ADC always has priority
	NVIC_SetPriority(ADC_IRQn, 0);

	NVIC_SetPriority(UART_IRQn, 1);
	NVIC_SetPriority(USART0_IRQn, 1);
	NVIC_SetPriority(USART1_IRQn, 1);
	NVIC_SetPriority(USART2_IRQn, 1);
	NVIC_SetPriority(USART3_IRQn, 1);
	NVIC_SetPriority(SSC_IRQn, 1);
	NVIC_SetPriority(DMAC_IRQn, 1);
	NVIC_SetPriority(SPI0_IRQn, 1);

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
	adc_set_bias_current(ADC, 1); //  Bias Current - allows to adapt performance versus power consumption. Recommended '1' for >=500 KSps.
	
	adc_stop_sequencer(ADC); // not using it
	adc_disable_tag(ADC); // it has to do with sequencer, not using it
	adc_disable_ts(ADC); // disable temperature sensor

	adc_disable_channel_differential_input(ADC, ADC_CHANNEL_0);
	adc_configure_trigger(ADC, ADC_TRIG_SW, 1); // triggering from software, freerunning mode
	adc_disable_all_channel(ADC);
	adc_enable_channel(ADC, ADC_CHANNEL_0); // just one channel enabled
	adc_enable_interrupt(ADC, ADC_IER_DRDY);
	NVIC_EnableIRQ(ADC_IRQn);
	adc_start(ADC); // This call does nothing, as apparently on Due one of the above setup functions also causes ADC to start
}

void ADC_Handler()
{
	//Get latest digital data value from ADC
	adcHandler.handleValue(ADC->ADC_LCDR);
}

// This local loop function should theoretically loop quicker than the standard Arduino one
force_inline void fastLoop()
{
	if (!adcHandler.bufferReady())
		return;
	
	//const auto start = millis();

	//tft.fillScreen(0);

	const auto samples = adcHandler.completedBuffer();
	for (int16_t i = 0; i < 160; ++i)
	{
		const auto sample = samples[(uint32_t)i * adcHandler.bufferLength / 160u];
		//max = std::max(max, sample);
		//min = std::min(min, sample);

		tft.drawPixel(i, (uint32_t)sample * 80u / 4096u, color<255, 128, 0>);
	}

	//Serial.println(millis() - start);

	// Serial.print(min);
	// Serial.print(' ');
	// Serial.println(max);
}

void setup()
{
	Serial.begin(115200);

	setupADC();
	setupDisplay();

//	runSdBenchmark();

	for(;;)
		fastLoop();
}