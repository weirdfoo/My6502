#include "6502.h"

#include <thread>

int main()
{
	Clock clock(1000000);
	Memory64k mem;
	mem.Reset();

	// Todo preload a real program in Memory64k !
	mem[0xFFFC] = 0xA9;
	mem[0xFFFD] = 0x99;

	Cpu6502 cpu(clock, Cpu6502Model::Original);

	std::thread cpuThread([&]()
		{
			cpu.Reset(mem);
			while (true)
			{
				// Todo: throttle for 6502 speed (something like 1Mhz)
				cpu.ExecuteCycle(mem);
			}
		});

	// Simulate other chips

	cpuThread.join();

	return 0;
}
