#pragma once

#include <cstdint>

class Clock
{
public:
	Clock(uint64_t frequency);
private:
	uint64_t Frequency;
};

