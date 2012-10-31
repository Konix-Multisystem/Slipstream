/*

	ASIC test

	Currently contains some REGISTERS and some video hardware - will move to EDL eventually
*/


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "video.h"
#include "audio.h"
#include "asic.h"

#define RGB4_RGB8(x)		( ((x&0x000F)<<4) | ((x&0x00F0)<<8) | ((x&0x0F00)<<12) )				// Old multistream is 444 format
#define RGB565_RGB8(x)		( ((x&0xF800)<<8) | ((x&0x07E0) <<5) | ((x&0x001F)<<3) )				// Later revisions are 565

void INTERRUPT(uint8_t);

int hClock=0;
int vClock=0;
int VideoInterruptLatch=0;

// Current ASIC registers

uint16_t	ASIC_KINT=0xFFFF;
uint8_t		ASIC_STARTL=0;
uint8_t		ASIC_STARTH=0;
uint32_t	ASIC_SCROLL=0;
uint8_t		ASIC_MODE=0;
uint16_t	ASIC_BORD=0;
uint8_t		ASIC_PMASK=0;
uint8_t		ASIC_INDEX=0;
uint8_t		ASIC_ENDL=0;
uint8_t		ASIC_ENDH=0;
uint8_t		ASIC_MEM=0;
uint8_t		ASIC_DIAG=0;
uint8_t		ASIC_DIS=0;
uint8_t		ASIC_BLTCON=0;

void ASIC_Write(uint16_t port,uint8_t byte,int warnIgnore)
{
	switch (port)
	{
		case 0x0000:
			ASIC_KINT&=0xFF00;
			ASIC_KINT|=byte;
			break;
		case 0x0001:
			ASIC_KINT&=0x00FF;
			ASIC_KINT|=(byte<<8);
			break;
		case 0x0004:
			ASIC_STARTL=byte;
			break;
		case 0x0010:
			ASIC_SCROLL&=0x00FFFF00;
			ASIC_SCROLL|=byte;
			break;
		case 0x0012:
			ASIC_SCROLL&=0x00FF00FF;
			ASIC_SCROLL|=(byte<<8);
			break;
		case 0x0014:
			ASIC_SCROLL&=0x0000FFFF;
			ASIC_SCROLL|=(byte<<16);
			break;
		case 0x0016:
			// Clear video interrupt for now
			VideoInterruptLatch=0;
			break;
		case 0x0018:
			ASIC_MODE=byte;
			break;
		case 0x001A:
			ASIC_BORD&=0xFF00;
			ASIC_BORD|=byte;
			break;
		case 0x001B:
			ASIC_BORD&=0x00FF;
			ASIC_BORD|=(byte<<8);
			break;
		case 0x001E:
			ASIC_PMASK=byte;
			break;
		case 0x0020:
			ASIC_INDEX=byte;
			break;
		case 0x0022:
			ASIC_ENDL=byte;
			break;
		case 0x0026:
			ASIC_MEM=byte;
			break;
		case 0x002A:
			ASIC_DIAG=byte;
			break;
		case 0x002C:
			ASIC_DIS=byte;
			break;
		case 0x0044:
			ASIC_BLTCON=byte;
			break;
		default:
#if ENABLE_DEBUG
			if (warnIgnore)
			{
				printf("ASIC WRITE IGNORE %04X<-%02X - TODO?\n",port,byte);
			}
#endif
			break;
	}
}

// According to docs for PAL - 17.734475 Mhz crystal - divided by 1.5	-- 11.822983 Mhz clock
//
//  11822983 ticks / 50  = 236459.66  (236459 ticks per frame)
//  236459 / 312 = 757 clocks per line
//
// Clocks per line is approximate but probably close enough - 757 clocks - matches documentation
//
//  active display is 120 to 631 horizontal		-- From documentation
//  active display is 33 to 288 vertical		-- From documentation
//

extern unsigned char PALETTE[256*2];
void DSP_STEP(void);

void DSP_SetDAC(uint8_t channels,uint16_t value)
{
	// Bleep
	if (channels&1)
	{
		_AudioAddData(0,value);
	}
	if (channels&2)
	{
		_AudioAddData(1,value);
	}
}

#if ENABLE_DEBUG

extern uint8_t *DSP_DIS_[32];			// FROM EDL

extern uint16_t	DSP_PC;
extern uint16_t	DSP_IX;
extern uint16_t	DSP_MZ0;
extern uint16_t	DSP_MZ1;
extern uint16_t	DSP_MZ2;
extern uint16_t	DSP_MODE;
extern uint16_t	DSP_X;
extern uint16_t	DSP_AZ;
	
uint16_t DSP_GetProgWord(uint16_t address);

