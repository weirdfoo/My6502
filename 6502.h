#pragma once

#include "Clock.h"
#include "Memory.h"

#include <cstdint>

enum class Cpu6502Model
{
	// Simulate bugs of original 6502:
	// - JMP Indirrect only increment LSB, causing issue with cross-page address
	Original,

	// Simulate Newer 6502 with bugfixes
	Cpu65C02
};
class Cpu6502
{
public:
	Cpu6502(Clock& clock, Cpu6502Model model);

	void Reset(Memory64k& mem);
	void ExecuteCycle(Memory64k& mem);
	void Interrupt();

private:
	uint8_t FetchProgramInstruction(Memory64k& mem)
	{
		assert(PC < 0xFFFF);
		return mem[PC++];
	}

	Clock& CpuClock;

	uint8_t A; // Accumulator register
	uint8_t X; // X Index register
	uint8_t Y; // Y index register
	uint16_t SP; // Stack pointer
	uint16_t PC; // Program Counter
	union
	{
		uint8_t F;
		struct {
			uint8_t N : 1; // Negative
			uint8_t V : 1; // Overflow
			uint8_t ZZZ : 1; // Unused
			uint8_t B : 1; // Break Command
			uint8_t D : 1; // Decimal Mode
			uint8_t I : 1; // Interrupt Disable
			uint8_t Z : 1; // Zero flag
			uint8_t C : 1; // Carry flag
		};
	};

	struct InstructionInformation
	{
		uint8_t size;
		uint8_t cycles;
		void (*func)(Cpu6502* cpu, Memory64k& mem);
		uint8_t(*extraCycle)(Cpu6502* cpu, Memory64k& mem);
	};

	uint8_t NextInstruction : 1; // Signal to fetch new intruction
	uint8_t InstructionCycle : 3; // Current cycle in the instruction
	uint8_t InstructionDecoding[4];
	Cpu6502Model Model;

	// Information for instruction decoding
	static const InstructionInformation InstructionInfo[256];
};
