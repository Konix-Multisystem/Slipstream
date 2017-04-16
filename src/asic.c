/*

	ASIC test

	Currently contains some REGISTERS and some video hardware - will move to EDL eventually

	Need to break this up some more, blitter going here temporarily
*/


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if OS_WINDOWS
#include <conio.h>
#endif

#include "system.h"
#include "logfile.h"
#include "video.h"
#include "audio.h"
#include "asic.h"
#include "dsp.h"

#define RGB444_RGB8(x)		( ((x&0x000F)<<4) | ((x&0x00F0)<<8) | ((x&0x0F00)<<12) )				// Old slipstream is 444 format
#define RGB565_RGB8(x)		( ((x&0xF800)<<8) | ((x&0x07E0) <<5) | ((x&0x001F)<<3) )				// Later revisions are 565

#if ENABLE_DEBUG
#define ENABLE_DEBUG_BLITTER	1
#define BLTDDBG(...)		//if (doShowBlits) { CONSOLE_OUTPUT(__VA_ARGS__); }
#else
#define ENABLE_DEBUG_BLITTER	0
#define BLTDDBG(...)		//if (doShowBlits) { CONSOLE_OUTPUT(__VA_ARGS__); }
#endif

extern unsigned char PALETTE[256*2];
extern int doShowPortStuff;
extern int doDebug;

void INTERRUPT(uint8_t);
void Z80_INTERRUPT(uint8_t);
void FL1DSP_RESET();

extern uint16_t FL1DSP_PC;
extern uint16_t DSP_STATUS;

int doShowBlits=0;

// Current ASIC registers
int hClock=0;
int vClock=0;
int VideoInterruptLatch=0;


uint16_t	ASIC_KINT=0x00FF;
uint8_t		ASIC_STARTL=33;
uint8_t		ASIC_STARTH=0;
uint32_t	ASIC_SCROLL=0;
uint8_t		ASIC_MODE=0;
uint16_t	ASIC_BORD=0;
uint8_t		ASIC_PMASK=0;
uint8_t		ASIC_INDEX=0;
uint8_t		ASIC_ENDL=33;
uint8_t		ASIC_ENDH=1;
uint8_t		ASIC_MEM=0;
uint8_t		ASIC_DIAG=0;
uint8_t		ASIC_DIS=0;
uint8_t		ASIC_BLTCON=0;
uint8_t		ASIC_BLTCMD=0;
uint8_t		ASIC_BLTENH=0;
uint32_t	ASIC_BLTPC=0;				// 20 bit address
uint8_t		ASIC_COLHOLD=0;					// Not changeable on later than Flare One revision


uint8_t		ASIC_FDC=0xFF;				// P89 Floppy Disk Controller
uint32_t	ASIC_FRC=0x00000000;			// P89 Floppy Disk Controller

uint8_t		ASIC_PROGCNT=0;		// FL1 Only
uint16_t	ASIC_PROGWRD=0;
uint16_t	ASIC_PROGADDR=0;
uint16_t	ASIC_INTRA=0;
uint16_t	ASIC_INTRD=0;
uint8_t		ASIC_INTRCNT=0;

uint8_t		ASIC_PALAW=0;		// FL1 Palette Board Extension
uint8_t		ASIC_PALAR=0;
uint32_t	ASIC_PALVAL=0;
uint8_t		ASIC_PALCNT=0;
uint8_t		ASIC_PALMASK=0xFF;

uint8_t		ASIC_PALSTORE[3*256];


uint32_t	ASIC_BANK0=0;				// Z80 banking registers  (stored in upper 16bits)
uint32_t	ASIC_BANK1=0;
uint32_t	ASIC_BANK2=0;
uint32_t	ASIC_BANK3=0;

uint8_t BLT_OUTER_SRC_FLAGS;
uint8_t BLT_OUTER_DST_FLAGS;
uint8_t BLT_OUTER_CMD;
uint32_t BLT_OUTER_SRC;
uint32_t BLT_OUTER_DST;
uint8_t BLT_OUTER_MODE;
uint8_t BLT_OUTER_CPLG;
uint8_t BLT_OUTER_CNT;
uint16_t BLT_INNER_CNT;
uint8_t BLT_INNER_STEP;
uint8_t BLT_INNER_PAT;
uint8_t	BLT_ENH;				// upper 3 bits representing the enhancment to the step function : ENSTP8 ENSTPS ENSTEP  (89 and possibly MSU)

uint32_t ADDRESSGENERATOR_SRCADDRESS;			// 21 bit  - LSB = nibble
uint32_t ADDRESSGENERATOR_DSTADDRESS;			// 21 bit  - LSB = nibble

uint16_t DATAPATH_SRCDATA;
uint8_t DATAPATH_DSTDATA;
uint8_t DATAPATH_PATDATA;
uint16_t DATAPATH_DATAOUT;

uint16_t BLT_INNER_CUR_CNT;

// EDDY - Floppy Disk Controller - P89
int8_t EDDY_Track=78;				// head position 
int8_t EDDY_Side=0;
uint32_t EDDY_BitPos=0;				// ~bit position on track
int8_t EDDY_Index=0;
uint16_t EDDY_State=0;				// (Just Bits 12-14 for now -- 000 IDLE 001 Waiting Index 111 Freq Locking 010 Waiting AMark 110 AMark 111 Reading Sector Data

#define INDEX_PULSE_FREQ	((WIDTH*HEIGHT*50)/300)		// Approximate
#define INDEX_PULSE_WIDTH	(((WIDTH*HEIGHT*50)/300)/8000)		// Approximate

uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);


void DoBlit();

void TickBlitterMSU()								// TODO - make this more modular!!!
{
	if (ASIC_BLTCMD & 1)
	{
	// Step one, make the blitter "free"
#if ENABLE_DEBUG_BLITTER
	if (doShowBlits)
	{
		CONSOLE_OUTPUT("Blitter Command : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
			ASIC_BLTCMD&0x02?1:0,
			ASIC_BLTCMD&0x04?1:0,
			ASIC_BLTCMD&0x08?1:0,
			ASIC_BLTCMD&0x10?1:0,
			ASIC_BLTCMD&0x20?1:0,
			ASIC_BLTCMD&0x40?1:0,
			ASIC_BLTCMD&0x80?1:0);
	}
#endif

		BLT_OUTER_CMD=ASIC_BLTCMD;		// First time through we don't read the command		-- Note the order of data appears to differ from the docs - This is true of MSU version!!
		ASIC_BLTCMD=0;

		do
		{
#if ENABLE_DEBUG_BLITTER
		if (doShowBlits)
		{
			CONSOLE_OUTPUT("Starting Blit : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
				BLT_OUTER_CMD&0x02?1:0,
				BLT_OUTER_CMD&0x04?1:0,
				BLT_OUTER_CMD&0x08?1:0,
				BLT_OUTER_CMD&0x10?1:0,
				BLT_OUTER_CMD&0x20?1:0,
				BLT_OUTER_CMD&0x40?1:0,
				BLT_OUTER_CMD&0x80?1:0);
		}

		if (BLT_OUTER_CMD&0x4E)
		{
			CONSOLE_OUTPUT("Unsupported BLT CMD type\n");
			exit(1);
		}


		if (doShowBlits)
		{
			CONSOLE_OUTPUT("Fetching Program Sequence :\n");
		}
#endif
		BLT_OUTER_SRC=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		BLT_OUTER_SRC|=GetByte(ASIC_BLTPC)<<8;
		ASIC_BLTPC++;
		BLT_OUTER_SRC_FLAGS=GetByte(ASIC_BLTPC);
		BLT_OUTER_SRC|=(BLT_OUTER_SRC_FLAGS&0xF)<<16;
		ASIC_BLTPC++;
		

		BLT_OUTER_CNT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_OUTER_DST=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		BLT_OUTER_DST|=GetByte(ASIC_BLTPC)<<8;
		ASIC_BLTPC++;
		BLT_OUTER_DST_FLAGS=GetByte(ASIC_BLTPC);
		BLT_OUTER_DST|=(BLT_OUTER_DST_FLAGS&0xF)<<16;
		ASIC_BLTPC++;
		

		BLT_OUTER_CPLG=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_CNT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_OUTER_MODE=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_PAT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_STEP=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;

#if ENABLE_DEBUG_BLITTER
		if (doShowBlits)
		{
			CONSOLE_OUTPUT("Src Address : %05X\n",BLT_OUTER_SRC&0xFFFFF);
			CONSOLE_OUTPUT("Outer Cnt : %02X\n",BLT_OUTER_CNT);
			CONSOLE_OUTPUT("Dst Address : %05X\n",BLT_OUTER_DST&0xFFFFF);
			CONSOLE_OUTPUT("Comp Logic : %02X\n",BLT_OUTER_CPLG);
			CONSOLE_OUTPUT("Inner Count : %02X\n",BLT_INNER_CNT);
			CONSOLE_OUTPUT("Mode Control : %02X\n",BLT_OUTER_MODE);
			CONSOLE_OUTPUT("Pattern : %02X\n",BLT_INNER_PAT);
			CONSOLE_OUTPUT("Step : %02X\n",BLT_INNER_STEP);
		}
#endif
		DoBlit();
		
		ASIC_BLTPC++;		// skip segment address
		BLT_OUTER_CMD=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		}
		while (BLT_OUTER_CMD&1);

//		exit(1);

	}
}

void TickBlitterP89()								// NOTE MSU and THIS may turn out to be identical -- need to go back and check!
{
	if (ASIC_BLTCMD & 1)
	{
	// Step one, make the blitter "free"
#if ENABLE_DEBUG_BLITTER
	if (doShowBlits)
	{
		CONSOLE_OUTPUT("Blitter Command : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
			ASIC_BLTCMD&0x02?1:0,
			ASIC_BLTCMD&0x04?1:0,
			ASIC_BLTCMD&0x08?1:0,
			ASIC_BLTCMD&0x10?1:0,
			ASIC_BLTCMD&0x20?1:0,
			ASIC_BLTCMD&0x40?1:0,
			ASIC_BLTCMD&0x80?1:0);
	}
#endif

		BLT_OUTER_CMD=ASIC_BLTCMD;		// First time through we don't read the command		-- Note the order of data appears to differ from the docs - This is true of P89 version!!
		BLT_ENH=ASIC_BLTENH;
		ASIC_BLTCMD=0;
		ASIC_BLTENH=0;

		do
		{
#if ENABLE_DEBUG_BLITTER
		if (doShowBlits)
		{
			CONSOLE_OUTPUT("Starting Blit : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
				BLT_OUTER_CMD&0x02?1:0,
				BLT_OUTER_CMD&0x04?1:0,
				BLT_OUTER_CMD&0x08?1:0,
				BLT_OUTER_CMD&0x10?1:0,
				BLT_OUTER_CMD&0x20?1:0,
				BLT_OUTER_CMD&0x40?1:0,
				BLT_OUTER_CMD&0x80?1:0);
			CONSOLE_OUTPUT("Enhanced Step : ENSTEP (%d), ENSTP8 (%d), ENSTPS (%d)\n",
				BLT_ENH&0x20?1:0,
				BLT_ENH&0x40?1:0,
				BLT_ENH&0x80?1:0);
		}

		if (BLT_OUTER_CMD&0x02)
		{
			CONSOLE_OUTPUT("Unsupported BLT CMD type -- COLLISION STOP -- \n");
		}


		if (doShowBlits)
		{
			CONSOLE_OUTPUT("Fetching Program Sequence :\n");
		}
#endif
		BLT_OUTER_SRC=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		BLT_OUTER_SRC|=GetByte(ASIC_BLTPC)<<8;
		ASIC_BLTPC++;
		BLT_OUTER_SRC_FLAGS=GetByte(ASIC_BLTPC);
		BLT_OUTER_SRC|=(BLT_OUTER_SRC_FLAGS&0xF)<<16;
		ASIC_BLTPC++;
		

		BLT_OUTER_CNT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_OUTER_DST=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		BLT_OUTER_DST|=GetByte(ASIC_BLTPC)<<8;
		ASIC_BLTPC++;
		BLT_OUTER_DST_FLAGS=GetByte(ASIC_BLTPC);
		BLT_OUTER_DST|=(BLT_OUTER_DST_FLAGS&0xF)<<16;
		ASIC_BLTPC++;
		

		BLT_OUTER_CPLG=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_CNT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_OUTER_MODE=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_PAT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_STEP=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;

#if ENABLE_DEBUG_BLITTER
		if (doShowBlits)
		{
			CONSOLE_OUTPUT("Src Address : %05X\n",BLT_OUTER_SRC&0xFFFFF);
			CONSOLE_OUTPUT("Outer Src Flags : SRCCMP (%d) , SWRAP (%d) , SSIGN (%d) , SRCA-1 (%d)\n",
				BLT_OUTER_SRC_FLAGS&0x10?1:0,
				BLT_OUTER_SRC_FLAGS&0x20?1:0,
				BLT_OUTER_SRC_FLAGS&0x40?1:0,
				BLT_OUTER_SRC_FLAGS&0x80?1:0);
			CONSOLE_OUTPUT("Outer Cnt : %02X\n",BLT_OUTER_CNT);
			CONSOLE_OUTPUT("Dst Address : %05X\n",BLT_OUTER_DST&0xFFFFF);
			CONSOLE_OUTPUT("Outer Dst Flags : DSTCMP (%d) , DWRAP (%d) , DSIGN (%d) , DSTA-1 (%d)\n",
				BLT_OUTER_DST_FLAGS&0x10?1:0,
				BLT_OUTER_DST_FLAGS&0x20?1:0,
				BLT_OUTER_DST_FLAGS&0x40?1:0,
				BLT_OUTER_DST_FLAGS&0x80?1:0);
			CONSOLE_OUTPUT("Comp Logic : %02X\n",BLT_OUTER_CPLG);
			CONSOLE_OUTPUT("Inner Count : %02X\n",BLT_INNER_CNT);
			CONSOLE_OUTPUT("Mode Control : %02X\n",BLT_OUTER_MODE);
			CONSOLE_OUTPUT("Pattern : %02X\n",BLT_INNER_PAT);
			CONSOLE_OUTPUT("Step : %02X\n",BLT_INNER_STEP);
		}
#endif
		DoBlit();
		
		BLT_ENH=GetByte(ASIC_BLTPC)&0xE0;
		ASIC_BLTPC++;
		BLT_OUTER_CMD=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		}
		while (BLT_OUTER_CMD&1);

//		exit(1);

	}
}



