#include "Clock.h"

#include <thread>

Clock::Clock(uint64_t frequency)
	: TimeCycleUs(1000000/frequency)
	, NextCycleTime(clock_type::now())
	, CycleCount(0)
{

}

void Clock::Start()
{
	NextCycleTime = clock_type::now() + TimeCycleUs;
}

void Clock::WaitForNextCycle()
{
	if (clock_type::now() < NextCycleTime)
		std::this_thread::sleep_until(NextCycleTime);
}

void Clock::NextCycle()
{
	NextCycleTime += TimeCycleUs;
	++CycleCount;
}

uint64_t Clock::Cycle()
{
	return CycleCount;
}