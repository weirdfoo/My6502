#pragma once

#include <cstdint>
#include <chrono>

class Clock
{
	using clock_type = std::chrono::high_resolution_clock;
public:
	Clock(uint64_t frequency);

	void Start();
	void WaitForNextCycle();
	void NextCycle();

	uint64_t Cycle();

private:
	std::chrono::microseconds TimeCycleUs;
	std::chrono::time_point<clock_type> NextCycleTime;
	uint64_t CycleCount;
};