void TickBlitterP88()
{
	if (ASIC_BLTCMD & 1)
	{
	// Step one, make the blitter "free"
#if ENABLE_DEBUG_BLITTER
	if (doShowBlits)
	{
		CONSOLE_OUTPUT("Blitter Command : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
			ASIC_BLTCMD&0x02?1:0,
			ASIC_BLTCMD&0x04?1:0,
			ASIC_BLTCMD&0x08?1:0,
			ASIC_BLTCMD&0x10?1:0,
			ASIC_BLTCMD&0x20?1:0,
			ASIC_BLTCMD&0x40?1:0,
			ASIC_BLTCMD&0x80?1:0);
	}
#endif

		BLT_OUTER_CMD=ASIC_BLTCMD;		// First time through we don't read the command	
		ASIC_BLTCMD=0;

		do
		{
#if ENABLE_DEBUG_BLITTER
		if (doShowBlits)
		{
			CONSOLE_OUTPUT("Starting Blit : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
				BLT_OUTER_CMD&0x02?1:0,
				BLT_OUTER_CMD&0x04?1:0,
				BLT_OUTER_CMD&0x08?1:0,
				BLT_OUTER_CMD&0x10?1:0,
				BLT_OUTER_CMD&0x20?1:0,
				BLT_OUTER_CMD&0x40?1:0,
				BLT_OUTER_CMD&0x80?1:0);
		}

		if (BLT_OUTER_CMD&0x02)
		{
			CONSOLE_OUTPUT("Unsupported BLT CMD type\n");
			exit(1);
		}


		if (doShowBlits)
		{
			CONSOLE_OUTPUT("Fetching Program Sequence :\n");
		}
#endif
		BLT_OUTER_SRC=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		BLT_OUTER_SRC|=GetByte(ASIC_BLTPC)<<8;
		ASIC_BLTPC++;
		BLT_OUTER_SRC_FLAGS=GetByte(ASIC_BLTPC);
		BLT_OUTER_SRC|=(BLT_OUTER_SRC_FLAGS&0xF)<<16;
		ASIC_BLTPC++;
		

		BLT_OUTER_DST=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		BLT_OUTER_DST|=GetByte(ASIC_BLTPC)<<8;
		ASIC_BLTPC++;
		BLT_OUTER_DST_FLAGS=GetByte(ASIC_BLTPC);
		BLT_OUTER_DST|=(BLT_OUTER_DST_FLAGS&0xF)<<16;
		ASIC_BLTPC++;
		

		BLT_OUTER_MODE=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_OUTER_CPLG=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_OUTER_CNT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_CNT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_STEP=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_PAT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;

#if ENABLE_DEBUG_BLITTER
		if (doShowBlits)
		{
			CONSOLE_OUTPUT("BLIT CMD : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
				BLT_OUTER_CMD&0x02?1:0,
				BLT_OUTER_CMD&0x04?1:0,
				BLT_OUTER_CMD&0x08?1:0,
				BLT_OUTER_CMD&0x10?1:0,
				BLT_OUTER_CMD&0x20?1:0,
				BLT_OUTER_CMD&0x40?1:0,
				BLT_OUTER_CMD&0x80?1:0);
			CONSOLE_OUTPUT("Src Address : %05X\n",BLT_OUTER_SRC&0xFFFFF);
			CONSOLE_OUTPUT("Src Flags : SRCCMP (%d) , SWRAP (%d) , SSIGN (%d) , SRCA-1 (%d)\n",
				BLT_OUTER_SRC_FLAGS&0x10?1:0,
				BLT_OUTER_SRC_FLAGS&0x20?1:0,
				BLT_OUTER_SRC_FLAGS&0x40?1:0,
				BLT_OUTER_SRC_FLAGS&0x80?1:0);
			CONSOLE_OUTPUT("Dst Address : %05X\n",BLT_OUTER_DST&0xFFFFF);
			CONSOLE_OUTPUT("Dst Flags : DSTCMP (%d) , DWRAP (%d) , DSIGN (%d) , DSTA-1 (%d)\n",
				BLT_OUTER_DST_FLAGS&0x10?1:0,
				BLT_OUTER_DST_FLAGS&0x20?1:0,
				BLT_OUTER_DST_FLAGS&0x40?1:0,
				BLT_OUTER_DST_FLAGS&0x80?1:0);
			CONSOLE_OUTPUT("BLT_MODE : STEP-1 (%d) , ILCNT (%d) , CMPBIT (%d) , LINDR (%d) , YFRAC (%d) , RES0 (%d) , RES1 (%d), PATSEL (%d)\n",
				BLT_OUTER_MODE&0x01?1:0,
				BLT_OUTER_MODE&0x02?1:0,
				BLT_OUTER_MODE&0x04?1:0,
				BLT_OUTER_MODE&0x08?1:0,
				BLT_OUTER_MODE&0x10?1:0,
				BLT_OUTER_MODE&0x20?1:0,
				BLT_OUTER_MODE&0x40?1:0,
				BLT_OUTER_MODE&0x80?1:0);
			CONSOLE_OUTPUT("BLT_COMP : CMPEQ (%d) , CMPNE (%d) , CMPGT (%d) , CMPLN (%d) , LOG0 (%d) , LOG1 (%d) , LOG2 (%d), LOG3 (%d)\n",
				BLT_OUTER_CPLG&0x01?1:0,
				BLT_OUTER_CPLG&0x02?1:0,
				BLT_OUTER_CPLG&0x04?1:0,
				BLT_OUTER_CPLG&0x08?1:0,
				BLT_OUTER_CPLG&0x10?1:0,
				BLT_OUTER_CPLG&0x20?1:0,
				BLT_OUTER_CPLG&0x40?1:0,
				BLT_OUTER_CPLG&0x80?1:0);
			CONSOLE_OUTPUT("Outer Cnt : %02X\n",BLT_OUTER_CNT);
			CONSOLE_OUTPUT("Inner Count : %02X\n",BLT_INNER_CNT);
			CONSOLE_OUTPUT("Step : %02X\n",BLT_INNER_STEP);
			CONSOLE_OUTPUT("Pattern : %02X\n",BLT_INNER_PAT);

			//getch();
		}
#endif
		DoBlit();

		BLT_OUTER_CMD=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
#if ENABLE_DEBUG_BLITTER
		if (doShowBlits)
		{
			CONSOLE_OUTPUT("Next Blit : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
				BLT_OUTER_CMD&0x02?1:0,
				BLT_OUTER_CMD&0x04?1:0,
				BLT_OUTER_CMD&0x08?1:0,
				BLT_OUTER_CMD&0x10?1:0,
				BLT_OUTER_CMD&0x20?1:0,
				BLT_OUTER_CMD&0x40?1:0,
				BLT_OUTER_CMD&0x80?1:0);
		}
#endif
		}
		while (BLT_OUTER_CMD&1);

//		exit(1);

	}
}

extern int pause;

uint8_t FL1BLT_GetByte(uint32_t addr)
{
	//printf("Blitter Fetch : %05X -> %02X\n",addr,GetByte(addr));
	return GetByte(addr);
}

void FL1BLT_SetByte(uint32_t addr,uint8_t byte)
{
	//printf("Blitter Store : %05X <- %02X\n",addr,byte);
	SetByte(addr,byte);
}


void FL1BLT_SetProgLow(uint8_t addr);
void FL1BLT_SetProgMiddle(uint8_t addr);
void FL1BLT_SetProgHi(uint8_t addr);
void FL1BLT_SetCmd(uint8_t byte);
uint8_t FL1BLT_GetDstLo();
uint8_t FL1BLT_GetDstMi();
uint32_t FL1BLT_Step(uint8_t hold);

