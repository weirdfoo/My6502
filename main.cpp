#include "6502.h"

#include <thread>

int main()
{
	Clock clock(1000000);
	Memory64k mem;
	mem.Reset();

	// Todo preload a real program in Memory64k !
	mem[0xFFFC] = 0x00;
	mem[0xFFFD] = 0x60;
	mem[0x6000] = 0xA9;
	mem[0x6001] = 0x99;

	Cpu6502 cpu(clock, Cpu6502Model::Original);
	std::thread cpuThread([&]()
		{
			cpu.Reset(mem);
			clock.Start();
			while (true)
			{
				clock.WaitForNextCycle();
				cpu.ExecuteCycle(mem);
				clock.NextCycle();
			}
		});

	// Simulate other chips

	cpuThread.join();

	return 0;
}
