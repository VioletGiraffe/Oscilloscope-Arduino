#include <arduino-utils.hpp>

#include <Arduino.h>
#include <SPI.h>

#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>

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

force_inline void setupDisplay()
{
	tft.reset();
	tft.begin(tft.readID());
	tft.fillScreen(YELLOW);

	tft.fillRect(10, 10, 50, 100, BLACK);
}

force_inline void setupADC()
{
	pmc_enable_periph_clk(ID_ADC); // To use peripheral, we must enable clock distributon to it

	adc_init(ADC, F_CPU, ADC_FREQ_MAX, ADC_STARTUP_NORM);
}

// This local loop function should theoretically loop quicker than the standard Arduino one
force_inline void fastLoop()
{

}

void setup()
{
	setupADC();
	setupDisplay();

	for(;;)
		fastLoop();
}