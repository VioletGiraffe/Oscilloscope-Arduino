#pragma once

#include <arduino-utils.hpp>

#include <stdint.h>

class AdcHandler {
public:
	static constexpr unsigned int bufferLength = 2048;

	force_inline void handleValue(const uint32_t value) volatile
	{
		bufferInProgress()[_currentBufferCursor++] = static_cast<uint16_t>(value & 0xFFFFul);
		if (_currentBufferCursor == bufferLength)
		{
			_currentBufferCursor = 0;
			_bufferReady = true;
			swapBuffers();
		}
	}

	force_inline volatile uint16_t* completedBuffer() volatile
	{
		return _currentBufferIndex == 0 ? _buffer2 : _buffer1;
	}

	force_inline volatile bool bufferReady() volatile
	{
		if (_bufferReady)
		{
			_bufferReady = false;
			return true;
		}
		else
			return false;
	}

private:
	force_inline void swapBuffers() volatile
	{
		_currentBufferIndex = (_currentBufferIndex + 1) & 0x1;
	}

	force_inline volatile uint16_t* bufferInProgress() volatile
	{
		return _currentBufferIndex == 0 ? _buffer1 : _buffer2;
	}

private:
	volatile uint16_t _buffer1[bufferLength];
	volatile uint16_t _buffer2[bufferLength];

	volatile unsigned int _currentBufferIndex = 0;
	unsigned int _currentBufferCursor = 0;

	volatile bool _bufferReady = false;
};