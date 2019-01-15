/*

 Memory management

*/

#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "system.h"
#include "logfile.h"
#include "memory.h"
#include "debugger.h"
#include "keys.h"
#include "asic.h"
#include "dsp.h"
#include "fdisk.h"

unsigned char ROM[ROM_SIZE];							
unsigned char RAM[RAM_SIZE];							
unsigned char RAM_HI[RAM_SIZE];								// 2Mb
unsigned char PALETTE[256*2];
unsigned char CP1_PALETTE[256 * 4];

uint8_t numPadRowSelect=0;
uint16_t numPadState=0;
uint16_t joyPadState=0;
uint8_t buttonState=0;			// bits 4&5 are button state -- I can only assume on front of unit?? (Start/Select style) -- bits 0&1 are fire button states - stored here for convenience
uint8_t ADPSelect=0;

uint8_t PotXValue=0x80;
uint8_t PotYValue=0x10;
uint8_t PotZValue=0xFF;
uint8_t PotLPValue=0x01;
uint8_t PotRPValue=0x00;
uint8_t PotSpareValue=0x40;

uint8_t Z80_GetByte(uint16_t addr)
{
	// First up, if addr>=3*16K use BANK3 address, BANK3 defaults to 0x40000-0x4FFFF which is overkill, so its going to be more complex than that!

	switch (addr&0xC000)
	{
		case 0x0000:
			return GetByte(ASIC_BANK0+(addr&0x3FFF));

		case 0x4000:
			return GetByte(ASIC_BANK1+(addr&0x3FFF));

		case 0x8000:
			return GetByte(ASIC_BANK2+(addr&0x3FFF));
		
		case 0xC000:
			break;
	}
	return GetByte(ASIC_BANK3+(addr&0x3FFF));
}

void Z80_SetByte(uint16_t addr,uint8_t byte)
{
	switch (addr&0xC000)
	{
		case 0x0000:
			SetByte(ASIC_BANK0+(addr&0x3FFF),byte);
			return;

		case 0x4000:
			SetByte(ASIC_BANK1+(addr&0x3FFF),byte);
			return;

		case 0x8000:
			SetByte(ASIC_BANK2+(addr&0x3FFF),byte);
			return;
		
		case 0xC000:
			break;
	}
	SetByte(ASIC_BANK3+(addr&0x3FFF),byte);
}

uint8_t Z80_GetPort(uint16_t addr)
{
	return GetPortB(addr&0xFF);			// ports are 8 bit in range, mirrored across full range
}

void Z80_SetPort(uint16_t addr,uint8_t byte)
{
	SetPortB(addr&0xFF,byte);
}

int TODOMemMap = 1;

uint8_t DSPCP1RAM[0x1000];
uint8_t FLASHRAM[0x10000];
uint8_t GetByteCP1(uint32_t addr)
{
	addr&=0xFFFFFF;		// 16Mb address space - linear (aka hardware address)

	if (addr>=0xFE0000 || TODOMemMap)
	{
		return ROM[addr & 0x1FFFF];
	}
	if (addr >= 0xF00000 && addr <= 0xF0FFFF)
	{
		CONSOLE_OUTPUT("Flash Read : %06X -> %02X\n", addr, FLASHRAM[addr-0xF00000]);
		return FLASHRAM[addr - 0xF00000];
	}
	if (addr >= 0xF10000 && addr <= 0xF103FF)
	{
		return CP1_PALETTE[addr - 0xF10000];
	}
	if (addr >= 0xF18000 && addr <= 0xF18FFF)
	{
		// Hack - this is DSP interface
		return DSPCP1RAM[addr - 0xF18000];
	}

	if (addr < RAM_SIZE)
	{
#if ENABLE_DEBUG
		if (debugWatchReads)
		{
			CONSOLE_OUTPUT("GetByte RAM: %05X - %02X\n",addr,RAM[addr]);
		}
#endif
		return RAM[addr];
	}

	if (addr >= 0x700000 && addr < 0x800000)
	{
		return RAM[addr-0x700000];
	}

	if (addr >= 0x800000 && addr < 0x900000)
	{
		return RAM_HI[addr-0x800000];
	}

	CONSOLE_OUTPUT("GetByte : %06X - TODO\n",addr);
	return 0xAA;
}


uint8_t GetByteMSU(uint32_t addr)
{
	addr&=0xFFFFF;
	if (addr<0xC0000)
	{
#if ENABLE_DEBUG
		if (debugWatchReads)
		{
			CONSOLE_OUTPUT("GetByte RAM: %05X - %02X\n",addr,RAM[addr]);
		}
#endif
		return RAM[addr];
	}
	if (addr>=0xC1000 && addr<=0xC1FFF)
	{
		return ASIC_HostDSPMemReadMSU(addr-0xC1000);
	}
	if (addr>=0xE0000)
	{
#if ENABLE_DEBUG
		if (debugWatchReads)
		{
			CONSOLE_OUTPUT("GetByte ROM: %05X - %02X\n",addr,ROM[addr-0xE0000]);
		}
#endif
		return ROM[addr-0xE0000];
//		return 0xCB;			// STUB BIOS, Anything that FAR calls into it, will be returned from whence it came
	}
#if ENABLE_DEBUG
	CONSOLE_OUTPUT("GetByte : %05X - TODO\n",addr);
#endif
	return 0xAA;
}

