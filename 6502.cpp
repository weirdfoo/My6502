// 6502.cpp : Defines the entry point for the application.
// Based on http://www.6502.org/users/obelisk/6502/reference.html
// And https://web.archive.org/web/20160406122905/http://homepage.ntlworld.com/cyborgsystems/CS_Main/6502/6502.htm

#include <cassert>
#include <cstring>

#include "6502.h"

namespace
{
	constexpr uint8_t kBit7Mask = 0b10000000;
	constexpr uint8_t kBit6Mask = 0b01000000;

	inline int8_t AsInt8(uint8_t value)
	{
		return *reinterpret_cast<int8_t*>(&value);
	}
}


Cpu6502::Cpu6502(Clock& clock, Cpu6502Model model)
	: CpuClock(clock)
	, Model(model)
{

}

// Information for instruction decoding
const Cpu6502::InstructionInformation Cpu6502::InstructionInfo[256] =
{
	/* 00 BRK*/
	{
		1,
		7, 
		[](Cpu6502* cpu, Memory64k& mem)
		{
			// The BRK instruction forces the generation of an interrupt request.
			// The program counter and processor status are pushed on the stack then the 
			// IRQ interrupt vector at $FFFE/F is loaded into the PC and the break flag 
			// in the status set to one.
			cpu->PC += 1;
			mem[cpu->SP--] = (cpu->PC >> 8) & 0xFF;
			mem[cpu->SP--] = cpu->PC & 0xFF;
			mem[cpu->SP--] = cpu->F;

			cpu->PC = combineAddr(mem[0xFFFE], mem[0xFFFF]);
			cpu->B = 1;
		},
		[](Cpu6502* cpu, Memory64k& mem) ->uint8_t { return 0; }
	},
	/* 01 ORA (ind,X) */{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 02 XXX*/			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 03 XXX*/			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 04 XXX*/			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 05 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 06 ASL ZeroPage */
	{
		2,
		5,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1];
			cpu->C = (mem[addr] & kBit7Mask) != 0;
			mem[addr] <<= 1;
			cpu->Z = mem[addr] == 0;
			cpu->N = (mem[addr] & kBit7Mask);
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 07 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 08 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 09 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 0A ASL A */
	{
		1,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			cpu->C = (cpu->A & kBit7Mask) != 0;
			cpu->A <<= 1;
			cpu->Z = cpu->A == 0;
			cpu->N = (cpu->A & kBit7Mask);
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 0B */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 0C */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 0D */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 0E ALS Absolute*/
	{
		3,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			cpu->C = (mem[addr] & kBit7Mask) != 0;
			mem[addr] <<= 1;
			cpu->Z = mem[addr] == 0;
			cpu->N = (mem[addr] & kBit7Mask);
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 0F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 10 BPL (branch if negative flag clear) */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			if (cpu->N == 0)
			{
				cpu->PC += AsInt8(cpu->InstructionDecoding[1]);
			}
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// Add 1 cycle if a the branch occurs and the destination address is on the same Page
			// Add 2 cycle if a the branch occurs and the destination address is on a different Page
			if (cpu->N != 0)
				return 0;
			return ((cpu->PC & 0xFF) + cpu->InstructionDecoding[1]) > 0xFF ? 2 : 1;
		}
	},
	/* 11 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 12 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 13 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 14 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 15 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 16 ASL ZeroPage,X */
	{
		2,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1] + cpu->X;
			cpu->C = (mem[addr] & kBit7Mask) != 0;
			mem[addr] <<= 1;
			cpu->Z = mem[addr] == 0;
			cpu->N = (mem[addr] & kBit7Mask);
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 17 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 18 CLC */
	{
		1, 
		2, 
		[](Cpu6502* cpu, Memory64k& mem)
		{
			cpu->C = 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 19 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 1A */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 1B */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 1C */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 1D */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 1E */
	{
		3,
		7,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->X;
			cpu->C = (mem[addr] & kBit7Mask) != 0;
			mem[addr] <<= 1;
			cpu->Z = mem[addr] == 0;
			cpu->N = (mem[addr] & kBit7Mask);
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 1F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 20 JSR Absolute */
	{
		3,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->PC - 1;
			mem[cpu->SP--] = (addr >> 8) & 0xFF;
			mem[cpu->SP--] = addr & 0xFF;
			cpu->PC = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
		},
		[](Cpu6502* cpu, Memory64k& mem) ->uint8_t { return 0; }
	},
	/* 21 AND (Indirrect,X) */
	{
		2,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t zpAddr = cpu->InstructionDecoding[1] + cpu->X;
			uint16_t addr = combineAddr(mem[zpAddr], mem[(zpAddr + 1) & 0xFF]);// indirrect Zero Page
			cpu->C = (mem[addr] & kBit7Mask) != 0;
			mem[addr] <<= 1;
			cpu->Z = mem[addr] == 0;
			cpu->N = (mem[addr] & kBit7Mask);
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 22 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 23 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 24 BIT ZeroPage*/
	{
		2,
		3, 
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1];
			uint8_t temp = cpu->A & mem[addr];
			cpu->N = (temp & kBit7Mask) > 0;
			cpu->V = (temp & kBit6Mask) > 0;
			cpu->Z = temp == 0;
		}, 
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 25 AND_ZeroPage*/
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1];
			uint8_t data = cpu->A &= mem[addr];
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			return 0;
		}
	},
	/* 26 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 27 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 28 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 29 AND_IMMEDIATE */
	{
		2, 
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t data = cpu->A &= cpu->InstructionDecoding[1];
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 2A */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 2B */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 2F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 2C BIT Absolute*/
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			uint8_t temp = cpu->A & mem[addr];
			cpu->N = (temp & kBit7Mask) > 0;
			cpu->V = (temp & kBit6Mask) > 0;
			cpu->Z = temp == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 2D AND_Absolute */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			uint8_t data = cpu->A &= mem[addr];
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 2E */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 30 BMI (branch if negative flag set) */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			if (cpu->N == 1)
			{
				cpu->PC += AsInt8(cpu->InstructionDecoding[1]);
			}
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// Add 1 cycle if a the branch occurs and the destination address is on the same Page
			// Add 2 cycle if a the branch occurs and the destination address is on a different Page
			if (cpu->N != 1)
				return 0;
			return ((cpu->PC & 0xFF) + cpu->InstructionDecoding[1]) > 0xFF ? 2 : 1;
		}
	},
	/* 31 AND (Indirrect),Y */
	{
		2,
		5,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			// TODO : figure out that indirrection....
			uint16_t addr = mem[cpu->InstructionDecoding[1]] + cpu->Y;
			cpu->C = (mem[addr] & kBit7Mask) != 0;
			mem[addr] <<= 1;
			cpu->Z = mem[addr] == 0;
			cpu->N = (mem[addr] & kBit7Mask);
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// ADD 1 cycle if page boundary crossed
			return uint16_t(mem[cpu->InstructionDecoding[1]]) + cpu->Y > 0xFF ? 1 : 0;
		}
	},
	/* 32 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 33 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 34 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 35 AND_ZeroPage,X */
	{
		2,
		3,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1] + cpu->X;
			uint8_t data = cpu->A &= mem[addr];
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 36 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 37 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 38 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 39 AND_Absolute,Y */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->Y;
			uint8_t data = cpu->A &= mem[addr];
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// ADD 1 cycle if page boundary crossed
			return cpu->InstructionDecoding[1] + cpu->Y > 0xFF ? 1 : 0;
		}
	},
	/* 3A */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 3B */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 3C */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 3D AND_Absolute,X */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->X;
			uint8_t data = cpu->A &= mem[addr];
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// ADD 1 cycle if page boundary crossed
			return cpu->InstructionDecoding[1] + cpu->X > 0xFF ? 1 : 0;
		}
	},
	/* 3E */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 3F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 40 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 41 EOR (Indirect,X) TODO */
	{
		2,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t zpAddr = cpu->InstructionDecoding[1] + cpu->X;
			uint16_t addr = combineAddr(mem[zpAddr], mem[(zpAddr + 1) & 0xFF]);// indirrect Zero Page
			cpu->A = cpu->A ^ mem[addr];
			cpu->N = cpu->A & kBit7Mask;
			cpu->Z = cpu->A == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 42 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 43 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 44 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 45 EOR ZeroPage */
	{
		2,
		3,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			cpu->A = cpu->A ^ mem[cpu->InstructionDecoding[1]];
			cpu->N = cpu->A & kBit7Mask;
			cpu->Z = cpu->A == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 46 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 47 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 48 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 49 EOR Immediate */
	{
		2, 
		2, 
		[](Cpu6502* cpu, Memory64k& mem) 
		{
			cpu->A = cpu->A ^ cpu->InstructionDecoding[1];
			cpu->N = cpu->A & kBit7Mask;
			cpu->Z = cpu->A == 0;
		}, 
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } 
	},
	/* 4A */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 4B */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 4C JMP Absolute */
	{
		3,
		3,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			cpu->PC = addr;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 4D EOR Absolute */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			cpu->A = cpu->A ^ mem[addr];
			cpu->N = cpu->A & kBit7Mask;
			cpu->Z = cpu->A == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 4E */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 4F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 50 BVC (branch if overflow flag clear) */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			if (cpu->V == 0)
			{
				cpu->PC += AsInt8(cpu->InstructionDecoding[1]);
			}
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// Add 1 cycle if a the branch occurs and the destination address is on the same Page
			// Add 2 cycle if a the branch occurs and the destination address is on a different Page
			if (cpu->V != 0)
				return 0;
			return ((cpu->PC & 0xFF) + cpu->InstructionDecoding[1]) > 0xFF ? 2 : 1;
		}
	},
	/* 51 EOR (Indirrect),Y TODO */
	{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 52 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 53 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 54 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 55 EOR ZeroPage,X */
	{
		2,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			cpu->A = cpu->A ^ mem[cpu->InstructionDecoding[1] + cpu->X];
			cpu->N = cpu->A & kBit7Mask;
			cpu->Z = cpu->A == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 56 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 57 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 58 CLI */
	{
		1,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			cpu->I = 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 59 EOR Absolute,Y */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->Y;
			cpu->A = cpu->A ^ mem[addr];
			cpu->N = cpu->A & kBit7Mask;
			cpu->Z = cpu->A == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			return uint16_t(cpu->InstructionDecoding[1]) + cpu->Y > 0xFF;
		}
	},
	/* 5A */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 5B */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 5C */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 5D EOR Absolute,X */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->X;
			cpu->A = cpu->A ^ mem[addr];
			cpu->N = cpu->A & kBit7Mask;
			cpu->Z = cpu->A == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t 
		{ 
			return uint16_t(cpu->InstructionDecoding[1]) + cpu->X > 0xFF;
		}
	},
	/* 5E */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 5F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 60 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 61 ADC (Indirrect,x) */
	{
		2,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t zpAddr = cpu->InstructionDecoding[1] + cpu->X;
			uint16_t addr = combineAddr(mem[zpAddr], mem[(zpAddr + 1) & 0xFF]);// indirrect Zero Page
			uint16_t result = cpu->A + mem[addr] + cpu->C;
			cpu->Z = result == 0;
			cpu->V = (cpu->A & kBit7Mask) != (result & kBit7Mask);
			cpu->N = (cpu->A & kBit7Mask);

			assert(cpu->D); // TODO: BCD
			cpu->A = result & 0x7F;
			cpu->C = result > 0xFF;
		}, 
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 62 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 63 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 64 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 65 ADC_ZP */
	{
		2,
		3,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1]; //Zero Page
			uint16_t result = cpu->A + mem[addr] + cpu->C;
			cpu->Z = result == 0;
			cpu->V = (cpu->A & kBit7Mask) != (result & kBit7Mask);
			cpu->N = (cpu->A & kBit7Mask);

			assert(cpu->D); // TODO: BCD
			cpu->A = result & 0x7F;
			cpu->C = result > 0xFF;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 66 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 67 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 68 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 69 ADC_IM */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t result = cpu->A + cpu->InstructionDecoding[1] + cpu->C;
			cpu->Z = result == 0;
			cpu->V = (cpu->A & kBit7Mask) != (result & kBit7Mask);
			cpu->N = (cpu->A & kBit7Mask);

			assert(cpu->D); // TODO: BCD
			cpu->A = result & 0x7F;
			cpu->C = result > 0xFF;
		}, 
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 6A */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 6B */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 6C JMP Indirect */
	{
		3,
		3,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			uint8_t data0 = mem[addr];
			addr = cpu->Model == Cpu6502Model::Original 
						? combineAddr((cpu->InstructionDecoding[1] + 1) & 0xFF, cpu->InstructionDecoding[2])
						: addr + 1;
			uint8_t data1 = mem[addr];
			cpu->PC = combineAddr(data0, data1);;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 6D ADC_ABSOLUTE */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			uint16_t result = cpu->A + mem[addr] + cpu->C;
			cpu->Z = result == 0;
			cpu->V = (cpu->A & kBit7Mask) != (result & kBit7Mask);
			cpu->N = (cpu->A & kBit7Mask);

			assert(cpu->D); // TODO: BCD
			cpu->A = result & 0x7F;
			cpu->C = result > 0xFF;
		}, 
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 6E */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 6F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 70 BVS (branch if overflow flag set) */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			if (cpu->V == 1)
			{
				cpu->PC += AsInt8(cpu->InstructionDecoding[1]);
			}
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// Add 1 cycle if a the branch occurs and the destination address is on the same Page
			// Add 2 cycle if a the branch occurs and the destination address is on a different Page
			if (cpu->V != 1)
				return 0;
			return ((cpu->PC & 0xFF) + cpu->InstructionDecoding[1]) > 0xFF ? 2 : 1;
		}
	},
	/* 71 ADC (indirrect),Y */
	{
		2,
		5,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = mem[cpu->InstructionDecoding[1]] + cpu->Y; // indirrect Zero Page
			uint16_t result = cpu->A + mem[addr] + cpu->C;
			cpu->Z = result == 0;
			cpu->V = (cpu->A & kBit7Mask) != (result & kBit7Mask);
			cpu->N = (cpu->A & kBit7Mask);

			assert(cpu->D); // TODO: BCD
			cpu->A = result & 0x7F;
			cpu->C = result > 0xFF;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// Add 1 cycle if a Page Boundary is crossed
			return uint16_t(mem[cpu->InstructionDecoding[1]]) + cpu->Y > 0xFF ? 1 : 0;
		}
	},
	/* 72 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 73 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 74 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 75  ADC_ZP_X*/
	{
		2,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1] + cpu->X; //Zero Page
			uint16_t result = cpu->A + mem[addr] + cpu->C;
			cpu->Z = result == 0;
			cpu->V = (cpu->A & kBit7Mask) != (result & kBit7Mask);
			cpu->N = (cpu->A & kBit7Mask);

			assert(cpu->D); // TODO: BCD
			cpu->A = result & 0x7F;
			cpu->C = result > 0xFF;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 76 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 77 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 78 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 79 ADC ABSOLUTE,Y */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->Y;
			uint16_t result = cpu->A + mem[addr] + cpu->C;
			cpu->Z = result == 0;
			cpu->V = (cpu->A & kBit7Mask) != (result & kBit7Mask);
			cpu->N = (cpu->A & kBit7Mask);

			assert(cpu->D); // TODO: BCD
			cpu->A = result & 0x7F;
			cpu->C = result > 0xFF;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// ADD 1 cycle if page boundary crossed
			return uint16_t(cpu->InstructionDecoding[1]) + cpu->Y > 0xFF ? 1 : 0;
		}
	},
	/* 7A */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 7B */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 7C */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 7D ADC ABSOLUTE,X */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->X;
			uint16_t result = cpu->A + mem[addr] + cpu->C;
			cpu->Z = result == 0;
			cpu->V = (cpu->A & kBit7Mask) != (result & kBit7Mask);
			cpu->N = (cpu->A & kBit7Mask);

			assert(cpu->D); // TODO: BCD
			cpu->A = result & 0x7F;
			cpu->C = result > 0xFF;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// ADD 1 cycle if page boundary crossed
			return uint16_t(cpu->InstructionDecoding[1]) + cpu->X > 0xFF ? 1 : 0;
		}
	},
	/* 7E */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 7F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 80 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 81 STA (Indirrect,X) */			
	{
		2,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t zpAddr = cpu->InstructionDecoding[1] + cpu->X;
			uint16_t addr = combineAddr(mem[zpAddr], mem[(zpAddr + 1) & 0xFF]);// indirrect Zero Page
			mem[addr] = cpu->A;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 82 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 83 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 84 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 85 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 86 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 87 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 88 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 89 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 8A */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 8B DEY */
	{
		1,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			--cpu->Y;
			cpu->Z = cpu->Y == 0;
			cpu->N = cpu->Y & kBit7Mask;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* 8C */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 8D */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 8E */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 8F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 90 BCC (branch if carry clear) */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			if (cpu->C == 0)
			{
				cpu->PC += AsInt8(cpu->InstructionDecoding[1]);
			}
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// Add 1 cycle if a the branch occurs and the destination address is on the same Page
			// Add 2 cycle if a the branch occurs and the destination address is on a different Page
			if (cpu->C != 0)
				return 0;
			return ((cpu->PC & 0xFF) + cpu->InstructionDecoding[1]) > 0xFF ? 2 : 1;
		}
	},
	/* 91 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 92 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 93 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 94 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 95 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 96 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 97 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 98 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 99 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 9A */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 9B */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 9C */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 9D */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 9E */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* 9F */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* A0 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* A1 LDA (Indirect,X) TODO !!! */
	{
		2,
		6,
		[](Cpu6502* cpu, Memory64k& mem) 
		{
			uint8_t zpAddr = cpu->InstructionDecoding[1] + cpu->X;
			uint16_t addr = combineAddr(mem[zpAddr], mem[(zpAddr + 1) & 0xFF]);// indirrect Zero Page
			uint8_t data = cpu->A = mem[addr];
			cpu->Z = data == 0;
			cpu->N = (data & 0b1000000) == 1;
		}, 
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* A2 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* A3 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* A4 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* A5 LDA ZeroPage */
	{
		2,
		3,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t data = cpu->A = mem[cpu->InstructionDecoding[1]];
			cpu->Z = data == 0;
			cpu->N = (data & 0b1000000) == 1;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* A6 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* A7 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* A8 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* A9 LDA_IM */  	
	{
		2, 
		2, 
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t data = cpu->A = cpu->InstructionDecoding[1];
			cpu->Z = data == 0;
			cpu->N = (data & 0b1000000) == 1;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* AA */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* AB */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* AC */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* AD LDA Absolute */
	{
		3, 
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			uint8_t data = cpu->A = mem[addr];
			cpu->Z = data == 0;
			cpu->N = (data & 0b1000000) == 1;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* AE */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* AF */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* B0 BCS (branch if carry set) */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			if (cpu->C == 1)
			{
				cpu->PC += AsInt8(cpu->InstructionDecoding[1]);
			}
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// Add 1 cycle if a the branch occurs and the destination address is on the same Page
			// Add 2 cycle if a the branch occurs and the destination address is on a different Page
			if (cpu->C != 1)
				return 0;
			return ((cpu->PC & 0xFF) + cpu->InstructionDecoding[1]) > 0xFF ? 2 : 1;
		}
	},
	/* B1 LDA (Indirect),Y TODO !!! */
	{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* B2 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* B3 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* B4 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* B5 LDA ZeroPage,X */
	{
		2,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t data = cpu->A = mem[cpu->InstructionDecoding[1] + cpu->X];
			cpu->Z = data == 0;
			cpu->N = (data & 0b1000000) == 1;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* B6 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* B7 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* B8 CLV */
	{
		1,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			cpu->V = 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* B9 LDA Absolute,Y */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->Y;
			uint8_t data = cpu->A = mem[addr];
			cpu->Z = data == 0;
			cpu->N = (data & 0b1000000) == 1;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			return cpu->InstructionDecoding[1] + cpu->Y > 0xFF ? 1 : 0;
		}
	},
	/* BA */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* BB */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* BC */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* BD LDA Absolute,X */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->X;
			uint8_t data = cpu->A = mem[addr];
			cpu->Z = data == 0;
			cpu->N = (data & 0b1000000) == 1;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			return cpu->InstructionDecoding[1] + cpu->X > 0xFF ? 1 : 0;
		}
	},
	/* BE */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* BF */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* C0 CPY Immediate */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			int16_t data = cpu->Y - cpu->InstructionDecoding[1];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* C1 CMP (Indirect,X) */
	{
		2,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t zpAddr = cpu->InstructionDecoding[1] + cpu->X;
			uint16_t addr = combineAddr(mem[zpAddr], mem[(zpAddr + 1) & 0xFF]);// indirrect Zero Page
			int16_t data = cpu->A - mem[addr];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* C2 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* C3 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* C4 CPY ZeroPage */
	{
		2,
		3,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			int16_t data = cpu->Y - mem[cpu->InstructionDecoding[1]];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* C5 CMP ZeroPage */
	{
		2,
		3,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			int16_t data = cpu->A - mem[cpu->InstructionDecoding[1]];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* C6 DEC ZeroPage */
	{
		2,
		5,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1];
			uint8_t& data = mem[addr];
			data = (data - 1) & 0xFF;
			cpu->Z = data == 0;
			cpu->N = data & kBit7Mask;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* C7 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* C8 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* C9 CMP Immediate */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			int16_t data = cpu->A - cpu->InstructionDecoding[1];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* CA DEX */
	{
		1,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			--cpu->X;
			cpu->Z = cpu->X == 0;
			cpu->N = cpu->X & kBit7Mask;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } 
	},
	/* CB INY */
	{
		1,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			cpu->Y = (cpu->Y + 1) & 0xFF;
			cpu->Z = cpu->Y == 0;
			cpu->N = cpu->Y & kBit7Mask;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* CC CPY Absolute */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			int16_t data = cpu->Y - mem[addr];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* CD CMP Absolute */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			int16_t data = cpu->A - mem[addr];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* CE DEC Absolute */
	{
		3,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			uint8_t& data = mem[addr];
			data = (data - 1) & 0xFF;
			cpu->Z = data == 0;
			cpu->N = data & kBit7Mask;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* CF */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* D0 BNE (branch if zeroflag clear) */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			if (cpu->Z == 0)
			{
				cpu->PC += AsInt8(cpu->InstructionDecoding[1]);
			}
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// Add 1 cycle if a the branch occurs and the destination address is on the same Page
			// Add 2 cycle if a the branch occurs and the destination address is on a different Page
			if (cpu->Z != 0)
				return 0;
			return ((cpu->PC & 0xFF) + cpu->InstructionDecoding[1]) > 0xFF ? 2 : 1;
		}
	},
	/* D1 CMP (Indirect,X) TODO !!!!*/
	{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* D2 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* D3 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* D4 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* D5 CMP ZeroPage,X */
	{
		2,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			int16_t data = cpu->A - mem[cpu->InstructionDecoding[1] + cpu->X];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* D6 DEC ZeroPage,X */
	{
		2,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1] + cpu->X;
			uint8_t& data = mem[addr];
			data = (data - 1) & 0xFF;
			cpu->Z = data == 0;
			cpu->N = data & kBit7Mask;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* D7 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* D8 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* D9 CMP Absolute,Y */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->Y;
			int16_t data = cpu->A - mem[addr];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// ADD 1 cycle if page boundary crossed
			return uint16_t(cpu->InstructionDecoding[1]) + cpu->Y > 0xFF ? 1 : 0;
		}
	},
	/* DA */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* DB CLD */
	{
		1,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			cpu->D = 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* DC */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* DD CMP Absolute,X */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->X;
			int16_t data = cpu->A - mem[addr];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// ADD 1 cycle if page boundary crossed
			return uint16_t(cpu->InstructionDecoding[1]) + cpu->X > 0xFF ? 1 : 0;
		}
	},
	/* DE DEC Absolute,X */
	{
		3,
		7,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->X;
			uint8_t& data = mem[addr];
			data = (data - 1) & 0xFF;
			cpu->Z = data == 0;
			cpu->N = data & kBit7Mask;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* DF */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* E0 CPX Immediate */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			int16_t data = cpu->X - cpu->InstructionDecoding[1];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* E1 SBC (Indirrect,X)*/
	{
		2, 
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t zpAddr = cpu->InstructionDecoding[1] + cpu->X;
			uint16_t addr = combineAddr(mem[zpAddr], mem[(zpAddr + 1) & 0xFF]);// indirrect Zero Page

			int16_t data = 0;
			assert(!cpu->D); // Decimal mode not implemented !!!
			//if(cpu->D)
			//{
			// 	data = bcd(cpu->A) - bcd(mem[addr]) - - (1 - cpu->C);
			//	P.V = (data > 99 || data < 0) ? 1 : 0;
			//}
			//else
			{
				data = int16_t(cpu->A) - mem[addr] - (1 - cpu->C);
				cpu->V = (data > 127 || data < -128) ? 1 : 0;
			}
			cpu->C = (data >= 0) ? 1 : 0;
			cpu->N = data & kBit7Mask;
			cpu->Z = (data == 0) ? 1 : 0;
			cpu->A = data & 0xFF;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* E2 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* E3 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* E4 CPX ZeroPage */
	{
		2,
		3,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			int16_t data = cpu->X - mem[cpu->InstructionDecoding[1]];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* E5 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* E6 INC ZeroPage */
	{
		2,
		5,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint8_t& data = mem[cpu->InstructionDecoding[1]];
			data = (data+1) & 0xFF;
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* E7 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* E8 INX */
	{
		1,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			cpu->X = (cpu->X + 1) & 0xFF;
			cpu->Z = cpu->X == 0;
			cpu->N = cpu->X & kBit7Mask;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* E9 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* EA */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* EB */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* EC CPX Absolute */
	{
		3,
		4,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			int16_t data = cpu->X - mem[addr];
			cpu->C = data >= 0;
			cpu->Z = data == 0;
			cpu->N = (data & kBit7Mask) ? 1 : 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* ED */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* EE INC Absolute */
	{
		3,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]);
			uint8_t& data = mem[addr];
			data = (data + 1) & 0xFF;
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* EF */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* F0 BEQ (branch if zeroflag set) */
	{
		2,
		2,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			if (cpu->Z == 1)
			{
				cpu->PC += AsInt8(cpu->InstructionDecoding[1]);
			}
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t
		{
			// Add 1 cycle if a the branch occurs and the destination address is on the same Page
			// Add 2 cycle if a the branch occurs and the destination address is on a different Page
			if (cpu->Z != 1)
				return 0;
			return ((cpu->PC & 0xFF) + cpu->InstructionDecoding[1]) > 0xFF ? 2 : 1;
		}
	},
	/* F1 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* F2 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* F3 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* F4 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* F5 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* F6 INC ZeroPage,X */
	{
		2,
		6,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = cpu->InstructionDecoding[1] + cpu->X;
			uint8_t& data = mem[addr];
			data = (data + 1) & 0xFF;
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* F7 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* F8 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* F9 */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* FA */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* FB */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* FC */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* FD */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
	/* FE INC Absolute,X */
	{
		3,
		7,
		[](Cpu6502* cpu, Memory64k& mem)
		{
			uint16_t addr = combineAddr(cpu->InstructionDecoding[1], cpu->InstructionDecoding[2]) + cpu->X;
			uint8_t& data = mem[addr];
			data = (data + 1) & 0xFF;
			cpu->N = data & kBit7Mask;
			cpu->Z = data == 0;
		},
		[](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; }
	},
	/* FF */			{0, 0, [](Cpu6502* cpu, Memory64k& mem) {}, [](Cpu6502* cpu, Memory64k& mem) -> uint8_t { return 0; } },
};

void Cpu6502::Reset(Memory64k& mem)
{
	PC = 0xFFFC;
	SP = 0x100;
	F = 0; // Reset all flags
	A = X = Y = 0;

	NextInstruction = true;
	InstructionCycle = 0;
	memset(InstructionDecoding, 0, 4);
}

void Cpu6502::ExecuteCycle(Memory64k& mem)
{
	if (NextInstruction)
	{
		InstructionDecoding[0] = FetchProgramInstruction(mem);
		NextInstruction = false;
		InstructionCycle = 0;
	}

	const InstructionInformation& instruction = InstructionInfo[InstructionDecoding[0]];
	assert(instruction.cycles > 0); // this would mean an invalid opcode was used
	if (InstructionCycle != 0 && InstructionCycle < instruction.size)
		InstructionDecoding[InstructionCycle+1] = FetchProgramInstruction(mem);

	++InstructionCycle;

	if (InstructionCycle >= instruction.cycles + instruction.extraCycle(this, mem))
	{
		instruction.func(this, mem);
		NextInstruction = true;
		InstructionCycle = 0;
		memset(InstructionDecoding, 0, 4);
	}
}

void Cpu6502::Interrupt()
{
	// TODO simulate interrupt pin activation
}