void TickBlitterFL1()
{
#if 0

/*	while (FL1BLT_Step(0))
	{
		pause=1;
	}*/

#else
	// Flare One blitter seems to be quite different - Going to try some hackery to make it work on a case by case basis for now

	if (ASIC_BLTCMD & 1)
	{
	// Step one, make the blitter "free"
		BLT_OUTER_CMD=ASIC_BLTCMD;		// First time through we don't read the command	
		ASIC_BLTCMD=0;

		do
		{
	
		BLT_OUTER_SRC=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		BLT_OUTER_SRC|=GetByte(ASIC_BLTPC)<<8;
		ASIC_BLTPC++;
		BLT_OUTER_SRC_FLAGS=GetByte(ASIC_BLTPC);			// The flags probably don't exist
		BLT_OUTER_SRC|=(BLT_OUTER_SRC_FLAGS&0xF)<<16;
		ASIC_BLTPC++;
		

		BLT_OUTER_DST=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		BLT_OUTER_DST|=GetByte(ASIC_BLTPC)<<8;
		ASIC_BLTPC++;
		BLT_OUTER_DST_FLAGS=GetByte(ASIC_BLTPC);			// The flags probably don't exist
		BLT_OUTER_DST|=(BLT_OUTER_DST_FLAGS&0xF)<<16;
		ASIC_BLTPC++;
		

		BLT_OUTER_MODE=GetByte(ASIC_BLTPC);				// MODE differs -   ? ? UNPACK  REMAP? YFRAC DSIGN SSIGN
		ASIC_BLTPC++;


		BLT_OUTER_CPLG=GetByte(ASIC_BLTPC);				// LOGIC and COMP probably stay the same
		ASIC_BLTPC++;


		BLT_OUTER_CNT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_CNT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_STEP=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_INNER_PAT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


			CONSOLE_OUTPUT("CMD %02X  (LINE -%s)(LT1 -%s)(LT0 -%s)(DSTUP -%s)(SRCUP -%s)(PARD - %s)(COLST - %s)(RUN - %s) | ",
				BLT_OUTER_CMD,
				(BLT_OUTER_CMD&0x80)?"1":"0",
				(BLT_OUTER_CMD&0x40)?"1":"0",
				(BLT_OUTER_CMD&0x20)?"1":"0",
				(BLT_OUTER_CMD&0x10)?"1":"0",
				(BLT_OUTER_CMD&0x08)?"1":"0",
				(BLT_OUTER_CMD&0x04)?"1":"0",
				(BLT_OUTER_CMD&0x02)?"1":"0",
				(BLT_OUTER_CMD&0x01)?"1":"0"
				);

			CONSOLE_OUTPUT("Src Address : %05X | ",BLT_OUTER_SRC&0xFFFFF);
			CONSOLE_OUTPUT("Src Flags : %01X | ",((BLT_OUTER_SRC_FLAGS&0xF0)>>4));
			CONSOLE_OUTPUT("Dst Address : %05X | ",BLT_OUTER_DST&0xFFFFF);
			CONSOLE_OUTPUT("Dst Flags : %01X | ",((BLT_OUTER_DST_FLAGS&0xF0)>>4));
			CONSOLE_OUTPUT("MOD %02X  (SSIGN -%s)(DSIGN -%s)(YFRAC -%s)(LKUP -%s)(HIRES -%s)(CMPBIT - %s)(ILCNT8 - %s)(STEP8 - %s) | ",
				BLT_OUTER_MODE,
				(BLT_OUTER_MODE&0x80)?"1":"0",
				(BLT_OUTER_MODE&0x40)?"1":"0",
				(BLT_OUTER_MODE&0x20)?"1":"0",
				(BLT_OUTER_MODE&0x10)?"1":"0",
				(BLT_OUTER_MODE&0x08)?"1":"0",
				(BLT_OUTER_MODE&0x04)?"1":"0",
				(BLT_OUTER_MODE&0x02)?"1":"0",
				(BLT_OUTER_MODE&0x01)?"1":"0"
				);
			CONSOLE_OUTPUT("CPLG : %02X | ",BLT_OUTER_CPLG);
			CONSOLE_OUTPUT("Outer Cnt : %02X | ",BLT_OUTER_CNT);
			CONSOLE_OUTPUT("Inner Count : %02X : %02X(%d) | ",BLT_INNER_CNT,BLT_INNER_CNT+((BLT_OUTER_MODE&0x02)<<7),BLT_INNER_CNT+((BLT_OUTER_MODE&0x02)<<7));
			CONSOLE_OUTPUT("Step : %02X.%d | ",BLT_INNER_STEP,BLT_OUTER_MODE&1);
			CONSOLE_OUTPUT("Pattern : %02X\n",BLT_INNER_PAT);


#if ENABLE_DEBUG_BLITTER
		if (doShowBlits)
		{
			CONSOLE_OUTPUT("CMD %02X  (LINE -%s)(MODE? -%s)(XXX -%s)(DSTUP -%s)(SRCUP -%s)(PARD - %s)(COLST - %s)(RUN - %s) | ",
				BLT_OUTER_CMD,
				(BLT_OUTER_CMD&0x80)?"1":"0",
				(BLT_OUTER_CMD&0x40)?"1":"0",
				(BLT_OUTER_CMD&0x20)?"1":"0",
				(BLT_OUTER_CMD&0x10)?"1":"0",
				(BLT_OUTER_CMD&0x08)?"1":"0",
				(BLT_OUTER_CMD&0x04)?"1":"0",
				(BLT_OUTER_CMD&0x02)?"1":"0",
				(BLT_OUTER_CMD&0x01)?"1":"0"
				);

			CONSOLE_OUTPUT("Src Address : %05X | ",BLT_OUTER_SRC&0xFFFFF);
			CONSOLE_OUTPUT("Src Flags : %01X | ",((BLT_OUTER_SRC_FLAGS&0xF0)>>4));
			CONSOLE_OUTPUT("Dst Address : %05X | ",BLT_OUTER_DST&0xFFFFF);
			CONSOLE_OUTPUT("Dst Flags : %01X | ",((BLT_OUTER_DST_FLAGS&0xF0)>>4));
			CONSOLE_OUTPUT("MODE : %02X | ",BLT_OUTER_MODE);
			CONSOLE_OUTPUT("CPLG : %02X | ",BLT_OUTER_CPLG);
			CONSOLE_OUTPUT("Outer Cnt : %02X | ",BLT_OUTER_CNT);
			CONSOLE_OUTPUT("Inner Count : %02X | ",BLT_INNER_CNT);
			CONSOLE_OUTPUT("Step : %02X | ",BLT_INNER_STEP);
			CONSOLE_OUTPUT("Pattern : %02X\n",BLT_INNER_PAT);
		}
#endif

		// Getting closer to the correct layout of the blitter command structure - now there is really just confusion over 1 or 2 bits
		//Think the blitter cmd breaks down as follows :
		//						RUN | COLST | PARD | SRCUP | DSTUP | xx | mode? (hires/nibble=1) | line

		if (BLT_OUTER_CPLG&07)
		{
			CONSOLE_OUTPUT("wrn - unknown Compare bits\n");
		}
		BLT_OUTER_CPLG=(BLT_OUTER_CPLG&0xF0)|(BLT_OUTER_CPLG&0x08?0x01:0x00);					// LOG combinations appear to be the same, 

		switch (BLT_OUTER_CMD&0xE0)
		{
			default:
				CONSOLE_OUTPUT("wrn - Unknown BLTCMD %02X\n",BLT_OUTER_CMD);
				pause=1;
				break;

			case 0x20:
				// Appears to be block copy
				if ((BLT_OUTER_MODE&0xFB)!=0)			// 0x04 = font mode
				{
					CONSOLE_OUTPUT("wrn - BLTMODE unknown bits (0x20 %02X)\n",BLT_OUTER_MODE&0xFB);
					//pause=1;
				}

				BLT_OUTER_CMD=0x20 | (BLT_OUTER_CMD&0x1F);
				BLT_OUTER_SRC_FLAGS=(BLT_OUTER_MODE&0x80)?0x40:0x00;
				BLT_OUTER_DST_FLAGS=BLT_OUTER_MODE&0x40;
				if (BLT_OUTER_CPLG&0x01)
				{
					BLT_OUTER_SRC_FLAGS|=0x10;
				}
				BLT_OUTER_MODE=0x20;
				if (BLT_INNER_CNT==0)
				{
					BLT_OUTER_MODE|=0x2;		//Clamp to 8 bit count
				}
				DoBlit();
				break;
			case 0x60:
				// Used when doing Mode 4
				if (BLT_OUTER_MODE!=0x04)
				{
//					CONSOLE_OUTPUT("wrn - BLTMODE unknown bits (0x60 %d)\n",BLT_OUTER_MODE&0xFB);
//					CONSOLE_OUTPUT("wrn - 0x71 command but mode not set to known value\n");
					if ((BLT_OUTER_MODE&0xFB)!=0)			// 0x04 = font mode
					{
						CONSOLE_OUTPUT("wrn - BLTMODE unknown bits (0x20 %02X)\n",BLT_OUTER_MODE&0xFB);
						//pause=1;
					}

					BLT_OUTER_CMD=0x60 | (BLT_OUTER_CMD&0x1F);
					BLT_OUTER_SRC_FLAGS=(BLT_OUTER_MODE&0x80)?0x40:0x00;
					BLT_OUTER_DST_FLAGS=BLT_OUTER_MODE&0x40;
					if (BLT_OUTER_CPLG&0x01)
					{
						BLT_OUTER_SRC_FLAGS|=0x10;
					}
					BLT_OUTER_MODE=0x20;
					if (BLT_INNER_CNT==0)
					{
						BLT_OUTER_MODE|=0x2;		//Clamp to 8 bit count
					}
					/*BLT_OUTER_CMD=0x60 | (BLT_OUTER_CMD&0x1F);
					BLT_OUTER_SRC_FLAGS=(BLT_OUTER_MODE&0x80)?0x40:0x00;
					BLT_OUTER_DST_FLAGS=BLT_OUTER_MODE&0x40;
					BLT_OUTER_MODE=0x20;
					if (BLT_INNER_CNT==0)
					{
						BLT_OUTER_MODE|=0x2;		//Clamp to 8 bit count
					}*/
					DoBlit();
				}
				else
				{
					BLT_OUTER_CMD=0x80 | (BLT_OUTER_CMD&0x17);
					BLT_OUTER_SRC_FLAGS=(BLT_OUTER_MODE&0x80)?0x40:0x00;
					BLT_OUTER_DST_FLAGS=BLT_OUTER_MODE&0x40;
					BLT_OUTER_MODE=0xA4;
					if (BLT_INNER_CNT==0)
					{
						BLT_OUTER_MODE|=0x2;		//Clamp to 8 bit count
					}
					DoBlit();
				}
				break;
			case 0x40:
				if ((BLT_OUTER_MODE&0xFB)!=0)			// 0x04 = font mode
				{
					CONSOLE_OUTPUT("wrn - BLTMODE unknown bits (0x40 %d)\n",BLT_OUTER_MODE&0xFB);
				}
				// Appears to be byte fill  -- assume cnts are 8bit not 9
				BLT_OUTER_CMD=0x00 | (BLT_OUTER_CMD&0x1F);
				BLT_OUTER_MODE=0x20;
				BLT_OUTER_SRC_FLAGS=0;
				BLT_OUTER_DST_FLAGS=0;
				if (BLT_INNER_CNT==0)
				{
					BLT_OUTER_MODE|=0x2;		//Clamp to 8 bit count
				}
				DoBlit();
				break;
			case 0x80:
			case 0xA0:
			case 0xC0:
				if ((BLT_OUTER_MODE&0x1F)!=0)
				{
					CONSOLE_OUTPUT("wrn - BLTMODE unknown bits");
				}
				// Line Drawing
				BLT_OUTER_SRC_FLAGS=(BLT_OUTER_MODE&0x80)?0x40:0x00;
				BLT_OUTER_DST_FLAGS=BLT_OUTER_MODE&0x40;
				BLT_OUTER_CMD=0x01 | (BLT_OUTER_CMD&0x1F);
				BLT_OUTER_MODE=0x28 | (BLT_OUTER_MODE&0x20?0x10:0x00);
				if (BLT_INNER_CNT==0)
				{
					BLT_OUTER_MODE|=0x2;		//Clamp to 8 bit count
				}
				DoBlit();
				break;

		}

		BLT_OUTER_CMD=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		}
		while (BLT_OUTER_CMD&1);

//		exit(1);

	}
#endif
}


/*

 Data Path

 	SRCDATA_LSB----->|             |
	DSTDATA--------->|  COMPARATOR |-----> INHIBIT
	PATDATA--------->|             |

	SRCDATA_LSB----->|       |
	PATDATA--------->|  MUX  |------>|     |
	DSTDATA------------------------->| LFU |-----> DATAOUT_LSB
	SRCDATA_MSB----------------------------------> DATAOUT_MSB
	

  LFU  - 	LOG0				LOG1				LOG2				LOG3
		(NOT SRC) AND (NOT DST)		(NOT SRC) AND DST		SRC AND (NOT DST)		SRC AND DST


*/