uint8_t GetByteP88(uint32_t addr)
{
	addr&=0xFFFFF;
	if (addr<0x40000)
	{
		return RAM[addr];		// Always Screen
	}
	if (addr>=0x80000 && addr<=0xFFFFF)//addr<0xC0000)		// Expansion RAM 0
	{
		return RAM[addr];
	}
	if (addr>=0x41000 && addr<=0x41FFF)
	{
		return ASIC_HostDSPMemReadP88(addr-0x41000);
	}
#if ENABLE_DEBUG
	CONSOLE_OUTPUT("GetByte : %05X - TODO\n",addr);
#endif
	return 0xAA;
}

uint8_t GetByteP89(uint32_t addr)	// Memory Map Changed for this machine
{
	addr&=0xFFFFF;
	if (addr<0xC0000)		// RAM (Either 0-7FFFF Expansion Ram & 80000-BFFFF Screen     OR    0-3FFFF Screen 4 & 40000-BFFFF Expansion Ram) - Selectable via MEM
	{
#if ENABLE_DEBUG
		if (debugWatchReads)
		{
			CONSOLE_OUTPUT("GetByte RAM: %05X - %02X\n",addr,RAM[addr]);
		}
#endif
		return RAM[addr];
	}
	if (addr>=0xC0000 && addr<=0xC01FF)
	{
#if ENABLE_DEBUG
		if (debugWatchReads)
		{
			CONSOLE_OUTPUT("GetByte PAL: %05X - %02X\n",addr,PALETTE[addr-0xC0000]);
		}
#endif
		return PALETTE[addr-0xC0000];
	}
	if (addr>=0xC1000 && addr<=0xC1FFF)
	{
		uint8_t b = ASIC_HostDSPMemReadP89(addr-0xC1000);
#if ENABLE_DEBUG
		if (debugWatchReads)
		{
			CONSOLE_OUTPUT("GetByte DSP: %05X - %02X\n",addr,b);
		}
#endif
		return b;
	}
	if (addr>=0xFFC00)
	{
#if ENABLE_DEBUG
		if (debugWatchReads)
		{
			CONSOLE_OUTPUT("GetByte ROM: %05X - %02X\n",addr,ROM[addr-0xFFC00]);
		}
#endif
		return ROM[addr-0xFFC00];		// Konix BIOS loads here
	}
#if ENABLE_DEBUG
	CONSOLE_OUTPUT("GetByte : %05X - TODO\n",addr);
#endif
	return 0xAA;
}

uint8_t GetByteFL1(uint32_t addr)
{
	// Flare One uses paging, the paging is managed directly in the Z80_GetByte routine, so on entry to here we already have a linear flat address
	addr&=0xFFFFF;

	if (addr<0xCFFFF)
	{
		return RAM[addr];
	}

	CONSOLE_OUTPUT("GetByte : %05X - TODO\n",addr);

	return 0xAA;
}

uint8_t GetByte(uint32_t addr)
{
	uint8_t retVal;
	switch (curSystem)
	{
		case ESS_CP1:
			return GetByteCP1(addr);
		case ESS_MSU:
			return GetByteMSU(addr);
		case ESS_P89:
			return GetByteP89(addr);
		case ESS_P88:
			retVal=GetByteP88(addr);
#if ENABLE_DEBUG
			if (debugWatchReads)
			{
				CONSOLE_OUTPUT("Reading from address : %05X->%02X\n",addr,retVal);
			}
#endif
			return retVal;
		case ESS_FL1:
			return GetByteFL1(addr);
	}
	return 0xBB;
}

void SetByteCP1(uint32_t addr,uint8_t byte)
{
	addr&=0xFFFFFF;
	if (addr>=0xFE0000 || TODOMemMap)
	{
		if (debugWatchWrites)
		{
			CONSOLE_OUTPUT("Write To ROM : %06X,%02X\n", addr, byte);
		}
		return;
	}
	if (addr >= 0xF00000 && addr <= 0xF0FFFF)
	{
		CONSOLE_OUTPUT("Flash Write : %06X,%02X\n", addr, byte);
		FLASHRAM[addr - 0xF00000]=byte;
		return;
	}

	if (addr >= 0xF10000 && addr <= 0xF103FF)
	{
		CP1_PALETTE[addr - 0xF10000]=byte;
		return;
	}
	if (addr >= 0xF18000 && addr <= 0xF18FFF)
	{
		// Hack - this is DSP interface
		DSPCP1RAM[addr - 0xF18000] = byte;
		return;
	}
	if (addr<RAM_SIZE)
	{
		if (debugWatchWrites)
		{
			CONSOLE_OUTPUT("Write To RAM : %06X,%02X\n", addr, byte);
		}
		RAM[addr]=byte;
		return;
	}
	if (addr >= 0x700000 && addr < 0x800000)
	{
		if (debugWatchWrites)
		{
			CONSOLE_OUTPUT("Write To LO RAM (mirror): %06X,%02X\n", addr, byte);
		}
		RAM[addr-0x700000]=byte;
		return;
	}

	if (addr >= 0x800000 && addr < 0x900000)
	{
		if (debugWatchWrites)
		{
			CONSOLE_OUTPUT("Write To HI RAM : %06X,%02X\n", addr, byte);
		}
		RAM_HI[addr-0x800000]=byte;
		return;
	}

	CONSOLE_OUTPUT("SetByte : %06X,%02X - TODO\n",addr,byte);
}

