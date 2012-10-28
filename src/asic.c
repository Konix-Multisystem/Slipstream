/*

	ASIC test

	Currently contains some REGISTERS and some video hardware - will move to EDL eventually
*/


#include <stdio.h>
#include <stdint.h>

#include "video.h"
#include "asic.h"

#define RGB4_RGB8(x)		( ((x&0x000F)<<4) | ((x&0x00F0)<<8) | ((x&0x0F00)<<12) )

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

void ASIC_Write(uint16_t port,uint8_t byte)
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
			printf("ASIC WRITE IGNORE %04X<-%02X - TODO?\n",port,byte);
			printf("Start Address : %08X\n",ASIC_SCROLL);
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

void TickAsic(int cycles)
{
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	uint32_t screenPtr = ASIC_SCROLL;
	outputTexture+=vClock*WIDTH + hClock;
	while (cycles)
	{
		// This is a quick hack up of the screen functionality -- at present simply timing related to get interrupts to fire
		if (VideoInterruptLatch)
		{
			INTERRUPT(0x21);
		}

		// Quick and dirty video display no contention or bus cycles
		if (hClock>=120 && hClock<632 && vClock>ASIC_STARTL && vClock<=ASIC_ENDL)
		{
			uint8_t palIndex = GetByte(screenPtr + ((vClock-ASIC_STARTL)-1)*256 + (hClock-120)/2);
			uint16_t palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];

			*outputTexture++=RGB4_RGB8(palEntry);
		}
		else
		{
			*outputTexture++=RGB4_RGB8(ASIC_BORD);
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