int DoDataPath()
{
	int inhibit=0;

	//COMPARATOR
								// TODO CMPPLN (multi plane screen) - current comparator is working in pixel mode so CMPGT is also ignored


	if (BLT_OUTER_CMD&0xE0)				// source or destination read -- might want to check 0xA0 since docs state : includes a source read, or a source read and a destination read
	{
		BLTDDBG("COMPARATOR MAYBE ACTIVE\n");

		uint8_t CMP_MASK=0xFF;
		if ((BLT_OUTER_MODE&0x20)==0)				// 4 bit mode - use destination address to figure out nibble
		{
			if (ADDRESSGENERATOR_DSTADDRESS&1)			// ODD address LSNibble
			{
				CMP_MASK=0x0F;
			}
			else
			{
				CMP_MASK=0xF0;
			}
		}
		BLTDDBG("COMPARATOR MASK %02X\n",CMP_MASK);
		// COMPARATOR might be active

		uint8_t CMPARATOR_A=DATAPATH_PATDATA;
		uint8_t CMPARATOR_B=DATAPATH_PATDATA;
		if (BLT_OUTER_SRC_FLAGS&0x10)
			CMPARATOR_A=DATAPATH_SRCDATA&0xFF;		// TODO Might need nibble swap operation if SRC and DST nibbles are misaligned
		if (BLT_OUTER_DST_FLAGS&0x10)
			CMPARATOR_B=DATAPATH_DSTDATA;

		CMPARATOR_A&=CMP_MASK;
		CMPARATOR_B&=CMP_MASK;

		if (BLT_OUTER_MODE&0x04)				// CMPBIT
		{
			if (((1<<(8-BLT_INNER_CUR_CNT)) & DATAPATH_SRCDATA)==0)
			{
				inhibit=1;
			}
		}
		else
		{
			BLTDDBG("COMPARATOR A %02X  B %02X\n",CMPARATOR_A,CMPARATOR_B);

			if (BLT_OUTER_CPLG&0x01)			// CMPEQ
			{
				if (CMPARATOR_A == CMPARATOR_B)
				{
					BLTDDBG("INHIBIT A %02X == B %02X\n",CMPARATOR_A,CMPARATOR_B);
					inhibit=1;
				}
			}

			if (BLT_OUTER_CPLG&0x02)
			{
				if (CMPARATOR_A != CMPARATOR_B)
				{
					BLTDDBG("INHIBIT A %02X != B %02X\n",CMPARATOR_A,CMPARATOR_B);
					inhibit=1;
				}
			}
		}
	}
	
	//LFU
	uint8_t SRC=DATAPATH_SRCDATA&0xFF;
	uint8_t DST=DATAPATH_DSTDATA;
	uint8_t RES=0;

	if (BLT_OUTER_CMD&0x80)					//PATSEL
	{
		SRC=DATAPATH_PATDATA;
	}
	BLTDDBG("SRC %02X DST %02X\n",SRC,DST);

	if (BLT_OUTER_CPLG&0x80)
	{
		RES|=SRC & DST;
		BLTDDBG("SRC&DST RES %02X\n",RES);
	}
	if (BLT_OUTER_CPLG&0x40)
	{
		RES|=SRC & (~DST);
		BLTDDBG("SRC&~DST RES %02X\n",RES);
	}
	if (BLT_OUTER_CPLG&0x20)
	{
		RES|=(~SRC) & DST;
		BLTDDBG("~SRC&DST RES %02X\n",RES);
	}
	if (BLT_OUTER_CPLG&0x10)
	{
		RES|=(~SRC) & (~DST);
		BLTDDBG("~SRC&~DST RES %02X\n",RES);
	}

	DATAPATH_DATAOUT=DATAPATH_SRCDATA&0xFF00;
	DATAPATH_DATAOUT|=RES;
		
	BLTDDBG("RESULT %04X\n",DATAPATH_DATAOUT);

	return inhibit;
}

void AddressGeneratorSourceStep(int32_t step)
{
	if (BLT_OUTER_SRC_FLAGS&0x40)				// SSIGN
		step*=-1;
	BLTDDBG("SRCADDR %06X (%d)\n",ADDRESSGENERATOR_SRCADDRESS,step);
	if (BLT_OUTER_SRC_FLAGS&0x20)						// SWRAP
	{
		uint32_t tmp = ADDRESSGENERATOR_SRCADDRESS&0xFFFE0000;		// 64K WRAP
		ADDRESSGENERATOR_SRCADDRESS+=step;
		ADDRESSGENERATOR_SRCADDRESS&=0x1FFFF;
		ADDRESSGENERATOR_SRCADDRESS|=tmp;
	}
	else
	{
		ADDRESSGENERATOR_SRCADDRESS+=step;
	}
	BLTDDBG("SRCADDR %06X\n",ADDRESSGENERATOR_SRCADDRESS);
}

void AddressGeneratorDestinationStep(int32_t step)
{
	if (BLT_OUTER_DST_FLAGS&0x40)				// DSIGN
		step*=-1;
	BLTDDBG("DSTADDR %06X (%d)\n",ADDRESSGENERATOR_DSTADDRESS,step);
	if (BLT_OUTER_DST_FLAGS&0x20)						// DWRAP
	{
		uint32_t tmp = ADDRESSGENERATOR_DSTADDRESS&0xFFFE0000;		// 64K WRAP
		ADDRESSGENERATOR_DSTADDRESS+=step;
		ADDRESSGENERATOR_DSTADDRESS&=0x1FFFF;
		ADDRESSGENERATOR_DSTADDRESS|=tmp;
	}
	else
	{
		ADDRESSGENERATOR_DSTADDRESS+=step;
	}

	BLTDDBG("DSTADDR %06X (X,Y) (%d,%d)\n",ADDRESSGENERATOR_DSTADDRESS,(ADDRESSGENERATOR_DSTADDRESS>>1)&0xFF,(ADDRESSGENERATOR_DSTADDRESS&0x1FE00)>>9);
}

void AddressGeneratorDestinationLineStep(int32_t step)
{
	BLTDDBG("DSTADDR %06X (%d)\n",ADDRESSGENERATOR_DSTADDRESS,step);
	if (BLT_OUTER_DST_FLAGS&0x20)						// DWRAP
	{
		uint32_t tmp = ADDRESSGENERATOR_DSTADDRESS&0xFFFE0000;		// 64K WRAP
		ADDRESSGENERATOR_DSTADDRESS+=step;
		ADDRESSGENERATOR_DSTADDRESS&=0x1FFFF;
		ADDRESSGENERATOR_DSTADDRESS|=tmp;
	}
	else
	{
		ADDRESSGENERATOR_DSTADDRESS+=step;
	}

	BLTDDBG("DSTADDR %06X\n",ADDRESSGENERATOR_DSTADDRESS);
}

void AddressGeneratorSourceRead()
{
	int32_t increment=0;
	switch (BLT_OUTER_MODE&0x60)		//RES0 RES1
	{
		case 0x00:				//4 bits  (256 pixel)
		case 0x40:				//4 bits  (512 pixel)
			if (curSystem==ESS_P89)
			{
				// TODO-- why ??
				if (ADDRESSGENERATOR_SRCADDRESS&1)			// ODD address LSNibble
				{
					DATAPATH_SRCDATA&=0xFFF0;
					DATAPATH_SRCDATA|=(GetByte(ADDRESSGENERATOR_SRCADDRESS>>1)>>4)&0xF;
				}
				else
				{
					DATAPATH_SRCDATA&=0xFFF0;
					DATAPATH_SRCDATA|=(GetByte(ADDRESSGENERATOR_SRCADDRESS>>1)>>0)&0xF;
				}
			}
			else
			{
				if (ADDRESSGENERATOR_SRCADDRESS&1)			// ODD address LSNibble
				{
					DATAPATH_SRCDATA&=0xFFF0;
					DATAPATH_SRCDATA|=GetByte(ADDRESSGENERATOR_SRCADDRESS>>1)&0xF;
				}
				else
				{
					DATAPATH_SRCDATA&=0xFF0F;
					DATAPATH_SRCDATA|=GetByte(ADDRESSGENERATOR_SRCADDRESS>>1)&0xF0;
				}
			}
			increment=1;
			break;
		case 0x20:				//8 bits  (256 pixel)
			DATAPATH_SRCDATA&=0xFF00;
			DATAPATH_SRCDATA|=GetByte(ADDRESSGENERATOR_SRCADDRESS>>1);
			increment=2;
			break;
		case 0x60:				//16 bits (N/A)
			DATAPATH_SRCDATA=(GetByte(ADDRESSGENERATOR_SRCADDRESS>>1)<<8)|GetByte((ADDRESSGENERATOR_SRCADDRESS>>1)+1);
			increment=4;
			break;
	}
	BLTDDBG("SRCREAD %04X  (%d)\n",DATAPATH_SRCDATA,increment);

	AddressGeneratorSourceStep(increment);
}


void AddressGeneratorDestinationRead()
{
	switch (BLT_OUTER_MODE&0x60)		//RES0 RES1
	{
		case 0x00:				//4 bits  (256 pixel)
		case 0x40:				//4 bits  (512 pixel)
			if (ADDRESSGENERATOR_DSTADDRESS&1)			// ODD address LSNibble		-- this could well be wrong!
			{
				DATAPATH_DSTDATA&=0xF0;
				DATAPATH_DSTDATA|=GetByte(ADDRESSGENERATOR_DSTADDRESS>>1)&0xF;
			}
			else
			{
				DATAPATH_DSTDATA&=0x0F;
				DATAPATH_DSTDATA|=GetByte(ADDRESSGENERATOR_DSTADDRESS>>1)&0xF0;
			}
			break;
		case 0x20:				//8 bits  (256 pixel)
			DATAPATH_DSTDATA=GetByte(ADDRESSGENERATOR_DSTADDRESS>>1);
			break;
		case 0x60:				//16 bits (N/A)  - Not sure if this should read both (since destination is only 8 bits!!!
			DATAPATH_DSTDATA=(GetByte(ADDRESSGENERATOR_DSTADDRESS>>1)<<8)|GetByte((ADDRESSGENERATOR_DSTADDRESS>>1)+1);
			break;
	}
	BLTDDBG("DSTREAD %02X\n",DATAPATH_DSTDATA);
}

void AddressGeneratorDestinationWrite()
{
	switch (BLT_OUTER_MODE&0x60)		//RES0 RES1
	{
		case 0x00:				//4 bits  (256 pixel)
		case 0x40:				//4 bits  (512 pixel)
			SetByte(ADDRESSGENERATOR_DSTADDRESS>>1,DATAPATH_DATAOUT);
			break;
		case 0x20:				//8 bits  (256 pixel)
			SetByte(ADDRESSGENERATOR_DSTADDRESS>>1,DATAPATH_DATAOUT);
			break;
		case 0x60:				//16 bits (N/A)  - Not sure if this should read both (since destination is only 8 bits!!!
			SetByte(ADDRESSGENERATOR_DSTADDRESS>>1,DATAPATH_DATAOUT>>8);
			SetByte((ADDRESSGENERATOR_DSTADDRESS>>1)+1,DATAPATH_DATAOUT);
			break;
	}
}

void AddressGeneratorDestinationUpdate()
{
	int32_t increment=0;
	switch (BLT_OUTER_MODE&0x60)		//RES0 RES1
	{
		case 0x00:				//4 bits  (256 pixel)
		case 0x40:				//4 bits  (512 pixel)
			increment=1;
			break;
		case 0x20:				//8 bits  (256 pixel)
			increment=2;
			break;
		case 0x60:				//16 bits (N/A)  - Not sure if this should read both (since destination is only 8 bits!!!
			increment=4;
			break;
	}
	AddressGeneratorDestinationStep(increment);
}

int DoBlitInner()
{
	BLTDDBG("InnerCnt : %03X\n",BLT_INNER_CUR_CNT);
	do
	{
		BLTDDBG("-----\nINNER %d\n-----\n",BLT_INNER_CUR_CNT);
		if (BLT_OUTER_CMD&0x20)
		{
			AddressGeneratorSourceRead();
		}

		if (BLT_OUTER_CMD&0x40)
		{
			AddressGeneratorDestinationRead();
		}

		if (DoDataPath())			// If it returns true the write is inhibited
		{
			if (BLT_OUTER_CMD&0x02)
			{
				return 1;
			}
		}
		else
		{
			AddressGeneratorDestinationWrite();
		}

		AddressGeneratorDestinationUpdate();

		BLT_INNER_CUR_CNT--;
		BLT_INNER_CUR_CNT&=0x1FF;

	}while (BLT_INNER_CUR_CNT);

	return 0;
}