void DSP_DUMP_REGISTERS()
{
	printf("--------\n");
	printf("FLAGS = C\n");
	printf("        %s\n",	DSP_MZ2&0x10?"1":"0");
	printf("IX = %04X\n",DSP_IX);
	printf("MZ0= %04X\n",DSP_MZ0);
	printf("MZ1= %04X\n",DSP_MZ1);
	printf("MZ2= %04X\n",DSP_MZ2&0xF);
	printf("MDE= %04X\n",DSP_MODE);
	printf("X  = %04X\n",DSP_X);
	printf("AZ = %04X\n",DSP_AZ);
	printf("--------\n");
}

const char* DSP_LookupAddress(uint16_t address)
{
	static char sprintBuffer[256];

	switch (address)
	{
		case 0x014A:
			return "PC";
		case 0x0141:
			return "IX";
		case 0x0145:
			return "MZ0";
		case 0x0146:
			return "MZ1";
		case 0x0147:
			return "MZ2";
		case 0x014B:
			return "MODE";
		case 0x014C:
			return "X";
		case 0x014D:
			return "AZ";
		default:
			break;
	}

	sprintf(sprintBuffer,"(%04X)",address);
	return sprintBuffer;
}

const char* DSP_decodeDisasm(uint8_t *table[32],unsigned int address)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];
	uint16_t word=DSP_GetProgWord(address);
	uint16_t data=word&0x1FF;
	int index=(word&0x200)>>9;
	const char* mnemonic=(char*)table[(word&0xF800)>>11];
	const char* sPtr=mnemonic;
	char* dPtr=temporaryBuffer;
	int counting = 0;
	int doingDecode=0;

	if (sPtr==NULL)
	{
		sprintf(temporaryBuffer,"UNKNOWN OPCODE");
		return temporaryBuffer;
	}
	
	while (*sPtr)
	{
		if (!doingDecode)
		{
			if (*sPtr=='%')
			{
				doingDecode=1;
			}
			else
			{
				*dPtr++=*sPtr;
			}
		}
		else
		{
			char *tPtr=sprintBuffer;
			int negOffs=1;
			if (*sPtr=='-')
			{
				sPtr++;
				negOffs=-1;
			}

			if (index)
			{
				sprintf(sprintBuffer,"(%04X%s)",data,"+IX");
			}
			else
			{
				sprintf(sprintBuffer,"%s",DSP_LookupAddress(data));
			}
			while (*tPtr)
			{
				*dPtr++=*tPtr++;
			}
			doingDecode=0;
			counting++;
		}
		sPtr++;
	}
	*dPtr=0;
	
	return temporaryBuffer;
}

int DSP_Disassemble(unsigned int address,int registers)
{
	const char* retVal = DSP_decodeDisasm(DSP_DIS_,address);

	if (strcmp(retVal,"UNKNOWN OPCODE")==0)
	{
		printf("UNKNOWN AT : %04X\n",address);
		printf("%04X ",DSP_GetProgWord(address));
		printf("\n");
		DSP_DUMP_REGISTERS();
		exit(-1);
	}

	if (registers)
	{
		DSP_DUMP_REGISTERS();
	}
	printf("%04X :",address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see

	printf("%04X ",DSP_GetProgWord(address));
	printf("   ");
	printf("%s\n",retVal);

	return 1;
}

#endif

extern unsigned char DSP[4*1024];

void DoDSP()
{
#if !DISABLE_DSP
	static int iamslow=10;
	if (DSP[0xFF0]==0x10)
	{
		if (iamslow==0)
		{

#if ENABLE_DEBUG
		DSP_Disassemble(DSP_PC,1);
#endif
		DSP_STEP();

			iamslow=10;
		}
		else
			iamslow--;
	}
#endif
}

uint8_t PeekByte(uint32_t addr);

void TickAsic(int cycles)
{
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	uint32_t screenPtr = ASIC_SCROLL;
	outputTexture+=vClock*WIDTH + hClock;
	while (cycles)
	{
		DoDSP();

		// This is a quick hack up of the screen functionality -- at present simply timing related to get interrupts to fire
		if (VideoInterruptLatch)
		{
			INTERRUPT(0x21);
		}

		// Quick and dirty video display no contention or bus cycles
		if (hClock>=120 && hClock<632 && vClock>ASIC_STARTL && vClock<=ASIC_ENDL)
		{
			uint8_t palIndex = PeekByte(screenPtr + ((vClock-ASIC_STARTL)-1)*256 + (hClock-120)/2);
			uint16_t palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];

			*outputTexture++=RGB565_RGB8(palEntry);
		}
		else
		{
			*outputTexture++=RGB565_RGB8(ASIC_BORD);
		}

		hClock++;
		if ((hClock==631) && (ASIC_KINT==vClock) && ((ASIC_DIS&0x1)==0))			//  Docs state interrupt fires at end of active display of KINT line
		{
			VideoInterruptLatch=1;
		}
		if (hClock==(WIDTH-1))
		{
			hClock=0;
			vClock++;
			if (vClock==(HEIGHT-1))
			{
				vClock=0;
			}
		}

		cycles--;
	}
}
