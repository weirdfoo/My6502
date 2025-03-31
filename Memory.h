#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>


template <int SIZE>
class Memory
{
private:
	uint8_t* Data;

public:
	Memory()
	{
		Data = reinterpret_cast<uint8_t*>(malloc(SIZE));
	}

	~Memory()
	{
		free(Data);
	}

	void Reset()
	{
		memset(Data, 0, SIZE);
	}

	uint8_t& operator [] (uint32_t index)
	{
		assert(index < SIZE);
		return Data[index];
	}
};

constexpr uint16_t combineAddr(uint8_t a, uint8_t b)
{
	return a | uint16_t(b) << 8;
}

constexpr uint32_t kMemory64kSize = 64 * 1024; // 64k
using Memory64k = Memory<kMemory64kSize>;
