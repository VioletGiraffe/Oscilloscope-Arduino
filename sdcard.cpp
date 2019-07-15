#include "sdcard.h"
#include <arduino-utils.hpp>

#include <SPI.h>
#include "SdFat.h"
#include "sdios.h"

// SD chip select pin
constexpr uint8_t chipSelect = 10;

#define SD_CONFIG SdSpiConfig(chipSelect, SHARED_SPI)

// Set PRE_ALLOCATE true to pre-allocate file clusters.
constexpr bool PRE_ALLOCATE = false;

// Set SKIP_FIRST_LATENCY true if the first read/write to the SD can
// be avoid by writing a file header or reading the first record.
constexpr bool SKIP_FIRST_LATENCY = false;

//------------------------------------------------------------------------------
// Store error strings in flash to save RAM.
#define error(s) sd.errorHalt(F(s))
//------------------------------------------------------------------------------
force_inline void cidDmp(SdFat32& sd)
{
	cid_t cid;
	if (!sd.card()->readCID(&cid))
	{
		error("readCID failed");
	}

	ArduinoOutStream cout(Serial);
	cout << F("\nManufacturer ID: ");
	cout << hex << int(cid.mid) << dec << endl;
	cout << F("OEM ID: ") << cid.oid[0] << cid.oid[1] << endl;
	cout << F("Product: ");
	for (uint8_t i = 0; i < 5; i++)
	{
		cout << cid.pnm[i];
	}
	cout << F("\nVersion: ");
	cout << int(cid.prv_n) << '.' << int(cid.prv_m) << endl;
	cout << F("Serial number: ") << hex << cid.psn << dec << endl;
	cout << F("Manufacturing date: ");
	cout << int(cid.mdt_month) << '/';
	cout << (2000 + cid.mdt_year_low + 10 * cid.mdt_year_high) << endl;
	cout << endl;
}