void DoBlitOuterLine()						// NB: this needs some work - it will be wrong in 16 bit modes and maybe wrong in 4 bit modes too
{
	uint8_t outerCnt = BLT_OUTER_CNT;			// This should be 1!

	uint8_t delta1Orig = (BLT_OUTER_SRC&0x00FF00)>>8;
	uint8_t delta1Work = (BLT_OUTER_SRC&0x0000FF);
	uint8_t delta2 = BLT_INNER_STEP;
	uint8_t length = BLT_INNER_CNT;
	uint16_t step = (BLT_INNER_STEP<<1)|(BLT_OUTER_MODE&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)

	int32_t deltaXStep=2;
	int32_t deltaYStep=512;
	
	if (BLT_OUTER_SRC_FLAGS&0x40)
		deltaXStep*=-1;
	if (BLT_OUTER_DST_FLAGS&0x40)
		deltaYStep*=-1;

	//Not clear if reloaded for between blocks (but for now assume it is)
	DATAPATH_SRCDATA=BLT_INNER_PAT|(BLT_INNER_PAT<<8);
	DATAPATH_DSTDATA=BLT_INNER_PAT;
	DATAPATH_PATDATA=BLT_INNER_PAT;

	while (1)
	{
		BLTDDBG("-----\nOUTER LINE %d\n-----\n",outerCnt);
		if ((BLT_OUTER_CMD&0xA0)==0x80)			// SRCENF enabled (ignored if SRCEN also enabled)
		{
			AddressGeneratorSourceRead();				// Should never happen because source address is not really a source address in line mode - but leave the read in case someone used it for effect
		}

		BLT_INNER_CUR_CNT=length;

		BLTDDBG("InnerCnt : %03X\n",BLT_INNER_CUR_CNT);
		do
		{
			BLTDDBG("-----\nINNER LINE %d\n-----\n",BLT_INNER_CUR_CNT);
			if (BLT_OUTER_CMD&0x20)
			{
				AddressGeneratorSourceRead();			// Again should never happen
			}

			if (BLT_OUTER_CMD&0x40)
			{
				AddressGeneratorDestinationRead();		// Could happen for 4 bit modes
			}

			if (DoDataPath())			// If it returns true the write is inhibited
			{
				// TODO - check for collision stop
				if (BLT_OUTER_CMD&0x02)
				{
					CONSOLE_OUTPUT("LineMode - ColStop Inhibit! - TODO");
				}
			}
			else
			{
				AddressGeneratorDestinationWrite();
			}

			// Now update Destination and source ptrs in line mode style .. not sure if line draw is pre/post increment... try post

			if (BLT_OUTER_MODE&0x10)		// YFRAC - set means step always X, sometimes Y
			{
				AddressGeneratorDestinationLineStep(deltaXStep);
				if (delta1Work<delta2)
				{
					delta1Work-=delta2;
					delta1Work+=delta1Orig;
					AddressGeneratorDestinationLineStep(deltaYStep);
				}
				else
				{
					delta1Work-=delta2;
				}
			}
			else
			{					// clear means step always Y, sometimes X
				AddressGeneratorDestinationLineStep(deltaYStep);
				if (delta1Work<delta2)
				{
					delta1Work-=delta2;
					delta1Work+=delta1Orig;
					AddressGeneratorDestinationLineStep(deltaXStep);
				}
				else
				{
					delta1Work-=delta2;
				}
			}

			BLT_INNER_CUR_CNT--;

		}while (BLT_INNER_CUR_CNT&0x1FF);

		outerCnt--;
		if (outerCnt==0)
			break;

		if (BLT_OUTER_CMD&0x10)				//DSTUP
		{
			AddressGeneratorDestinationStep(step);			// Should never reach here in line mode!
		}

		if (BLT_OUTER_CMD&0x08)
		{
			AddressGeneratorSourceStep(step);
		}

		if (BLT_OUTER_CMD&0x04)
		{
			if (curSystem==ESS_P89)		// This might apply to MSU version too (and would make more sense)
			{
				BLT_INNER_CNT=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_OUTER_MODE=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_INNER_PAT=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_INNER_STEP=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;
			}
			else
			{
				BLT_INNER_CNT=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_INNER_STEP=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_INNER_PAT=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;
			}
#if ENABLE_DEBUG_BLITTER
			if (doShowBlits)
			{
				if (curSystem==ESS_P89)
				{
					CONSOLE_OUTPUT("Inner Count : %02X\n",BLT_INNER_CNT);
					CONSOLE_OUTPUT("Mode Control : %02X\n",BLT_OUTER_MODE);
					CONSOLE_OUTPUT("Pattern : %02X\n",BLT_INNER_PAT);
					CONSOLE_OUTPUT("Step : %02X\n",BLT_INNER_STEP);
				}
				else
				{
					CONSOLE_OUTPUT("Inner Count : %02X\n",BLT_INNER_CNT);
					CONSOLE_OUTPUT("Step : %02X\n",BLT_INNER_STEP);
					CONSOLE_OUTPUT("Pattern : %02X\n",BLT_INNER_PAT);
				}
			}
#endif

			step = (BLT_INNER_STEP<<1)|(BLT_OUTER_MODE&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
			length = BLT_INNER_CNT;

			//Not clear if reloaded for between blocks (but for now assume it is)
			DATAPATH_SRCDATA=BLT_INNER_PAT|(BLT_INNER_PAT<<8);
			DATAPATH_DSTDATA=BLT_INNER_PAT;
			DATAPATH_PATDATA=BLT_INNER_PAT;
		}

	}
}

void DoBlitOuter()
{
	uint8_t outerCnt = BLT_OUTER_CNT;
	uint16_t step;
	uint16_t innerCnt;

	if (curSystem==ESS_P89)
	{
		step = (BLT_INNER_STEP<<1)|((BLT_OUTER_MODE>>1)&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
		innerCnt = ((BLT_OUTER_MODE&0x1)<<8) | BLT_INNER_CNT;			//TODO PARRD will cause this (and BLT_INNER_PAT and BLT_INNER_STEP) to need to be re-read 
	}
	else
	{
		step = (BLT_INNER_STEP<<1)|(BLT_OUTER_MODE&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
		innerCnt = ((BLT_OUTER_MODE&0x2)<<7) | BLT_INNER_CNT;			//TODO PARRD will cause this (and BLT_INNER_PAT and BLT_INNER_STEP) to need to be re-read 

		switch (BLT_OUTER_MODE&0x60)
		{
			case 0x00:
			case 0x40:
				innerCnt<<=1;					// 4bit modes double cnt -- but 16 bit modes dont half it... very odd
				break;
			case 0x20:
			case 0x60:
				break;
		}
	}

	//Not clear if reloaded for between blocks (but for now assume it is)
	DATAPATH_SRCDATA=BLT_INNER_PAT|(BLT_INNER_PAT<<8);
	DATAPATH_DSTDATA=BLT_INNER_PAT;
	DATAPATH_PATDATA=BLT_INNER_PAT;

	BLTDDBG("OuterCnt : %02X\n",outerCnt);
	BLTDDBG("SRCDATA : %04X\n",DATAPATH_SRCDATA);
	BLTDDBG("DSTDATA : %02X\n",DATAPATH_DSTDATA);
	BLTDDBG("PATDATA : %02X\n",DATAPATH_PATDATA);
	while (1)
	{
		BLTDDBG("-----\nOUTER %d\n-----\n",outerCnt);
		if ((BLT_OUTER_CMD&0xA0)==0x80)			// SRCENF enabled (ignored if SRCEN also enabled)
		{
			AddressGeneratorSourceRead();
		}

		BLT_INNER_CUR_CNT=innerCnt;
		if (DoBlitInner()==1)
			return;

		outerCnt--;
		if (outerCnt==0)
			break;

		if (BLT_OUTER_CMD&0x10)				//DSTUP
		{
			AddressGeneratorDestinationStep(step);			// ?? Does step just apply, or should it be multiplied by pixel width?
		}

		if (BLT_OUTER_CMD&0x08)
		{
			AddressGeneratorSourceStep(step);
		}

		if (BLT_OUTER_CMD&0x04)
		{
			if (curSystem==ESS_P89)		// This might apply to MSU version too (and would make more sense)
			{
				BLT_INNER_CNT=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_OUTER_MODE=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_INNER_PAT=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_INNER_STEP=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;
			}
			else
			{
				BLT_INNER_CNT=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_INNER_STEP=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;

				BLT_INNER_PAT=GetByte(ASIC_BLTPC);
				ASIC_BLTPC++;
			}

#if ENABLE_DEBUG_BLITTER
			if (doShowBlits)
			{
				if (curSystem==ESS_P89)
				{
					CONSOLE_OUTPUT("Inner Count : %02X\n",BLT_INNER_CNT);
					CONSOLE_OUTPUT("Mode Control : %02X\n",BLT_OUTER_MODE);
					CONSOLE_OUTPUT("Pattern : %02X\n",BLT_INNER_PAT);
					CONSOLE_OUTPUT("Step : %02X\n",BLT_INNER_STEP);
				}
				else
				{
					CONSOLE_OUTPUT("Inner Count : %02X\n",BLT_INNER_CNT);
					CONSOLE_OUTPUT("Step : %02X\n",BLT_INNER_STEP);
					CONSOLE_OUTPUT("Pattern : %02X\n",BLT_INNER_PAT);
				}
			}
#endif


			if (curSystem==ESS_P89)
			{
				step = (BLT_INNER_STEP<<1)|((BLT_OUTER_MODE>>1)&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
				innerCnt = ((BLT_OUTER_MODE&0x1)<<8) | BLT_INNER_CNT;			//TODO PARRD will cause this (and BLT_INNER_PAT and BLT_INNER_STEP) to need to be re-read 
			}
			else
			{
				step = (BLT_INNER_STEP<<1)|(BLT_OUTER_MODE&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
				innerCnt = ((BLT_OUTER_MODE&0x2)<<7) | BLT_INNER_CNT;			//TODO PARRD will cause this (and BLT_INNER_PAT and BLT_INNER_STEP) to need to be re-read 

				switch (BLT_OUTER_MODE&0x60)
				{
					case 0x00:
					case 0x40:
						innerCnt<<=1;					// 4bit modes double cnt -- but 16 bit modes dont half it... very odd -- Investigate Camels
						break;
					case 0x20:
					case 0x60:
						break;
				}
			}

			//Not clear if reloaded for between blocks (but for now assume it is)
			DATAPATH_SRCDATA=BLT_INNER_PAT|(BLT_INNER_PAT<<8);
			DATAPATH_DSTDATA=BLT_INNER_PAT;
			DATAPATH_PATDATA=BLT_INNER_PAT;
		}

	}
}

void DoBlit()
{
	ADDRESSGENERATOR_SRCADDRESS=(BLT_OUTER_SRC<<1) | ((BLT_OUTER_SRC_FLAGS&0x80)>>7);
	ADDRESSGENERATOR_DSTADDRESS=(BLT_OUTER_DST<<1) | ((BLT_OUTER_DST_FLAGS&0x80)>>7);
	BLTDDBG("SRCADDR : %06X\n",ADDRESSGENERATOR_SRCADDRESS);
	BLTDDBG("DSTADDR : %06X\n",ADDRESSGENERATOR_DSTADDRESS);

	if (BLT_OUTER_MODE&0x08)		// Line Draw
	{
		BLTDDBG("Line Parameters :\n");

		BLTDDBG("Delta One : %02X\n",(BLT_OUTER_SRC&0x00FF00)>>8);
		BLTDDBG("Delta One Working : %02X\n",(BLT_OUTER_SRC&0x0000FF));
		BLTDDBG("Delta Two : %02X\n",BLT_INNER_STEP);
		BLTDDBG("Gradient : %f",(float)((BLT_OUTER_SRC&0x00FF00)>>8) / (float)BLT_INNER_STEP);

		DoBlitOuterLine();
	}
	else
	{
		DoBlitOuter();
	}
}

void TickBlitter()
{
	if (BLT_OUTER_CMD&0x01)
	{
		DoBlit();
	}
}

void ASIC_WriteMSU(uint16_t port,uint8_t byte,int warnIgnore)
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
		case 0x0040:
			ASIC_BLTPC&=0xFFF00;
			ASIC_BLTPC|=byte;
			break;
		case 0x0041:
			ASIC_BLTPC&=0xF00FF;
			ASIC_BLTPC|=byte<<8;
			break;
		case 0x0042:
			ASIC_BLTPC&=0x0FFFF;
			ASIC_BLTPC|=(byte&0xF)<<16;
			break;
		case 0x0043:
			ASIC_BLTCMD=byte;
			break;
		case 0x0044:
			ASIC_BLTCON=byte;
			break;
		default:
#if ENABLE_DEBUG
			if (warnIgnore)
			{
				CONSOLE_OUTPUT("ASIC WRITE IGNORE %04X<-%02X - TODO?\n",port,byte);
			}
#endif
			break;
	}
}

void ASIC_WriteP89(uint16_t port,uint8_t byte,int warnIgnore)
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
		case 0x0005:
			ASIC_STARTH=byte;
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
			if (byte!=0)
				CONSOLE_OUTPUT("Warning PMASK!=0 - likely to be an emulation mismatch\n");
			break;
		case 0x0020:
			ASIC_INDEX=byte;
			if (byte!=0)
				CONSOLE_OUTPUT("Warning INDEX!=0 - likely to be an emulation mismatch\n");
			break;
		case 0x0022:
			ASIC_ENDL=byte;
			break;
		case 0x0023:
			ASIC_ENDH=byte;
			break;
		case 0x0026:
			ASIC_MEM=byte;
			if (byte!=0)
				CONSOLE_OUTPUT("Warning MEM!=0 - (mem banking not implemented)\n");
			break;
		case 0x002A:
			ASIC_DIAG=byte;
			if (byte!=0)
				CONSOLE_OUTPUT("Warning DIAG!=0 - (Diagnostics not implemented)\n");
			break;
		case 0x002C:
			ASIC_DIS=byte;
			if (byte&0xFE)
				CONSOLE_OUTPUT("Warning unhandled DIS bits set (%02X)- (Other intterupts not implemented)\n",byte);
			break;
		case 0x0040:
			ASIC_BLTPC&=0xFFF00;
			ASIC_BLTPC|=byte;
			break;
		case 0x0041:
			ASIC_BLTPC&=0xF00FF;
			ASIC_BLTPC|=byte<<8;
			break;
		case 0x0042:
			ASIC_BLTENH=byte&0xE0;
			ASIC_BLTPC&=0x0FFFF;
			ASIC_BLTPC|=(byte&0xF)<<16;		// note ENbits in upper nibble - not used yet!
			break;
		case 0x0043:
			ASIC_BLTCMD=byte;
			break;
		case 0x0044:
			ASIC_BLTCON=byte;
			if ((byte&2)!=0)
				CONSOLE_OUTPUT("Warning BLTCON!=0 - (Blitter control not implemented)  (%02X)\n",byte);
			break;
		case 0x0048:
#if ENABLE_DEBUG
			CONSOLE_OUTPUT("Floppy Read Control : %02X\n",byte);
#endif
			ASIC_FRC&=0xFF00;
			ASIC_FRC|=byte;
			break;
		case 0x0049:
			ASIC_FRC&=0x00FF;
			ASIC_FRC|=byte<<8;
			if (ASIC_FRC&1)
			{
				EDDY_State=0x6000;
#if ENABLE_DEBUG
				CONSOLE_OUTPUT("Begin Floppy DMA @%06X\n",(ASIC_FRC&0xFFE0)<<4);
#endif
			}
			break;
		case 0x0080:
			// SIDE | WRITE | STEP | DIR | MOTOR | DS1 | DS0 | 0
			if ( ((ASIC_FDC&0x20)==0x20) && ((byte&0x20)==0) )
			{	
				// Step request
#if ENABLE_DEBUG
				CONSOLE_OUTPUT("Stepping Head : %d\n",(byte&0x10)?-1:1);
#endif
				EDDY_Track+= (byte&0x10)?-1 : 1;
				if (EDDY_Track<0)
				{
					EDDY_Track=0;
				}
				if (EDDY_Track>79)
				{
					EDDY_Track=79;
				}
			}
			EDDY_Side=((byte&0x80)>>7)?0:1;
			ASIC_FDC=byte;
#if ENABLE_DEBUG
			CONSOLE_OUTPUT("Floppy Drive Control : Track %d : Side %d : %02X\n",EDDY_Track,EDDY_Side,byte);
#endif
			break;
		default:
#if ENABLE_DEBUG
			if (warnIgnore)
			{
				CONSOLE_OUTPUT("ASIC WRITE IGNORE %04X<-%02X - TODO?\n",port,byte);
			}
#endif
			break;
	}
}

void TERMINAL_OUTPUT(uint8_t byte);
uint8_t terminalWrote=0;

void ASIC_WriteP88(uint16_t port,uint8_t byte,int warnIgnore)
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
		case 0x0002:
			ASIC_STARTL=byte;
			break;
		case 0x0003:
			ASIC_STARTH=byte;
			break;
		case 0x0008:
			ASIC_SCROLL&=0x00FFFF00;
			ASIC_SCROLL|=byte;
			break;
		case 0x0009:
			ASIC_SCROLL&=0x00FF00FF;
			ASIC_SCROLL|=(byte<<8);
			break;
		case 0x000A:
			ASIC_SCROLL&=0x0000FFFF;
			ASIC_SCROLL|=(byte<<16);
			break;
		case 0x000B:
			// Clear video interrupt for now
			VideoInterruptLatch=0;
			break;
		case 0x000C:
			ASIC_MODE=byte;
			if (byte&0x01)
				CONSOLE_OUTPUT("256 Colour Screen Mode\n");
			else
				CONSOLE_OUTPUT("16 Colour Screen Mode\n");
			if (byte&0xFE)
				CONSOLE_OUTPUT("Warning unhandled MODE bits set - likely to be an emulation mismatch\n");
			break;
		case 0x000D:
			ASIC_BORD&=0xFF00;
			ASIC_BORD|=byte;
			break;
		case 0x000E:
			ASIC_BORD&=0x00FF;
			ASIC_BORD|=(byte<<8);
			break;
		case 0x000F:
			ASIC_PMASK=byte;
			if (byte!=0)
				CONSOLE_OUTPUT("Warning PMASK!=0 - likely to be an emulation mismatch\n");
			break;
		case 0x0010:
			ASIC_INDEX=byte;
			if (byte!=0)
				CONSOLE_OUTPUT("Warning INDEX!=0 - likely to be an emulation mismatch\n");
			break;
		case 0x0011:
			ASIC_ENDL=byte;
			break;
		case 0x0012:
			ASIC_ENDH=byte;
			break;
		case 0x0013:
			ASIC_MEM=byte;
			if (byte!=0)
				CONSOLE_OUTPUT("Warning MEM!=0 - (mem banking not implemented)\n");
			break;
		case 0x0015:
			ASIC_DIAG=byte;
			if (byte!=0)
				CONSOLE_OUTPUT("Warning DIAG!=0 - (Diagnostics not implemented)\n");
			break;
		case 0x0016:
			ASIC_DIS=byte;
			if (byte&0xFE)
				CONSOLE_OUTPUT("Warning unhandled DIS bits set (%02X)- (Other intterupts not implemented)\n",byte);
			break;
		case 0x0030:
			ASIC_BLTPC&=0xFFF00;
			ASIC_BLTPC|=byte;
			break;
		case 0x0031:
			ASIC_BLTPC&=0xF00FF;
			ASIC_BLTPC|=byte<<8;
			break;
		case 0x0032:
			ASIC_BLTPC&=0x0FFFF;
			ASIC_BLTPC|=(byte&0xF)<<16;
			break;
		case 0x0033:
			ASIC_BLTCMD=byte;
			break;
		case 0x0034:
			ASIC_BLTCON=byte;
			if (byte!=0)
				CONSOLE_OUTPUT("Warning BLTCON!=0 - (Blitter control not implemented)\n");
			break;
		case 0x0071:
			// Serial Port Control Register (Devkit)
			CONSOLE_OUTPUT("Serial Port Control : %02X\n",byte);
			break;
		case 0x0073:
			// Serial Port Data
			TERMINAL_OUTPUT(byte);
			terminalWrote=byte;
			break;
		default:
#if ENABLE_DEBUG
			if (warnIgnore)
			{
				CONSOLE_OUTPUT("ASIC WRITE IGNORE %04X<-%02X - TODO?\n",port,byte);
			}
#endif
			break;
	}
}
void FL1DSP_POKE(uint16_t,uint16_t);