void SetByteMSU(uint32_t addr,uint8_t byte)
{
	addr&=0xFFFFF;
#if ENABLE_DEBUG
	if (addr==doDebugTrapWriteAt)
	{
		CONSOLE_OUTPUT("STOMP STOMP STOMP\n");
	}
	if (debugWatchWrites)
	{
		if (addr<0xC1000 || addr>0xC1FFF)		// DSP handled seperately
		{
			CONSOLE_OUTPUT("Writing to address : %05X<-%02X\n",addr,byte);
		}
	}
#endif
	if (addr<0xC0000)
	{
		RAM[addr]=byte;
		return;
	}
	if (addr>=0xC0000 && addr<=0xC01FF)
	{
		PALETTE[addr-0xC0000]=byte;
		return;
	}
	if (addr>=0xC1000 && addr<=0xC1FFF)
	{
		ASIC_HostDSPMemWriteMSU(addr-0xC1000,byte);
		return;
	}
#if ENABLE_DEBUG
	CONSOLE_OUTPUT("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
#endif
}

void SetByteP89(uint32_t addr,uint8_t byte)
{
	addr&=0xFFFFF;
	if (addr<0xC0000)
	{
#if ENABLE_DEBUG
		if (debugWatchWrites)
		{
			CONSOLE_OUTPUT("SetByte RAM: %05X - %02X\n",addr,byte);
		}
#endif
		RAM[addr]=byte;
		return;
	}
	if (addr>=0xC0000 && addr<=0xC01FF)
	{
#if ENABLE_DEBUG
		if (debugWatchWrites)
		{
			CONSOLE_OUTPUT("SetByte PAL: %05X - %02X\n",addr,byte);
		}
#endif
		PALETTE[addr-0xC0000]=byte;
		return;
	}
	if (addr>=0xC1000 && addr<=0xC1FFF)
	{
#if ENABLE_DEBUG
		if (debugWatchWrites)
		{
			CONSOLE_OUTPUT("SetByte DSP: %05X - %02X\n",addr,byte);
		}
#endif
		ASIC_HostDSPMemWriteP89(addr-0xC1000,byte);
		return;
	}
#if ENABLE_DEBUG
	CONSOLE_OUTPUT("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
#endif
}

void SetByteP88(uint32_t addr,uint8_t byte)
{
	addr&=0xFFFFF;
#if ENABLE_DEBUG
	if (addr==doDebugTrapWriteAt)
	{
		CONSOLE_OUTPUT("STOMP STOMP STOMP\n");
	}
	if (debugWatchWrites)
	{
		if (addr<0x41000 || addr>0x41FFF)		// DSP handled seperately
		{
			CONSOLE_OUTPUT("Writing to address : %05X<-%02X\n",addr,byte);
		}
	}
#endif
	if (addr<0x40000)
	{
		RAM[addr]=byte;
		return;
	}
	if (addr>=0x80000 && addr<=0xFFFFF)//addr<0xC0000)		// Expansion RAM 0
	{
		RAM[addr]=byte;
		return;
	}
	if (addr>=0x40000 && addr<=0x401FF)
	{
		PALETTE[addr-0x40000]=byte;
		return;
	}
	if (addr>=0x41000 && addr<=0x41FFF)
	{
		ASIC_HostDSPMemWriteP88(addr-0x41000,byte);
		return;
	}
#if ENABLE_DEBUG
	CONSOLE_OUTPUT("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
#endif
}

void SetByteFL1(uint32_t addr,uint8_t byte)
{
	// Flare One uses paging, the paging is managed directly in the Z80_GetByte routine, so on entry to here we already have a linear flat address
	addr&=0xFFFFF;

	if (addr<0xCFFFF)
	{
		RAM[addr]=byte;
		return;
	}

#if ENABLE_DEBUG
	CONSOLE_OUTPUT("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
#endif
}

void SetByte(uint32_t addr,uint8_t byte)
{
	switch (curSystem)
	{
		case ESS_CP1:
			SetByteCP1(addr,byte);
			break;
		case ESS_MSU:
			SetByteMSU(addr,byte);
			break;
		case ESS_P89:
			SetByteP89(addr,byte);
			break;
		case ESS_P88:
			SetByteP88(addr,byte);
			break;
		case ESS_FL1:
			SetByteFL1(addr,byte);
			break;
	}
}

extern uint32_t ASIC_BLTPC;

uint8_t bitSwizzle=0x55;

extern uint8_t IsKeyAvailable();
extern uint8_t NextKeyCode();

uint8_t GetPortB(uint16_t port)
{
	switch (curSystem)
	{
		case ESS_CP1:
			CONSOLE_OUTPUT("CP1 Port Read %04X\n", port);
			return 0xFF;	// TODO
		case ESS_MSU:
			if (port==0x0C)
			{
				return buttonState;
			}
			if (port==0xE0)
			{
				switch (numPadRowSelect)
				{
					case 1:
						return (numPadState&0xF);
					case 2:
						return ((numPadState&0xF0)>>4);
					case 4:
						return ((numPadState&0xF00)>>8);
					case 8:
						return ((numPadState&0xF000)>>12);
					default:
#if ENABLE_DEBUG
						CONSOLE_OUTPUT("Warning unknown numPadRowSelectValue : %02X\n",numPadRowSelect);
#endif
						return 0xFF;
				}
			}
			break;
		case ESS_P89:
			return ASIC_ReadP89(port,doShowPortStuff);
			break;
		case ESS_P88:
			if (port==0x40)
			{
				return (0xFFFF ^ joyPadState)>>8;
			}
			if (port==0x50)
			{
				return (0xFFFF ^ joyPadState)&0xFF;
			}
			if (port<=3 || port==0x71 || port==0x73)
			{
				return ASIC_ReadP88(port,doShowPortStuff);
			}
			break;
		case ESS_FL1:
			switch (port)
			{
/*				case 0x18:
					return ASIC_BLTPC&0xFF;
				case 0x19:
					return (ASIC_BLTPC>>8)&0xFF;
				case 0x1A:
					return (ASIC_BLTPC>>16)&0xFF;*/
				case 0xE0:
					switch (ADPSelect)
					{
					case 5:
						return PotYValue;
					case 6:
						return PotXValue;
					}
					return 0xFF;
				case 0x22:		// Ready PORT  bit 5 seems to indicate drive ready (maybe?) bios expects it to be 0 during floppy initialisation (for now bit 5 is masked out)
					return ((0xFFFF^joyPadState)>>10)&0xDF;
				case 0xA0:
					return (0xFFFF^joyPadState)>>8;
				case 0x0051:
				case 0x0000:
				case 0x0001:
				case 0x0002:
				case 0x0003:
				case 0x0007:
				case 0x0014:
				case 0x0020:
				case 0x0021:
				case 0x0006:		// LPEN3 -- labelled as combined light pen register -- but combined how.. All I know is the bios during interrupt reads from this and takes bit 4 and based on result either
							//tries to read various peripherals or goes on to clear the interrupt and do other processing.
							//Speculation : Could bit 4 indicate interrupt source, with a second interrupt firing when devices need processing
					return ASIC_ReadFL1(port,doShowPortStuff);

				case 0x001C:		// KBSP - Keyboard Status Port -- bit 0 seems to be set when keyboard has data to read
					return IsKeyAvailable();
				case 0x0018:		// KBDP - Keyboard Data Port -- should be the scan code from the keyboard
					return NextKeyCode();
				case 0x0026:	// Hack around uart
					bitSwizzle=((bitSwizzle&0x80)>>7)|((bitSwizzle&0x7F)<<1);
					return (bitSwizzle&0xFE) | IsKeyAvailable();

				case 0x0030:
					return FDC_GetStatus();
				case 0x0031:
					return FDC_GetTrack();
				case 0x0032:
					return FDC_GetSector();
				case 0x0033:
					return FDC_GetData();
			}
			break;
	}

#if ENABLE_DEBUG
	if (doShowPortStuff)
	{
		CONSOLE_OUTPUT("GetPortB : %04X - TODO\n",port);
		DebugRPort(port);
	}
#endif
	return 0x00;
}

void ClearKBKey();

void SetPortB(uint16_t port,uint8_t byte)
{
	switch (curSystem)
	{
		case ESS_CP1:
			CONSOLE_OUTPUT("CP1 Port Write : %04X %02X", port, byte);
			break;
		case ESS_MSU:
			switch (port)
			{
				case 0xC0:
				ADPSelect=byte;
				break;
			case 0xE0:
				numPadRowSelect=byte;
				break;
			default:
				ASIC_WriteMSU(port,byte,doShowPortStuff);
				break;
			}
			break;
		case ESS_P89:
			ASIC_WriteP89(port,byte,doShowPortStuff);
			break;
		case ESS_P88:
			switch (port)
			{
			case 0xC0:
				ADPSelect=byte;
				break;
			case 0xE0:
				numPadRowSelect=byte;
				break;
			default:
				ASIC_WriteP88(port,byte,doShowPortStuff);
				break;
			}
			break;
		case ESS_FL1:
			switch (port)
			{
			case 0xE0:
				ADPSelect=byte;			//5 = vertical , 6 = horizontal		(reading port after some delay returns -128+127 ?)
				break;

			case 0x0014:
				DSP_STATUS=byte;
#if ENABLE_DEBUG
				if (doShowPortStuff)
				{
					CONSOLE_OUTPUT("DSP STATUS : %02X\n",byte);
				}
#endif
				break;
			case 0x0030:
				FDC_SetCommand(byte);
				break;
			case 0x0031:
				FDC_SetTrack(byte);
				break;
			case 0x0032:
				FDC_SetSector(byte);
				break;
			case 0x0033:
				FDC_SetData(byte);
				break;
			case 0x001B:
				// Keyboard Control
				if (byte&0x01)
				{
					ClearKBKey();
				}

				break;
			case 0x0022:
				// GPO known bits ????sdd?
				//for now we just need the side
				if (byte&0x2)
				{
					FDC_SetDrive(0);
				}
				if (byte&0x4)
				{
					FDC_SetDrive(1);
				}
				FDC_SetSide((byte>>3)&0x01);
				break;
			default:
				ASIC_WriteFL1(port,byte,doShowPortStuff);
				break;
			}
			break;
	}
#if ENABLE_DEBUG
	if (doShowPortStuff)
	{
		DebugWPort(port);
	}
#endif
}

extern int hClock;
extern int vClock;

extern volatile unsigned char* pMapIO;

uint16_t HACK_PDATA = 0xFFFF;
uint16_t HACK_PSTAT = 0x0000;
uint16_t HACK_BIOS = 0xFFFF;

int cnt = 0;
uint8_t sequence[9+64*1024+1] = { 128,0,0,0,0,0xCB,0x15,0,0 };
int onetime=1;
uint16_t CP1_GetNextREXThing()
{
	if (onetime)
	{
		onetime = 0;
		FILE *tFile = fopen("GEOFF6.REX", "rb");
		fseek(tFile, 0, SEEK_END);
		uint32_t length = ftell(tFile);

		sequence[5] = (length >> 0) & 0xFF;
		sequence[6] = (length >> 8) & 0xFF;
		sequence[7] = (length >> 16) & 0xFF;
		sequence[8] = (length >> 24) & 0xFF;

		fseek(tFile, 0, SEEK_SET);
		fread(&sequence[9], 1, length, tFile);
		fclose(tFile);
		// calc checksum and store
		uint8_t sum = 0;
		for (int a = 0; a < length; a++)
		{
			sum ^= sequence[9 + a];
		}
		sequence[9 + length] = sum;
	}
	uint16_t t = sequence[cnt];
	cnt++;
	return t;
}

uint16_t MSU_JOYIN = 0xFFFF;

uint16_t GetPortW(uint16_t port)
{
#if ENABLE_DEBUG
	if (doShowPortStuff)
	{
		CONSOLE_OUTPUT("GetPortW : %04X - TODO\n",port);
		DebugRPort(port);
	}
#endif
	switch (curSystem)
	{
		case ESS_CP1:
			switch (port)
			{
			case 0x0000:
				return hClock;
			case 0x0002:
				return vClock;
			case 0x000C:	// STAT
				return 0;// bit 0 pal/ntsc, bit 1 lpen signalled
			case 0x0080:
				return MSU_JOYIN;
			case 0x0084:
				return 0;
			case 0x0086:	// MCommsPort
				return 0x0002;		// bit 0 = 0 can send , bit 1 = 1 can read
			case 0x0088:
				return CP1_GetNextREXThing();
				//return HACK_PDATA;
			case 0x008A:
				return HACK_PSTAT;
			case 0x008C:
				return HACK_BIOS;
			case 0xE0:
			case 0xE2:
			case 0xE4:
			case 0xE6:
			case 0xE8:
			case 0xEA:
			case 0xEC:
			case 0xEE:
			{
				uint16_t word = pMapIO[2];
				word += pMapIO[3] << 8;
				return word;
			}
			default:
				if (doShowPortStuff)
				{
					CONSOLE_OUTPUT("CP1 Read Port : %04X\n", port);
				}
				return 0xFFFF;
			}
			break;
		case ESS_MSU:
			if (port==0xC0)
			{
				uint16_t potStatus;

				potStatus=buttonState&3;
				if (ADPSelect==PotXValue)
					potStatus|=(0x04);
				if (ADPSelect==PotYValue)
					potStatus|=(0x08);
				if (ADPSelect==PotZValue)
					potStatus|=(0x10);
				if (ADPSelect==PotLPValue)
					potStatus|=(0x20);
				if (ADPSelect==PotRPValue)
					potStatus|=(0x40);
				if (ADPSelect==PotSpareValue)
					potStatus|=(0x80);
				return 0x0003 ^ potStatus;
			}
			if (port==0x80)
			{
				return 0xFFFF ^ joyPadState;
			}
			break;
		case ESS_P89:
			return (ASIC_ReadP89(port+1,doShowPortStuff)<<8)|ASIC_ReadP89(port,doShowPortStuff);
		case ESS_P88:
			if (port<=3 || port==0x71 || port==0x73)
			{
				return (ASIC_ReadP88(port+1,doShowPortStuff)<<8)|ASIC_ReadP88(port,doShowPortStuff);
			}
			break;
		case ESS_FL1:
			CONSOLE_OUTPUT("Flare One Should never reach GetPortW!");
			break;
	}
	return 0x0000;
}
extern uint16_t	ASIC_BORD;
extern uint16_t	ASIC_KINT;
extern uint32_t	ASIC_CP1_BLTPC;
extern uint16_t	ASIC_CP1_BLTCMD;
extern int VideoInterruptLatch;
extern uint8_t	ASIC_DIS;
extern uint32_t	ASIC_SCROLL;
extern uint16_t	ASIC_CP1_MODE;
extern uint16_t	ASIC_CP1_MODE2;

uint16_t ASIC_CP1_DEB_PORTS[128];

void SetPortW(uint16_t port,uint16_t word)
{
	switch (curSystem)
	{
		case ESS_CP1:
			ASIC_CP1_DEB_PORTS[port / 2] = word;
			switch (port)
			{
			case 0x00:
				ASIC_KINT = word;
				break;
			case 0x10:
				ASIC_SCROLL &= 0xFFFF0000;
				ASIC_SCROLL |= word;
				break;
			case 0x12:
				ASIC_SCROLL &= 0x0000FFFF;
				ASIC_SCROLL |= word << 16;
				break;
			case 0x16:
				//INT_ACK
				VideoInterruptLatch = 0;
				break;
			case 0x18:
				ASIC_CP1_MODE = word;
				break;
			case 0x1A:
				ASIC_BORD = word;
				break;
			case 0x26:
				TODOMemMap = 0;
				break;
			case 0x2A:
				ASIC_CP1_MODE2 = word;
				break;
			case 0x2C:
				ASIC_DIS = word & 0x01;
				break;
			case 0x40:
				ASIC_CP1_BLTPC &= 0xFFFF0000;
				ASIC_CP1_BLTPC |= word;
				break;
			case 0x42:
				ASIC_CP1_BLTPC &= 0x0000FFFF;
				ASIC_CP1_BLTPC |= word << 16;
				break;
			case 0x44:
				ASIC_CP1_BLTCMD = word;
				break;
			case 0x84:
				break;
			case 0x8C:
				HACK_BIOS = word;
				break;
			case 0xE0:
			case 0xE2:
			case 0xE4:
			case 0xE6:
			case 0xE8:
			case 0xEA:
			case 0xEC:
			case 0xEE:
				for (int a = 0xE0; a < 0xF0; a += 2)
				{
					ASIC_CP1_DEB_PORTS[a / 2] = word;
				}
				pMapIO[0] = word & 0xFF;
				pMapIO[1] = (word & 0xFF00) >> 8;
				break;
			default:
				CONSOLE_OUTPUT("CP1 Port Write : %04X,%04X\n", port, word);
				break;
			}
			break;
		case ESS_MSU:
			ASIC_WriteMSU(port,word&0xFF,doShowPortStuff);
			ASIC_WriteMSU(port+1,word>>8,doShowPortStuff);
			if (port==0xC0)
			{
				ADPSelect=word&0xFF;
			}
			break;
		case ESS_P89:
			ASIC_WriteP89(port,word&0xFF,doShowPortStuff);
			ASIC_WriteP89(port+1,word>>8,doShowPortStuff);
			break;
		case ESS_P88:
			ASIC_WriteP88(port,word&0xFF,doShowPortStuff);
			ASIC_WriteP88(port+1,word>>8,doShowPortStuff);
			break;
		case ESS_FL1:
			CONSOLE_OUTPUT("Flare One Should never reach SetPortW!");
			break;
	}
#if ENABLE_DEBUG
	if (doShowPortStuff)
	{
		DebugWPort(port);
	}
#endif
}

uint8_t MSU_GetByte(uint32_t addr)
{
	return GetByte(addr);
}

void MSU_SetByte(uint32_t addr,uint8_t byte)
{
	SetByte(addr,byte);
}

#if MEMORY_MAPPED_DEBUGGER
extern uint16_t ASIC_DEB_PORTS[128];
#endif

uint16_t MSU_GetPortW(uint16_t port)
{
    return GetPortW(port & 0xFF);
}

uint8_t MSU_GetPortB(uint16_t port)
{
    return GetPortB(port & 0xFF);
}

void MSU_SetPortW(uint16_t port,uint16_t word)
{
    port &= 0xFF;
#if MEMORY_MAPPED_DEBUGGER
    ASIC_DEB_PORTS[port >> 1] = word;
#endif
    SetPortW(port, word);
}

void MSU_SetPortB(uint16_t port,uint8_t byte)
{
    port &= 0xFF;
#if MEMORY_MAPPED_DEBUGGER
    uint16_t t = ASIC_DEB_PORTS[port >> 1];
    if (port&1)
    {
        t &= 0x00FF;
        t |= ((uint16_t)byte)<<8;
    }
    else
    {
        t &= 0xFF00;
        t |= byte;
    }
    ASIC_DEB_PORTS[port >> 1] = t;
#endif
	SetPortB(port,byte);
}

uint32_t joy89state;

void TickKeyboard()
{
	int a;
	static const int keyToJoy_KB[16]={	0,0,GLFW_KEY_KP_1,GLFW_KEY_KP_3,GLFW_KEY_KP_4,GLFW_KEY_KP_6,GLFW_KEY_KP_8,GLFW_KEY_KP_2,		// Joystick 2
						0,0,GLFW_KEY_Z,GLFW_KEY_X,GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_UP,GLFW_KEY_DOWN};			// Joystick 1
	static const int keyToJoy_JY[16]={	0,0,GLFW_KEY_KP_1,GLFW_KEY_KP_3,GLFW_KEY_KP_4,GLFW_KEY_KP_6,GLFW_KEY_KP_8,GLFW_KEY_KP_2,		// Joystick 2
						0,0,0x10000002,0x10000001,0x1000000D,0x1000000B,0x1000000A,0x1000000C};				// Joystick 1 - Mapped to joysticks (hence special numbers)
	static const int keyToJoy_KBFL1[16]={	0,0,GLFW_KEY_KP_8,GLFW_KEY_KP_2,GLFW_KEY_KP_4,GLFW_KEY_KP_6,GLFW_KEY_KP_3,GLFW_KEY_KP_1,		// Joystick 2
						0,0,GLFW_KEY_UP,GLFW_KEY_DOWN,GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_RIGHT_CONTROL,GLFW_KEY_RIGHT_ALT};			// Joystick 1
	static const int keyToJoy_JYFL1[16]={	0,0,GLFW_KEY_KP_8,GLFW_KEY_KP_2,GLFW_KEY_KP_4,GLFW_KEY_KP_6,GLFW_KEY_KP_3,GLFW_KEY_KP_1,		// Joystick 2
						0,0,0x1000000A,0x1000000C,0x1000000D,0x1000000B,0x10000001,0x10000002};				// Joystick 1 - Mapped to joysticks (hence special numbers)

	static const int knxKeyToJoy_KB[19]={	0,0,GLFW_KEY_R,GLFW_KEY_E,GLFW_KEY_W,GLFW_KEY_Q,GLFW_KEY_F,GLFW_KEY_D,GLFW_KEY_S,GLFW_KEY_A,GLFW_KEY_Z,GLFW_KEY_X,GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_UP,GLFW_KEY_DOWN,GLFW_KEY_C,GLFW_KEY_ENTER,GLFW_KEY_V};

	const int* keyToJoy=keyToJoy_KB;
	if (JoystickPresent())
	{
		keyToJoy=keyToJoy_JY;
		if (curSystem==ESS_FL1)
		{
			keyToJoy=keyToJoy_JYFL1;
		}
	}
	else
	{
		if (curSystem==ESS_FL1)
		{
			keyToJoy=keyToJoy_KBFL1;
		}
	}

	for (a=0;a<19;a++)
	{
		if (KeyDown(knxKeyToJoy_KB[a]))
		{
			joy89state|=(1<<a);
		}
		else
		{
			joy89state&=~(1<<a);
		}
	}

	for (a=0;a<12;a++)
	{
        int key = GLFW_KEY_F1 + a;

        if (a == 11)
            key = GLFW_KEY_BACKSPACE;

		if (KeyDown(key))
		{
			numPadState|=(1<<a);
		}
		else
		{
			numPadState&=~(1<<a);
		}
	}
	for (a=0;a<16;a++)
	{
		if (keyToJoy[a]!=0)
		{
			if (keyToJoy[a]>=0x10000000)
			{
				if (JoyDown(keyToJoy[a]&0xF))
				{
					joyPadState|=1<<a;
				}
				else
				{
					joyPadState&=~(1<<a);
				}
			}
			else
			{
				if (KeyDown(keyToJoy[a]))
				{
					joyPadState|=1<<a;
				}
				else
				{
					joyPadState&=~(1<<a);
				}
			}
		}
		else
		{
			joyPadState&=~(1<<a);
		}
	}
	if ((JoystickPresent() && JoyDown(0))||(!JoystickPresent() && KeyDown(GLFW_KEY_SPACE)))
	{
		buttonState|=0x01;
	}
	else
	{
		buttonState&=~0x01;
	}
	if (KeyDown(GLFW_KEY_KP_5))
	{
		buttonState|=0x02;
	}
	else
	{
		buttonState&=~0x02;
	}
	if (KeyDown(GLFW_KEY_2))
	{
		buttonState|=0x10;
	}
	else
	{
		buttonState&=~0x10;
	}
	if (KeyDown(GLFW_KEY_1))
	{
		buttonState|=0x20;
	}
	else
	{
		buttonState&=~0x20;
	}

	if (JoystickPresent())
	{
		PotXValue=(JoystickAxis(0)*127)+128;
		PotYValue=(JoystickAxis(1)*127)+128;
		PotZValue=(JoystickAxis(3)*127)+128;
		if (JoystickAxis(2)>=0.0f)
		{
			PotLPValue=(JoystickAxis(2)*255);
		}
		else
		{
			PotLPValue=0;
		}
		if (JoystickAxis(2)<0.0f)
		{
			PotRPValue=-(JoystickAxis(2)*255);
		}
		else
		{
			PotRPValue=0;
		}
		PotSpareValue=(JoystickAxis(4)*127)+128;
	}
}

void PALETTE_INIT()
{
/*
	const uint8_t SuspectedDefaultPalette[512]={0x00,0x00,0x07,0x00,0x08,0x00,0x0F,0x00,0x30,0x00,0x37,0x00,0x38,0x00,0x3F,0x00,0x40,0x00,0x47,0x00,0x48,0x00,0x4F,0x00,0x70,0x00,0x77,0x00,0x78,0x00,0x7F,0x00,
						    0x80,0x00,0x87,0x00,0x88,0x00,0x8F,0x00,0xB0,0x00,0xB7,0x00,0xB8,0x00,0xBF,0x00,0xC0,0x00,0xC7,0x00,0xC8,0x00,0xCF,0x00,0xF0,0x00,0xF7,0x00,0xF8,0x00,0xFF,0x00,
						    0x00,0x00,0x33,0x03,0x55,0x05,0x77,0x07,0x99,0x09,0xBB,0x0B,0xEE,0x0E,0xFF,0x0F,0x6A,0x03,0x7F,0x05,0x9F,0x06,0xAF,0x07,0x00,0x0F,0x00,0x0C,0x00,0x09,0x7F,0x03,
						    0x07,0x00,0x07,0x03,0x07,0x06,0x08,0x08,0x0A,0x09,0x0C,0x0A,0x0F,0x0B,0x7F,0x0B,0xBF,0x0D,0x05,0x00,0x04,0x00,0x39,0x02,0xA7,0x0A,0xCA,0x0C,0xEC,0x0E,0x4B,0x02,
						    0x80,0x00,0x92,0x00,0xB6,0x01,0x60,0x00,0x30,0x04,0x20,0x00,0x38,0x04,0x3F,0x04,0x40,0x04,0x47,0x04,0x48,0x04,0x4F,0x04,0x70,0x04,0x77,0x04,0x78,0x04,0x7F,0x04,
						    0x80,0x04,0x87,0x04,0x88,0x04,0x8F,0x04,0xB0,0x04,0xB7,0x04,0xB8,0x04,0xBF,0x04,0xC0,0x04,0xC7,0x04,0xC8,0x04,0xCF,0x04,0xF0,0x04,0xF7,0x04,0xF8,0x04,0xFF,0x04,
						    0x00,0x07,0x07,0x07,0x08,0x07,0x0F,0x07,0x30,0x07,0x37,0x07,0x38,0x07,0x3F,0x07,0x40,0x07,0x47,0x07,0x48,0x07,0x4F,0x07,0x70,0x07,0x77,0x07,0x78,0x07,0x7F,0x07,
						    0x80,0x07,0x87,0x07,0x88,0x07,0x8F,0x07,0xB0,0x07,0xB7,0x07,0xB8,0x07,0xBF,0x07,0xC0,0x07,0xC7,0x07,0xC8,0x07,0xCF,0x07,0xF0,0x07,0xF7,0x07,0xF8,0x07,0xFF,0x07,
						    0x00,0x08,0x07,0x08,0x08,0x08,0x0F,0x08,0x30,0x08,0x37,0x08,0x38,0x08,0x3F,0x08,0x40,0x08,0x47,0x08,0x48,0x08,0x4F,0x08,0x70,0x08,0x77,0x08,0x78,0x08,0x7F,0x08,
						    0x80,0x08,0x87,0x08,0x88,0x08,0x8F,0x08,0xB0,0x08,0xB7,0x08,0xB8,0x08,0xBF,0x08,0xC0,0x08,0xC7,0x08,0xC8,0x08,0xCF,0x08,0xF0,0x08,0xF7,0x08,0xF8,0x08,0xFF,0x08,
						    0x00,0x0B,0x07,0x0B,0x08,0x0B,0x0F,0x0B,0x30,0x0B,0x37,0x0B,0x38,0x0B,0x3F,0x0B,0x40,0x0B,0x47,0x0B,0x48,0x0B,0x4F,0x0B,0x70,0x0B,0x77,0x0B,0x78,0x0B,0x7F,0x0B,
						    0x80,0x0B,0x87,0x0B,0x88,0x0B,0x8F,0x0B,0xB0,0x0B,0xB7,0x0B,0xB8,0x0B,0xBF,0x0B,0xC0,0x0B,0xC7,0x0B,0xC8,0x0B,0xCF,0x0B,0xF0,0x0B,0xF7,0x0B,0xF8,0x0B,0xFF,0x0B,
						    0x00,0x0C,0x07,0x0C,0x08,0x0C,0x0F,0x0C,0x30,0x0C,0x37,0x0C,0x38,0x0C,0x3F,0x0C,0x40,0x0C,0x47,0x0C,0x48,0x0C,0x4F,0x0C,0x70,0x0C,0x77,0x0C,0x78,0x0C,0x7F,0x0C,
						    0x80,0x0C,0x87,0x0C,0x88,0x0C,0x8F,0x0C,0xB0,0x0C,0xB7,0x0C,0xB8,0x0C,0xBF,0x0C,0xC0,0x0C,0xC7,0x0C,0xC8,0x0C,0xCF,0x0C,0xF0,0x0C,0xF7,0x0C,0xF8,0x0C,0xFF,0x0C,
						    0x00,0x0F,0x07,0x0F,0x08,0x0F,0x0F,0x0F,0x30,0x0F,0x37,0x0F,0x38,0x0F,0x3F,0x0F,0x40,0x0F,0x47,0x0F,0x48,0x0F,0x4F,0x0F,0x70,0x0F,0x77,0x0F,0x78,0x0F,0x7F,0x0F,
						    0x80,0x0F,0x87,0x0F,0x88,0x0F,0x8F,0x0F,0xB0,0x0F,0xB7,0x0F,0xB8,0x0F,0xBF,0x0F,0xC0,0x0F,0xC7,0x0F,0xC8,0x0F,0xCF,0x0F,0xF0,0x0F,0xF7,0x0F,0x00,0x00,0xFF,0x0F};
*/
	int a;
	for (a=0;a<256;a++)			// Setup flare 1 default palette
	{
	//	printf("Index : %d : Red %01X : Green %01X : Blue %01X\n",a,((a>>5)&7)<<1,(((a>>2)&7)<<1),((a&3)<<2));
		PALETTE[a*2+0]=(((a>>2)&7)<<5)|((a&3)<<2);//    SuspectedDefaultPalette[a*2+0];
		PALETTE[a*2+1]=((a>>5)&7)<<1;//SuspectedDefaultPalette[a*2+1];
	}

}

void MEMORY_INIT()
{
	memset(RAM,0x00,RAM_SIZE);

	numPadRowSelect=0;
	numPadState=0;
	joyPadState=0;
	buttonState=0;			// bits 4&5 are button state -- I can only assume on front of unit?? (Start/Select style) -- bits 0&1 are fire button states - stored here for convenience
	ADPSelect=0;

	PotXValue=0x80;
	PotYValue=0x10;
	PotZValue=0xFF;
	PotLPValue=0x01;
	PotRPValue=0x00;
	PotSpareValue=0x40;
}

void VECTORS_INIT()
{
	int a;
	switch (curSystem)
	{
		case ESS_CP1:
			break;
		case ESS_P89:
			break;
		case ESS_MSU:
			// Add CB to vectors used by msu emulation
			SetByte(0xE0004,0xCB);
			SetByte(0xE000C,0xCB);
			SetByte(0xE0010,0xCB);
			SetByte(0xE001C,0xCB);
			ROM[0x04]=0xCB;
			ROM[0x0C]=0xCB;
			ROM[0x10]=0xCB;
			ROM[0x1C]=0xCB;
			SetByte(0x0D000,0xCB);

		case ESS_P88:
			// Also pre-fill vector table at $0000 to point to an IRET instruction at $400 (suspect the bios is supposed to safely setup this area before booting a program)
			for (a=0;a<256;a++)
			{
				SetByte(a*4+0,0x00);
				SetByte(a*4+1,0x04);
				SetByte(a*4+2,0x00);
				SetByte(a*4+3,0x00);
			}
			SetByte(0x400,0xCF);
			break;
		case ESS_FL1:
			// Bit more comlex. RST38 is used to trigger the video interrupt (no idea on the real system, this is simply how the emulator handles it)
			//Note this is not done via a redirect table, the code is simply inserted at 0x38 ....

/*			SetByte(0x40038,0xF3);	// di
			SetByte(0x40039,0xF5);	// push af
			SetByte(0x4003A,0xDB);	// in a,(7)
			SetByte(0x4003B,0x07);
			SetByte(0x4003C,0xF1);	// pop af
			SetByte(0x4003D,0xFB);	// ei
			SetByte(0x4003E,0xC9);	// ret*/
			break;
	}
}