force_inline void runSdBenchmark(uint8_t* buf, const size_t BUF_SIZE, SdFat32& sd)
{
	// File size in MB where MB = 1,000,000 bytes.
	constexpr uint32_t FILE_SIZE_MB = 5;
	// File size in bytes.
	constexpr uint32_t FILE_SIZE = 1000000UL * FILE_SIZE_MB;
	// Write pass count.
	constexpr uint8_t WRITE_COUNT = 2;
	// Read pass count.
	constexpr uint8_t READ_COUNT = 2;

	float s;
	uint32_t t;
	uint32_t maxLatency;
	uint32_t minLatency;
	uint32_t totalLatency;
	bool skipLatency;

	// Serial output stream
	ArduinoOutStream cout(Serial);

	cout << F("\nUse a freshly formatted SD for best performance.\n");

	// use uppercase in hex and use 0X base prefix
	cout << uppercase << showbase << endl;

	// test file
	File32 file;
	// open or create file - truncate existing file.
	if (!sd.begin(SD_CONFIG))
	{
		sd.initErrorHalt(&Serial);
	}
	if (sd.fatType() == FAT_TYPE_EXFAT)
	{
		cout << F("Type is exFAT") << endl;
	}
	else
	{
		cout << F("Type is FAT") << int(sd.fatType()) << endl;
	}

	cout << F("Card size: ") << sd.card()->sectorCount() * 512E-9;
	cout << F(" GB (GB = 1E9 bytes)") << endl;

	cidDmp(sd);

	// open or create file - truncate existing file.
	if (!file.open("bench.dat", O_RDWR | O_CREAT | O_TRUNC))
	{
		error("open failed");
	}

	// fill buf with known data
	for (uint16_t i = 0; i < (BUF_SIZE - 2); i++)
	{
		buf[i] = 'A' + (i % 26);
	}
	buf[BUF_SIZE - 2] = '\r';
	buf[BUF_SIZE - 1] = '\n';

	cout << F("FILE_SIZE_MB = ") << FILE_SIZE_MB << endl;
	cout << F("BUF_SIZE = ") << BUF_SIZE << F(" bytes\n");
	cout << F("Starting write test, please wait.") << endl
		 << endl;

	// do write test
	uint32_t n = FILE_SIZE / BUF_SIZE;
	cout << F("write speed and latency") << endl;
	cout << F("speed,max,min,avg") << endl;
	cout << F("KB/Sec,usec,usec,usec") << endl;
	for (uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++)
	{
		file.truncate(0);
		if (PRE_ALLOCATE)
		{
			if (!file.preAllocate(FILE_SIZE))
			{
				error("preAllocate failed");
			}
		}
		maxLatency = 0;
		minLatency = 9999999;
		totalLatency = 0;
		skipLatency = SKIP_FIRST_LATENCY;
		t = millis();
		for (uint32_t i = 0; i < n; i++)
		{
			uint32_t m = micros();
			if (file.write(buf, BUF_SIZE) != BUF_SIZE)
			{
				error("write failed");
			}
			m = micros() - m;
			totalLatency += m;
			if (skipLatency)
			{
				// Wait until first write to SD, not just a copy to the cache.
				skipLatency = file.curPosition() < 512;
			}
			else
			{
				if (maxLatency < m)
				{
					maxLatency = m;
				}
				if (minLatency > m)
				{
					minLatency = m;
				}
			}
		}
		file.sync();
		t = millis() - t;
		s = file.fileSize();
		cout << s / t << ',' << maxLatency << ',' << minLatency;
		cout << ',' << totalLatency / n << endl;
	}
	cout << endl
		 << F("Starting read test, please wait.") << endl;
	cout << endl
		 << F("read speed and latency") << endl;
	cout << F("speed,max,min,avg") << endl;
	cout << F("KB/Sec,usec,usec,usec") << endl;

	// do read test
	for (uint8_t nTest = 0; nTest < READ_COUNT; nTest++)
	{
		file.rewind();
		maxLatency = 0;
		minLatency = 9999999;
		totalLatency = 0;
		skipLatency = SKIP_FIRST_LATENCY;
		t = millis();
		for (uint32_t i = 0; i < n; i++)
		{
			buf[BUF_SIZE - 1] = 0;
			uint32_t m = micros();
			int32_t nr = file.read(buf, BUF_SIZE);
			if (nr != BUF_SIZE)
			{
				error("read failed");
			}
			m = micros() - m;
			totalLatency += m;
			if (buf[BUF_SIZE - 1] != '\n')
			{
				error("data check error");
			}
			if (skipLatency)
			{
				skipLatency = false;
			}
			else
			{
				if (maxLatency < m)
				{
					maxLatency = m;
				}
				if (minLatency > m)
				{
					minLatency = m;
				}
			}
		}
		s = file.fileSize();
		t = millis() - t;
		cout << s / t << ',' << maxLatency << ',' << minLatency;
		cout << ',' << totalLatency / n << endl;
	}
	cout << endl
		 << F("Done") << endl;
	file.close();
}

void runSdBenchmark()
{
	// Discard any input.
	do
	{
		delay(10);
	} while (Serial.available() && Serial.read() >= 0);

	ArduinoOutStream cout(Serial);
	// F( stores strings in flash to save RAM
	cout << F("chipSelect: ") << int(SS) << endl;

	// Initialize at the highest speed supported by the board that is
	// not over 50 MHz. Try a lower speed if SPI errors occur.
	SdFat32 sd;
	if (!sd.begin(chipSelect, SPI_HALF_SPEED))
	{
		sd.initErrorHalt();
	}

	cout << F("Type is FAT") << int(sd.vol()->fatType()) << endl;
	cout << F("Card size: ") << sd.card()->cardSize() * 512E-9;
	cout << F(" GB (GB = 1E9 bytes)") << endl;

	cidDmp(sd);

	{
		delay(100);
		constexpr size_t N = 2048;
		uint8_t buf[N];
		runSdBenchmark(buf, N, sd);
	}

	// {
	// 	delay(1000);
	// 	constexpr int N = 1024;
	// 	uint8_t buf[N];
	// 	runSdBenchmark(buf, N);
	// }

	// {
	// 	delay(1000);
	// 	constexpr int N = 8192;
	// 	uint8_t buf[N];
	// 	runSdBenchmark(buf, N);
	// }
}