extern int useRemoteDebugger;

void ASIC_WriteFL1(uint16_t port,uint8_t byte,int warnIgnore)
{
	switch (port)
	{
		case 0x0000:
			ASIC_BANK0=byte*16384;
#if ENABLE_DEBUG
			if (doShowPortStuff)
			{
				CONSOLE_OUTPUT("BANK0 Set to : %05X\n",ASIC_BANK0);
			}
#endif
			break;
		case 0x0001:
			ASIC_BANK1=byte*16384;
#if ENABLE_DEBUG
			if (doShowPortStuff)
			{
				CONSOLE_OUTPUT("BANK1 Set to : %05X\n",ASIC_BANK1);
			}
#endif
			break;
		case 0x0002:
			ASIC_BANK2=byte*16384;
#if ENABLE_DEBUG
			if (doShowPortStuff)
			{
				CONSOLE_OUTPUT("BANK2 Set to : %05X\n",ASIC_BANK2);
			}
#endif
			break;
		case 0x0003:
			ASIC_BANK3=byte*16384;
#if ENABLE_DEBUG
			if (doShowPortStuff)
			{
				CONSOLE_OUTPUT("BANK3 Set to : %05X\n",ASIC_BANK3);
			}
#endif
			break;
		case 0x0007:			// INTREG
			ASIC_KINT&=0xFF00;
			ASIC_KINT|=byte;
			break;
		case 0x0008:			// CMD1 - bit 2 (msb of line interrupt), bit 6 (which screen is visible)
			ASIC_KINT&=0xFEFF;
			ASIC_KINT|=(byte&0x04)<<6;
			if (byte&0xBB)
			{
				CONSOLE_OUTPUT("Unknown CMD1 bits set : %02X\n",byte&0xBB);
			}
#if ENABLE_DEBUG
			if (doShowPortStuff)
			{
				CONSOLE_OUTPUT("Interrupt Line set : %03X\n",ASIC_KINT&0x1FF);
			}
#endif

			ASIC_SCROLL&=0x0000FFFF;
			if (byte&0x40)
			{
				ASIC_SCROLL|=0x00030000;
#if ENABLE_DEBUG
				if (doShowPortStuff)
				{
					CONSOLE_OUTPUT("Visible screen is at Bank 3\n");
				}
#endif
			}
			else
			{
				ASIC_SCROLL|=0x00020000;
#if ENABLE_DEBUG
				if (doShowPortStuff)
				{
					CONSOLE_OUTPUT("Visible screen is at Bank 2\n");
				}
#endif
			}
			break;
		case 0x0009:			// CMD2 - bit 0 (mode 256 or 512 pixel (256 or 16 colour)
			ASIC_MODE&=0x9C;
			ASIC_MODE|=((byte)&0x01)+1;
			ASIC_MODE|=((byte&0x08)<<2);
			ASIC_MODE|=((byte&0x80)>>1);	
			if (byte&0x76)
			{
				CONSOLE_OUTPUT("Unknown CMD2 bits set : %02X\n",byte&0x76);
			}
#if ENABLE_DEBUG
			if (doShowPortStuff)
			{
				CONSOLE_OUTPUT("ASIC MODE : %02X\nCMD2 bit 0 -512 pixel mode : %02X\nCMD2 Set to : %02X\n",ASIC_MODE,byte&1,byte);
			}
#endif
			break;
		case 0x000A:
			ASIC_BORD=byte;		// BORD
			break;
		case 0x000B:			// SCRLH
			ASIC_SCROLL&=0xFFFFFF00;
			ASIC_SCROLL|=byte;
			break;
		case 0x000C:			// SCRLV
			ASIC_SCROLL&=0xFFFF00FF;
			ASIC_SCROLL|=byte<<8;
			break;
		case 0x000D:
			ASIC_COLHOLD=byte;	
			break;
		case 0x0010:
			ASIC_INTRCNT=(~ASIC_INTRCNT)&1;
			ASIC_INTRD>>=8;
			ASIC_INTRD|=byte<<8;
			if (ASIC_INTRCNT==0)
			{
#if ENABLE_DEBUG
				if (doShowPortStuff)
				{
					CONSOLE_OUTPUT("Load DSP Data %04X <- %04X\n",ASIC_INTRA,ASIC_INTRD);
				}
#endif
				FL1DSP_POKE(ASIC_INTRA,ASIC_INTRD);
			}
			break;
		case 0x0011:
			ASIC_INTRCNT=(~ASIC_INTRCNT)&1;
			ASIC_INTRD>>=8;
			ASIC_INTRD|=byte<<8;
			if (ASIC_INTRCNT==0)
			{
#if ENABLE_DEBUG
				if (doShowPortStuff)
				{
					CONSOLE_OUTPUT("Load DSP Data %04X <- %04X\n",ASIC_INTRA,ASIC_INTRD);
				}
#endif
				FL1DSP_POKE(ASIC_INTRA,ASIC_INTRD);
				ASIC_INTRA++;		// Post Increment Intrude
			}
			break;
		case 0x0012:
			ASIC_INTRA>>=8;
			ASIC_INTRA|=byte<<8;
			break;
		case 0x0013:
			ASIC_PROGCNT=(~ASIC_PROGCNT)&1;
			ASIC_PROGWRD>>=8;
			ASIC_PROGWRD|=byte<<8;
			if (ASIC_PROGCNT==0)
			{
#if ENABLE_DEBUG
				if (doShowPortStuff)
				{
					CONSOLE_OUTPUT("Load DSP Program %04X <- %04X\n",ASIC_PROGADDR,ASIC_PROGWRD);
				}
				DSP_TranslateInstructionFL1(ASIC_PROGADDR,ASIC_PROGWRD);
#endif
				FL1DSP_POKE(0x800+ASIC_PROGADDR,ASIC_PROGWRD);
			}
			break;
		case 0x0015:
			ASIC_PROGADDR>>=8;
			ASIC_PROGADDR|=byte<<8;
			if ((DSP_STATUS&1)==0)
			{
//				CONSOLE_OUTPUT("PERFORMING RESET : %04X\n",ASIC_PROGADDR);
				FL1DSP_RESET();
				FL1DSP_PC=ASIC_PROGADDR|0x800;
			}
			break;
		case 0x0018:
			FL1BLT_SetProgLow(byte);
			ASIC_BLTPC&=0xFFF00;
			ASIC_BLTPC|=byte;
			break;
		case 0x0019:
			FL1BLT_SetProgMiddle(byte);
			ASIC_BLTPC&=0xF00FF;
			ASIC_BLTPC|=byte<<8;
			break;
		case 0x001A:
			FL1BLT_SetProgHi(byte);
			ASIC_BLTPC&=0x0FFFF;
			ASIC_BLTPC|=(byte&0xF)<<16;
			break;
		case 0x0020:
			FL1BLT_SetCmd(byte);
			ASIC_BLTCMD=byte;
			if (useRemoteDebugger)
			{
				//pause=1;
			}
		case 0x0050:
			ASIC_PALAW=byte;
			ASIC_PALCNT=0;
			break;
		case 0x0051:
			ASIC_PALVAL<<=8;
			ASIC_PALVAL|=byte;
			ASIC_PALVAL&=0x00FFFFFF;
			ASIC_PALSTORE[ASIC_PALAW*3+ASIC_PALCNT]=byte;
			ASIC_PALCNT++;
			if (ASIC_PALCNT==3)
			{
				ASIC_PALCNT=0;
#if ENABLE_DEBUG
				if (doShowPortStuff)
				{
					CONSOLE_OUTPUT("New Palette Written : %08X\n",ASIC_PALVAL);
				}
#endif
				PALETTE[ASIC_PALAW*2+0]=((ASIC_PALVAL&0x3F)>>2)|((ASIC_PALVAL&0x3C00)>>6);
				PALETTE[ASIC_PALAW*2+1]=ASIC_PALVAL>>(16+2);
			}
			break;
		case 0x0052:
			ASIC_PALMASK=byte;
			if (byte!=255)
			{
				CONSOLE_OUTPUT("Unknown PALMASK value of : %02X\n",byte);
			}
			break;
		case 0x0053:
			ASIC_PALAR=byte;
			ASIC_PALCNT=0;
			break;
		default:
#if ENABLE_DEBUG
			if (warnIgnore)
			{
				CONSOLE_OUTPUT("ASIC WRITE IGNORE %04X<-%02X - TODO?\n",port,byte);
			}
#endif
			break;
	}
}

extern uint16_t joyPadState;
extern uint32_t joy89state;

uint8_t ASIC_ReadP89(uint16_t port,int warnIgnore)
{
	switch (port)
	{
		case 0x0000:				// HLPL		--- TODO.. only when DIAG bit 0 set!
			return hClock&0xFF;
		case 0x0001:				// HLPH
			return (hClock>>8)&0xFF;
		case 0x0002:				// VLPL
			return vClock&0xFF;
		case 0x0003:
			return (vClock>>8)&0xFF;
		case 0x0008:
			return (joy89state)&0xFF;
		case 0x0009:
			return ((joy89state)>>8)&0xFF;
		case 0x000C:
			// STAT - 0IJJJ9PN	- Index | Joystick16-18 | 9Mhz CPU mode | (Light) Pen input received | Ntsc mode
			return (EDDY_Index<<6)|(((joy89state)&0x70000)>>13);
		case 0x0040:
			// BLT DST ADDRESS 0-15
			return (ADDRESSGENERATOR_DSTADDRESS>>1)&0xFF;
		case 0x0041:
			// BLT DST ADDRESS 0-15
			return (ADDRESSGENERATOR_DSTADDRESS>>9)&0xFF;
		case 0x0042:
			// todo Istop CStop inner cnt
			return ((ADDRESSGENERATOR_DSTADDRESS>>17)&0x000F)|((ADDRESSGENERATOR_DSTADDRESS&1)<<4);
		case 0x0043:
			// todo Istop CStop inner cnt
			return 0;//((ADDRESSGENERATOR_DSTADDRESS>>17)&0x000F)|((ADDRESSGENERATOR_DSTADDRESS&1)<<4);
		case 0x0044:
			// BLT SRC ADDRESS 0-15
			return (ADDRESSGENERATOR_SRCADDRESS>>1)&0xFF;
		case 0x0045:
			// BLT SRC ADDRESS 0-15
			return (ADDRESSGENERATOR_SRCADDRESS>>9)&0xFF;
		case 0x0046:
			// todo outer cnt
			return ((ADDRESSGENERATOR_SRCADDRESS>>17)&0x000F)|((ADDRESSGENERATOR_SRCADDRESS&1)<<4);
		case 0x0047:
			// todo outer cnt
			return 0;//((ADDRESSGENERATOR_SRCADDRESS>>17)&0x000F)|((ADDRESSGENERATOR_SRCADDRESS&1)<<4);
		case 0x0048:
#if ENABLE_DEBUG
			CONSOLE_OUTPUT("Read from Floppy Read Status\n");
#endif
			return EDDY_State&0xFF;
		case 0x0049:
#if ENABLE_DEBUG
			CONSOLE_OUTPUT("Read from Floppy Read Status\n");
#endif
			return (EDDY_State>>8)&0xFF;
		case 0x0080:
#if ENABLE_DEBUG
			CONSOLE_OUTPUT("Read from Floppy Drive Status Status %d\n",(EDDY_Track==0)?2:3);
#endif
			return (EDDY_Track==0)?2:3;		// Status inverted for Track 0 
		default:
#if ENABLE_DEBUG
			if (warnIgnore)
			{
				CONSOLE_OUTPUT("ASIC READ IGNORE %04X - TODO?\n",port);
			}
#endif
			break;
	}
	return 0xAA;
}



uint8_t GetTermKey();
uint8_t HasTermKey();


uint8_t ASIC_ReadP88(uint16_t port,int warnIgnore)
{
	switch (port)
	{
		case 0x0000:				// HLPL
			return hClock&0xFF;
		case 0x0001:				// HLPH
			return (hClock>>8)&0xFF;
		case 0x0002:				// VLPL
			return vClock&0xFF;
		case 0x0003:
			return (vClock>>8)&0xFF;
		case 0x0071:
			// Serial Port Control Register (Devkit) (bit 0 - CTR , bit 2 - CTS)
			{
				uint8_t ret=0x04 + (HasTermKey()||terminalWrote?1:0);	// Fake always ready for send
				terminalWrote=0;
				return ret;
			}
			break;
		case 0x0073:
			// Serial Port Data
			{
				uint8_t t = GetTermKey();
				CONSOLE_OUTPUT("FROM TERMINAL : READ %d\n",t);
				return t;
			}
			break;
		default:
#if ENABLE_DEBUG
			if (warnIgnore)
			{
				CONSOLE_OUTPUT("ASIC READ IGNORE %04X - TODO?\n",port);
			}
#endif
			break;
	}
	return 0xAA;
}

uint8_t ASIC_ReadFL1(uint16_t port,int warnIgnore)
{
	switch (port)
	{
		case 0x0000:
			return ASIC_BANK0/16384;
		case 0x0001:
			return ASIC_BANK1/16384;
		case 0x0002:
			return ASIC_BANK2/16384;
		case 0x0003:
			return ASIC_BANK3/16384;
		case 0x0020:
			return FL1BLT_GetDstLo();
//			return (ADDRESSGENERATOR_DSTADDRESS>>1)&0xFF;
		case 0x0021:
			return FL1BLT_GetDstMi();
//			return (ADDRESSGENERATOR_DSTADDRESS>>9)&0xFF;
		case 0x0007:		// INTACK
			VideoInterruptLatch=0;
			return 0;
		case 0x0006:
			return VideoInterruptLatch<<4;
		case 0x0014:		// RUNST
			return 0;			// Temporary -known bottom 3 bits are intrude status
		case 0x0051:
			return ASIC_PALSTORE[ASIC_PALAR*3+ASIC_PALCNT++];
		default:
			if (warnIgnore)
			{
				CONSOLE_OUTPUT("ASIC READ IGNORE %04X - TODO?\n",port);
			}
			break;
	}
	return 0xAA;
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


uint8_t PeekByte(uint32_t addr);

uint32_t ConvPaletteMSU(uint16_t pal)
{
	return RGB565_RGB8(pal);
}

uint32_t ConvPaletteP88(uint16_t pal)
{
	return RGB444_RGB8(pal);
}

void DoScreenInterrupt()
{
	switch (curSystem)
	{
		case ESS_MSU:
		case ESS_P89:
		case ESS_P88:
			INTERRUPT(0x21);
			break;
		case ESS_FL1:
			Z80_INTERRUPT(0x00);		// use a nop, which should mean we don't smack into uninitialised interrupt... maybe
			break;
	}
}
extern uint8_t IsKeyAvailable();

void DoPeripheralInterrupt()
{
	switch (curSystem)
	{
		case ESS_MSU:
		case ESS_P88:
			break;
		case ESS_FL1:
			Z80_INTERRUPT(0x00);
			break;
	}
}

void TickAsic(int cycles,uint32_t(*conv)(uint16_t),int fl1)
{
	uint8_t palIndex;
	uint16_t palEntry;
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	uint32_t screenPtr = ASIC_SCROLL;
	static uint32_t lastCol;
	uint32_t curCol;
	uint32_t wrapOffset;
	uint16_t StartL = ((ASIC_STARTH&1)<<8)|ASIC_STARTL;
	uint16_t EndL = ((ASIC_ENDH&1)<<8)|ASIC_ENDL;
	outputTexture+=vClock*WIDTH + hClock;

	// Video addresses are expected to be aligned to 256/128 byte boundaries - this allows for wrap to occur for a given line

	while (cycles)
	{
		if (fl1)
		{
			TickFL1DSP();
		}
		else
		{
			TickDSP();
		}

		// This is a quick hack up of the screen functionality -- at present simply timing related to get interrupts to fire
		if (VideoInterruptLatch)
		{
			DoScreenInterrupt();		
		}

		// Quick and dirty video display no contention or bus cycles
		if (hClock>=120 && hClock<632 && vClock>StartL && vClock<=EndL)
		{
			switch (ASIC_MODE&0x43)
			{
				case 0:			// LoRes (1 nibble per pixel - 256 wide)
					wrapOffset=(screenPtr+((vClock-StartL)-1)*128)&0xFFFFFF80;
					wrapOffset|=(screenPtr+((hClock-120)/4))&0x7F;

					palIndex = PeekByte(wrapOffset);
					if (((hClock-120)/2)&1)
					{
						// MSB nibble
						palIndex>>=4;
					}
					palIndex&=0xF;
					palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];
					break;

				case 1:			// MediumRes (1 byte per pixel - 256 wide)
					wrapOffset=(screenPtr+((vClock-StartL)-1)*256)&0xFFFFFF00;
					wrapOffset|=(screenPtr+((hClock-120)/2))&0xFF;

					palIndex = PeekByte(wrapOffset);
					palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];

					break;

				case 2:			// HiRes (1 nibble per pixel - 512 wide)
					wrapOffset=(screenPtr+((vClock-StartL)-1)*256)&0xFFFFFF00;
					wrapOffset|=(screenPtr+((hClock-120)/2))&0xFF;

					palIndex = PeekByte(wrapOffset);
					if (((hClock-120))&1)
					{
						// MSB nibble
						palIndex>>=4;
					}
					palIndex&=0xF;
					palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];

					break;


				default:
				case 0x40:
				case 0x41:
					// Compute byte fetch  ((hclock-120)/2
					wrapOffset=(screenPtr+((vClock-StartL)-1)*256)&0xFFFFFF00;
					wrapOffset|=(screenPtr+((hClock-120)/2))&0xFF;

					palIndex=PeekByte(wrapOffset);			// We should now have a screen byte - if bit 7 is set, it should be treated as 2 nibbles - note upper nibble will have range 8-15 due to bit
					if (palIndex&0x80)
					{
						if (((hClock-120))&1)
						{
							// MSB nibble
							palIndex>>=4;
						}
						palIndex&=0x0F;
					}
					else
					{
						palIndex&=0x7F;
					}

					palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];

					break;
			}
			curCol=conv(palEntry);
			if ((ASIC_MODE&0x20) && (palIndex==ASIC_COLHOLD))
			{
				*outputTexture++ = lastCol;
			}
			else
			{
				*outputTexture++=curCol;
				lastCol=curCol;
			}
		}
		else
		{
			*outputTexture++=conv(ASIC_BORD);
		}

		hClock++;
		if (IsKeyAvailable())
		{
			DoPeripheralInterrupt();
		}
		if ((hClock==631) && (ASIC_KINT==vClock) && ((ASIC_DIS&0x1)==0))			//  Docs state interrupt fires at end of active display of KINT line
		{
			VideoInterruptLatch=1;
		}
		if (hClock==(WIDTH))
		{
			hClock=0;
			vClock++;
			if (vClock==(HEIGHT))
			{
				vClock=0;
			}
		}

		cycles--;
	}
}

void ShowOffScreen(uint32_t(*conv)(uint16_t),int fl1)
{
	uint8_t palIndex;
	uint16_t palEntry;
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	uint32_t screenPtr = ASIC_SCROLL^0x10000;
	static uint32_t lastCol;
	uint32_t curCol;
	uint32_t wrapOffset;
	uint16_t StartL = ((ASIC_STARTH&1)<<8)|ASIC_STARTL;
	uint16_t EndL = ((ASIC_ENDH&1)<<8)|ASIC_ENDL;
	uint32_t cycles=WIDTH*HEIGHT;
	uint32_t hClock=0;
	uint32_t vClock=0;
//	outputTexture+=vClock*WIDTH + hClock;

	// Video addresses are expected to be aligned to 256/128 byte boundaries - this allows for wrap to occur for a given line

	while (cycles)
	{
		// Quick and dirty video display no contention or bus cycles
		if (hClock>=120 && hClock<632 && vClock>StartL && vClock<=EndL)
		{
			switch (ASIC_MODE&0x43)
			{
				case 0:			// LoRes (1 nibble per pixel - 256 wide)
					wrapOffset=(screenPtr+((vClock-StartL)-1)*128)&0xFFFFFF80;
					wrapOffset|=(screenPtr+((hClock-120)/4))&0x7F;

					palIndex = PeekByte(wrapOffset);
					if (((hClock-120)/2)&1)
					{
						// MSB nibble
						palIndex>>=4;
					}
					palIndex&=0xF;
					palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];
					break;

				case 1:			// MediumRes (1 byte per pixel - 256 wide)
					wrapOffset=(screenPtr+((vClock-StartL)-1)*256)&0xFFFFFF00;
					wrapOffset|=(screenPtr+((hClock-120)/2))&0xFF;

					palIndex = PeekByte(wrapOffset);
					palEntry = palIndex;//(PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];

					break;

				case 2:			// HiRes (1 nibble per pixel - 512 wide)
					wrapOffset=(screenPtr+((vClock-StartL)-1)*256)&0xFFFFFF00;
					wrapOffset|=(screenPtr+((hClock-120)/2))&0xFF;

					palIndex = PeekByte(wrapOffset);
					if (((hClock-120))&1)
					{
						// MSB nibble
						palIndex>>=4;
					}
					palIndex&=0xF;
					palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];

					break;


				default:
				case 0x40:
				case 0x41:
					// Compute byte fetch  ((hclock-120)/2
					wrapOffset=(screenPtr+((vClock-StartL)-1)*256)&0xFFFFFF00;
					wrapOffset|=(screenPtr+((hClock-120)/2))&0xFF;

					palIndex=PeekByte(wrapOffset);			// We should now have a screen byte - if bit 7 is set, it should be treated as 2 nibbles - note upper nibble will have range 8-15 due to bit
					if (palIndex&0x80)
					{
						if (((hClock-120))&1)
						{
							// MSB nibble
							palIndex>>=4;
						}
						palIndex&=0x0F;
					}
					else
					{
						palIndex&=0x7F;
					}

					palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];

					break;
			}
			curCol=conv(palEntry);
			if ((ASIC_MODE&0x20) && (palIndex==ASIC_COLHOLD))
			{
				*outputTexture++ = lastCol;
			}
			else
			{
				*outputTexture++=curCol;
				lastCol=curCol;
			}
		}
		else
		{
			*outputTexture++=conv(ASIC_BORD);
		}

		hClock++;
		if (hClock==(WIDTH))
		{
			hClock=0;
			vClock++;
			if (vClock==(HEIGHT))
			{
				vClock=0;
			}
		}

		cycles--;
	}
}


void TickAsicMSU(int cycles)
{
	TickBlitterMSU();
	TickAsic(cycles,ConvPaletteMSU,0);
}

void PrintAt(unsigned char* buffer,unsigned int width,unsigned char r,unsigned char g,unsigned char b,unsigned int x,unsigned int y,const char *msg,...);

extern uint8_t DISK_IMAGE[5632*2*80];

void TickAsicP89(int cycles)
{
	TickBlitterP89();
	TickAsic(cycles,ConvPaletteP88,0);

	if ( EDDY_State!=0 )	// Floppy Read
	{
		// For now, read the whole sector and clear the status
		int a;

		for (a=0;a<5632;a++)
		{
			SetByte(((ASIC_FRC&0xFFE0)<<4)+a,DISK_IMAGE[EDDY_Track*5632*2 + EDDY_Side*5632 + a]);
		}

		ASIC_FRC&=0xFFFE;
		EDDY_State=0;
/*		if (EDDY_Track==25 && EDDY_Side==1)
			doDebug=1;*/
		
	}
	if ( (ASIC_FDC&0x08)==0 )	// Motor On
	{
		EDDY_BitPos+=cycles;
		EDDY_Index=0;
		if (EDDY_BitPos>=INDEX_PULSE_FREQ)
		{
			EDDY_BitPos=0;
		}
		if (EDDY_BitPos<INDEX_PULSE_WIDTH)
		{
			EDDY_Index=1;
		}
	}
}

void ShowEddyDebug()
{
	PrintAt(videoMemory[MAIN_WINDOW],windowWidth[MAIN_WINDOW],255,255,255,1,1,"Floppy Status : FDC (%02X) : Track %d : Side %d : BitPos %d : Index %d\n",ASIC_FDC,EDDY_Track,EDDY_Side,EDDY_BitPos,EDDY_Index);
}

void TickAsicP88(int cycles)
{
	TickBlitterP88();
	TickAsic(cycles,ConvPaletteP88,0);
}

void TickAsicFL1(int cycles)
{
	// There are 2 screens on FLARE 1 (they are hardwired unlike later versions) - 1 at 0x20000 and the other at 0x30000
	TickBlitterFL1();
	TickAsic(cycles,ConvPaletteP88,1);
}

void DebugDrawOffScreen()
{
	ShowOffScreen(ConvPaletteP88,1);
}

void ASIC_INIT()
{
	hClock=0;
	vClock=0;
	VideoInterruptLatch=0;

	ASIC_KINT=0x00FF;
	ASIC_STARTL=33;
	ASIC_STARTH=0;
	ASIC_SCROLL=0;
	ASIC_MODE=0;
	ASIC_BORD=0;
	ASIC_PMASK=0;
	ASIC_INDEX=0;
	ASIC_ENDL=33;
	ASIC_ENDH=1;
	ASIC_MEM=0;
	ASIC_DIAG=0;
	ASIC_DIS=0;
	ASIC_BLTCON=0;
	ASIC_BLTCMD=0;
	ASIC_BLTPC=0;				// 20 bit address
	ASIC_COLHOLD=0;					// Not changeable on later than Flare One revision

	ASIC_PROGCNT=0;
	ASIC_PROGWRD=0;
	ASIC_PROGADDR=0;
	ASIC_INTRA=0;
	ASIC_INTRD=0;
	ASIC_INTRCNT=0;

	ASIC_PALAW=0;
	ASIC_PALVAL=0;
	ASIC_PALCNT=0;
	ASIC_PALAR=0;
	ASIC_PALMASK=0xFF;

	ASIC_FDC=0xFF;

	ASIC_BANK0=0;				// Z80 banking registers  (stored in upper 16bits)
	ASIC_BANK1=0;
	ASIC_BANK2=0;
	ASIC_BANK3=0;

	BLT_OUTER_SRC_FLAGS=0;
	BLT_OUTER_DST_FLAGS=0;
	BLT_OUTER_CMD=0;
	BLT_OUTER_SRC=0;
	BLT_OUTER_DST=0;
	BLT_OUTER_MODE=0;
	BLT_OUTER_CPLG=0;
	BLT_OUTER_CNT=0;
	BLT_INNER_CNT=0;
	BLT_INNER_STEP=0;
	BLT_INNER_PAT=0;

	ADDRESSGENERATOR_SRCADDRESS=0;			// 21 bit  - LSB = nibble
	ADDRESSGENERATOR_DSTADDRESS=0;			// 21 bit  - LSB = nibble

	DATAPATH_SRCDATA=0;
	DATAPATH_DSTDATA=0;
	DATAPATH_PATDATA=0;
	DATAPATH_DATAOUT=0;

	BLT_INNER_CUR_CNT=0;
}
