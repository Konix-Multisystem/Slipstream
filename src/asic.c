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
#define BLTDDBG(...)		if (doShowBlits) { CONSOLE_OUTPUT(__VA_ARGS__); }
#else
#define ENABLE_DEBUG_BLITTER	0
#define BLTDDBG(...)		//if (doShowBlits) { CONSOLE_OUTPUT(__VA_ARGS__); }
#endif

extern int MAIN_WINDOW;

extern unsigned char PALETTE[256*2];
extern int doShowPortStuff;
extern int doDebug;

void MSU_INTERRUPT(uint8_t);
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
uint8_t		ASIC_BLTCON2=0;
uint8_t		ASIC_BLTCMD=0;
uint8_t		ASIC_BLTENH=0;
uint32_t	ASIC_BLTPC=0;				// 20 bit address
uint8_t		ASIC_COLHOLD=0;					// Not changeable on later than Flare One revision
uint8_t		ASIC_MAG = 0x55;			//FL1 - hires
uint8_t		ASIC_YEL = 0x66;			//FL1 - hires
uint8_t		ASIC_CMD1 = 0;				//FL1
uint8_t		ASIC_CMD2 = 0;				//FL1
uint8_t		ASIC_CHAIR = 0;

uint8_t		ASIC_FDC=0xFF;				// P89 Floppy Disk Controller
uint32_t	ASIC_FRC=0x00000000;			// P89 Floppy Disk Controller

uint16_t	ASIC_PROGWRD=0;		// FL1 Only
uint16_t	ASIC_PROGADDR=0;
uint16_t	ASIC_INTRA=0;
uint16_t	ASIC_INTRD=0;
uint8_t		ASIC_INTRCNT=0;

uint8_t		ASIC_PALAW=0;		// FL1 Palette Board Extension
uint8_t		ASIC_PALAR=0;
uint32_t	ASIC_PALVAL=0;
uint8_t		ASIC_PALCNT=0;
uint8_t		ASIC_PALMASK=0x00;

uint8_t		ASIC_PALSTORE[3*256];

uint32_t	ASIC_CP1_BLTPC = 0;
uint16_t	ASIC_CP1_BLTCMD = 0;
uint16_t	ASIC_CP1_MODE = 0;
uint16_t	ASIC_CP1_MODE2 = 0;

uint32_t	ASIC_BANK0=0;				// Z80 banking registers  (stored in upper 16bits)
uint32_t	ASIC_BANK1=0;
uint32_t	ASIC_BANK2=0;
uint32_t	ASIC_BANK3=0;

uint8_t BLT_OUTER_SRC_FLAGS;
uint8_t BLT_OUTER_DST_FLAGS;
uint16_t BLT_OUTER_CMD;
uint32_t BLT_OUTER_SRC;
uint32_t BLT_OUTER_DST;
uint8_t BLT_OUTER_MODE;
uint8_t BLT_OUTER_CPLG;
uint16_t BLT_OUTER_CNT;
uint16_t BLT_INNER_CNT;
uint8_t BLT_INNER_STEP;
uint16_t BLT_SRC_STEP;
uint16_t BLT_DST_STEP;
uint8_t BLT_PATTERN;
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

void TickBlitterCP1()								// word blitter formats
{
    if (ASIC_CP1_BLTCMD & 1)
    {
        // Step one, make the blitter "free"
#if ENABLE_DEBUG_BLITTER
        if (doShowBlits)
        {
            CONSOLE_OUTPUT("BLTPC : %08X\n",ASIC_CP1_BLTPC);
            CONSOLE_OUTPUT("Blitter Command : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d), PSIZE0-1 (%d), WIDTH0-1 (%d), LINDR (%d), YFRAC (%d), PATSEL (%d), SHADE (%d)\n",
                    ASIC_CP1_BLTCMD&0x02?1:0,
                    ASIC_CP1_BLTCMD&0x04?1:0,
                    ASIC_CP1_BLTCMD&0x08?1:0,
                    ASIC_CP1_BLTCMD&0x10?1:0,
                    ASIC_CP1_BLTCMD&0x20?1:0,
                    ASIC_CP1_BLTCMD&0x40?1:0,
                    ASIC_CP1_BLTCMD&0x80?1:0,
                    (ASIC_CP1_BLTCMD&0x300)>>8,
                    (ASIC_CP1_BLTCMD&0xC00)>>10,
                    ASIC_CP1_BLTCMD&0x1000?1:0,
                    ASIC_CP1_BLTCMD&0x2000?1:0,
                    ASIC_CP1_BLTCMD&0x4000?1:0,
                    ASIC_CP1_BLTCMD&0x8000?1:0);
        }
#endif

        BLT_OUTER_CMD=ASIC_CP1_BLTCMD;
        BLT_ENH=ASIC_BLTENH;
        ASIC_CP1_BLTCMD=0;
        ASIC_BLTENH=0;

        do
        {
#if ENABLE_DEBUG_BLITTER
            if (doShowBlits)
            {
                CONSOLE_OUTPUT("Starting Blit : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d), PSIZE0-1 (%d), WIDTH0-1 (%d), LINDR (%d), YFRAC (%d), PATSEL (%d), SHADE (%d)\n",
                        BLT_OUTER_CMD&0x02?1:0,
                        BLT_OUTER_CMD&0x04?1:0,
                        BLT_OUTER_CMD&0x08?1:0,
                        BLT_OUTER_CMD&0x10?1:0,
                        BLT_OUTER_CMD&0x20?1:0,
                        BLT_OUTER_CMD&0x40?1:0,
                        BLT_OUTER_CMD&0x80?1:0,
                        (BLT_OUTER_CMD&0x300)>>8,
                        (BLT_OUTER_CMD&0xC00)>>10,
                        BLT_OUTER_CMD&0x1000?1:0,
                        BLT_OUTER_CMD&0x2000?1:0,
                        BLT_OUTER_CMD&0x4000?1:0,
                        BLT_OUTER_CMD&0x8000?1:0);
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
            BLT_OUTER_SRC=GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;
            BLT_OUTER_SRC|=GetByte(ASIC_CP1_BLTPC)<<8;
            ASIC_CP1_BLTPC++;
            BLT_OUTER_SRC |= GetByte(ASIC_CP1_BLTPC) << 16;
            ASIC_CP1_BLTPC++;
            BLT_OUTER_SRC_FLAGS=GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;

            BLT_OUTER_DST=GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;
            BLT_OUTER_DST|=GetByte(ASIC_CP1_BLTPC)<<8;
            ASIC_CP1_BLTPC++;
            BLT_OUTER_DST|=GetByte(ASIC_CP1_BLTPC)<<16;
            ASIC_CP1_BLTPC++;
            BLT_OUTER_DST_FLAGS=GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;

            uint16_t tWord = 0;
            tWord = GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;
            tWord |= GetByte(ASIC_CP1_BLTPC) << 8;
            ASIC_CP1_BLTPC++;

            BLT_OUTER_CNT = tWord & 0x03FF;
            BLT_OUTER_CPLG = (tWord & 0xF000) >> 8;

            tWord = GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;
            tWord |= GetByte(ASIC_CP1_BLTPC) << 8;
            ASIC_CP1_BLTPC++;

            BLT_INNER_CNT = tWord & 0x03FF;


            tWord = GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;
            tWord |= GetByte(ASIC_CP1_BLTPC) << 8;
            ASIC_CP1_BLTPC++;

            BLT_SRC_STEP = tWord;

            tWord = GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;
            tWord |= GetByte(ASIC_CP1_BLTPC) << 8;
            ASIC_CP1_BLTPC++;

            BLT_DST_STEP = tWord;

            tWord = GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;
            tWord |= GetByte(ASIC_CP1_BLTPC) << 8;
            ASIC_CP1_BLTPC++;

            BLT_INNER_PAT = tWord;

#if ENABLE_DEBUG_BLITTER
            if (doShowBlits)
            {
                CONSOLE_OUTPUT("Src Address : %06X\n",BLT_OUTER_SRC&0xFFFFFF);
                /*			CONSOLE_OUTPUT("Outer Src Flags : SRCCMP (%d) , SWRAP (%d) , SSIGN (%d) , SRCA-1 (%d)\n",
                            BLT_OUTER_SRC_FLAGS&0x10?1:0,
                            BLT_OUTER_SRC_FLAGS&0x20?1:0,
                            BLT_OUTER_SRC_FLAGS&0x40?1:0,
                            BLT_OUTER_SRC_FLAGS&0x80?1:0);*/
                CONSOLE_OUTPUT("Dst Address : %06X\n",BLT_OUTER_DST&0xFFFFFF);
                /*CONSOLE_OUTPUT("Outer Dst Flags : DSTCMP (%d) , DWRAP (%d) , DSIGN (%d) , DSTA-1 (%d)\n",
                  BLT_OUTER_DST_FLAGS&0x10?1:0,
                  BLT_OUTER_DST_FLAGS&0x20?1:0,
                  BLT_OUTER_DST_FLAGS&0x40?1:0,
                  BLT_OUTER_DST_FLAGS&0x80?1:0);*/
                CONSOLE_OUTPUT("Comp Logic : %02X\n",BLT_OUTER_CPLG);
                CONSOLE_OUTPUT("Outer Cnt : %04X\n",BLT_OUTER_CNT);
                CONSOLE_OUTPUT("Inner Count : %04X\n",BLT_INNER_CNT);
                CONSOLE_OUTPUT("Src Step : %04X\n",BLT_SRC_STEP);
                CONSOLE_OUTPUT("Dst Step : %04X\n",BLT_DST_STEP);
                CONSOLE_OUTPUT("Pattern : %04X\n",BLT_PATTERN);
            }
#endif

            // HACK - just enough to perform the needed copy
            for (int a = 0;a < BLT_OUTER_CNT;a++)
            {
                for (int b = 0;b < BLT_INNER_CNT;b++)
                {
                    uint8_t val = GetByte(BLT_OUTER_SRC);
                    BLT_OUTER_SRC++;
                    SetByte(BLT_OUTER_DST, val);
                    BLT_OUTER_DST++;
                }
                BLT_OUTER_SRC += BLT_SRC_STEP;
                BLT_OUTER_DST += BLT_DST_STEP;
            }

            //DoBlit();

            BLT_OUTER_CMD = GetByte(ASIC_CP1_BLTPC);
            ASIC_CP1_BLTPC++;
            BLT_OUTER_CMD |= GetByte(ASIC_CP1_BLTPC) << 8;
            ASIC_CP1_BLTPC++;
        }
        while (BLT_OUTER_CMD&1);

        //		exit(1);

    }
}



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
                CONSOLE_OUTPUT("Enhanced Step : ENSTEP (%d), ENSTPS (%d), ENSTP8 (%d)\n",
                        BLT_ENH&0x20?1:0,
                        BLT_ENH&0x40?1:0,
                        BLT_ENH&0x80?1:0);
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

            BLT_ENH=GetByte(ASIC_BLTPC)&0xE0;
            ASIC_BLTPC++;
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
            CONSOLE_OUTPUT("BLTPC : %05X\n",ASIC_BLTPC);
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

uint8_t FL1BLT_GetByte(uint32_t addr)
{
    return GetByte(addr);
}


void FL1BLT_SetByte(uint32_t addr,uint8_t byte)
{
    SetByte(addr,byte);
}


void FL1BLT_SetProgLow(uint8_t addr);
void FL1BLT_SetProgMiddle(uint8_t addr);
void FL1BLT_SetProgHi(uint8_t addr);
void FL1BLT_SetCmd(uint8_t byte);
uint8_t FL1BLT_GetDstLo();
uint8_t FL1BLT_GetDstMi();
uint32_t FL1BLT_Step(uint8_t hold);

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

void AddressGeneratorSourceStep(int32_t step,int isStep)
{
    if ((BLT_ENH&0x20)&& isStep)
    {
        if (BLT_ENH&0x80)
        {
            step|=512;
        }
        if (BLT_ENH&0x40)
        {
            step*=-1;
        }
    }
    else
    {
        if (BLT_OUTER_SRC_FLAGS&0x40)				// SSIGN
            step*=-1;
    }
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

void AddressGeneratorDestinationStep(int32_t step,int isStep)
{
    if ((BLT_ENH&0x20) && isStep)
    {
        if (BLT_ENH&0x80)
        {
            step|=512;
        }
        if (BLT_ENH&0x40)
        {
            step*=-1;
        }
    }
    else
    {
        if (BLT_OUTER_DST_FLAGS&0x40)				// DSIGN
            step*=-1;
    }
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
            if (curSystem==ESS_P89 || curSystem==ESS_MSU)
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

    AddressGeneratorSourceStep(increment,0);
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
    AddressGeneratorDestinationStep(increment,0);
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
            AddressGeneratorDestinationStep(step,1);			// Should never reach here in line mode!
        }

        if (BLT_OUTER_CMD&0x08)
        {
            AddressGeneratorSourceStep(step,1);
        }

        if (BLT_OUTER_CMD&0x04)
        {
            if (curSystem==ESS_P89 || curSystem==ESS_MSU)		// This might apply to MSU version too (and would make more sense)
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
                if (curSystem==ESS_P89 || curSystem==ESS_MSU)
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

    if (curSystem==ESS_P89 || curSystem==ESS_MSU)
    {
        step = (BLT_INNER_STEP<<1)|((BLT_OUTER_MODE>>1)&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
        innerCnt = ((BLT_OUTER_MODE&0x1)<<8) | BLT_INNER_CNT;			//TODO PARRD will cause this (and BLT_INNER_PAT and BLT_INNER_STEP) to need to be re-read 
    }
    else
    {
        step = (BLT_INNER_STEP<<1)|(BLT_OUTER_MODE&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
        innerCnt = ((BLT_OUTER_MODE&0x2)<<7) | BLT_INNER_CNT;			//TODO PARRD will cause this (and BLT_INNER_PAT and BLT_INNER_STEP) to need to be re-read 
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
            AddressGeneratorDestinationStep(step,1);			// ?? Does step just apply, or should it be multiplied by pixel width?
        }

        if (BLT_OUTER_CMD&0x08)
        {
            uint16_t srcStep = step;
            if ((ASIC_BLTCON&0x40)==0x40)
            {
                // Src step override specificied
                srcStep = ASIC_BLTCON2;
                srcStep <<=1;
                srcStep|= ASIC_BLTCON>>7;
            }

            AddressGeneratorSourceStep(srcStep,1);
        }

        if (BLT_OUTER_CMD&0x04)
        {
            if (curSystem==ESS_P89 || curSystem==ESS_MSU)		// This might apply to MSU version too (and would make more sense)
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
                if (curSystem==ESS_P89 || curSystem==ESS_MSU)
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


            if (curSystem==ESS_P89 || curSystem==ESS_MSU)
            {
                step = (BLT_INNER_STEP<<1)|((BLT_OUTER_MODE>>1)&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
                innerCnt = ((BLT_OUTER_MODE&0x1)<<8) | BLT_INNER_CNT;			//TODO PARRD will cause this (and BLT_INNER_PAT and BLT_INNER_STEP) to need to be re-read 
            }
            else
            {
                step = (BLT_INNER_STEP<<1)|(BLT_OUTER_MODE&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
                innerCnt = ((BLT_OUTER_MODE&0x2)<<7) | BLT_INNER_CNT;			//TODO PARRD will cause this (and BLT_INNER_PAT and BLT_INNER_STEP) to need to be re-read 
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
            if (byte!=0)
                CONSOLE_OUTPUT("Warning MEM = %02X - (mem banking not implemented)\n", byte);
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
            ASIC_BLTENH=byte&0xE0;
            ASIC_BLTPC&=0x0FFFF;
            ASIC_BLTPC|=(byte&0xF)<<16;
            break;
        case 0x0043:
            ASIC_BLTCMD=byte;
            break;
        case 0x0044:
            ASIC_BLTCON=byte;
            break;
        case 0x0045:
            ASIC_BLTCON2=byte;
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
            ASIC_KINT&=0x00FF;
            ASIC_KINT|=(byte&0x04)<<6;
            ASIC_CMD1 = byte;
            if (byte&0x3B)
            {
                CONSOLE_OUTPUT("Unknown CMD1 bits set : %02X\n",byte&0x3B);
            }
            break;
        case 0x0009:			// CMD2 - bit 0 (mode 256 or 512 pixel (256 or 16 colour)
            ASIC_CMD2 = byte;
            if (byte&0x14)
            {
                CONSOLE_OUTPUT("Unknown CMD2 bits set : %02X\n",byte&0x14);
            }
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
        case 0x000E:
            ASIC_MAG = byte;
            break;
        case 0x000F:
            ASIC_YEL = byte;
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
            ASIC_INTRCNT=(~ASIC_INTRCNT)&1;
            ASIC_PROGWRD>>=8;
            ASIC_PROGWRD|=byte<<8;
            if (ASIC_INTRCNT==0)
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
        case 0x0014:
            DSP_STATUS = byte;
#if ENABLE_DEBUG
            if (doShowPortStuff)
            {
                CONSOLE_OUTPUT("DSP STATUS : %02X\n", byte);
            }
#endif
            break;
        case 0x0015:
            ASIC_PROGADDR>>=8;
            ASIC_PROGADDR|=byte<<8;
            if ((DSP_STATUS&1)==0)
            {
                //CONSOLE_OUTPUT("PERFORMING RESET : %04X\n",ASIC_PROGADDR);
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
            break;
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
        case 0x00C0:
            ASIC_CHAIR = byte;
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
        case 0x0004:		//LPEN1
            return hClock & 0xFF;
        case 0x0005:		//LPEN2
            return vClock & 0xFF;
        case 0x0006:		//LPEN3
            {
                uint8_t pen3 = (hClock >> 8) & 0x03;
                pen3 |= (vClock >> 6) & 0x04;
                pen3 |= 0;		// light pen interrupt latch
                pen3 |= (VideoInterruptLatch) << 4;
                pen3 |= (vClock & 0x1C0) >> 1;
                return pen3;
            }
        case 0x0014:		// RUNST
            return ASIC_INTRCNT;	// bit 0 == odd or even status of double write registers
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

uint32_t ConvPaletteCP1(uint32_t pal)
{
    uint32_t fCol=0;
    fCol |= (pal & 0x000000FC)<<0;
    fCol |= (pal & 0x00FC0000)>>0;
    fCol |= (pal & 0xFC000000)>>16;
    return fCol;
}
uint32_t ConvPaletteMSU(uint16_t pal)
{
    return RGB565_RGB8(pal);
}

uint32_t ConvPaletteP88(uint16_t pal)
{
    return RGB444_RGB8(pal);
}

uint8_t FL1_VECTOR = 0;		// NOP instruction - to prevent interrupts from crashing during bootup of flare one system

void DoScreenInterrupt()
{
    switch (curSystem)
    {
        case ESS_CP1:
            MSU_INTERRUPT(0xA1);
            break;//todo
        case ESS_MSU:
            MSU_INTERRUPT(0x21);
            break;
        case ESS_P89:
        case ESS_P88:
            INTERRUPT(0x21);
            break;
        case ESS_FL1:
            Z80_INTERRUPT(FL1_VECTOR);
            break;
    }
}

void DoPeripheralInterrupt()
{
    switch (curSystem)
    {
        case ESS_CP1:
        case ESS_MSU:
        case ESS_P88:
        case ESS_P89:
            break;
        case ESS_FL1:
            Z80_INTERRUPT(0x00);
            break;
    }
}

int FL1_KBD_InterruptPending();

void TickAsic(int cycles,uint32_t(*conv)(uint16_t))
{
    uint8_t palIndex;
    uint16_t palEntry;
    uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
    uint32_t screenPtr = ASIC_SCROLL;
    static uint32_t lastCol;
    uint32_t curCol;
    uint32_t wrapOffset;
//    uint16_t StartL = curSystem != ESS_MSU ? ((ASIC_STARTH & 1) << 8) | ASIC_STARTL : ASIC_STARTL;
//    uint16_t EndL = curSystem != ESS_MSU ? ((ASIC_ENDH & 1) << 8) | ASIC_ENDL : ASIC_ENDL;
    uint16_t StartL = ((ASIC_STARTH & 1) << 8) | ASIC_STARTL;
    uint16_t EndL = ((ASIC_ENDH & 1) << 8) | ASIC_ENDL;
    outputTexture+=vClock*WIDTH + hClock;

    // Video addresses are expected to be aligned to 256/128 byte boundaries - this allows for wrap to occur for a given line

    while (cycles)
    {
        TickDSP();

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
                    palEntry = (PALETTE[palIndex * 2 + 1] << 8) | PALETTE[palIndex * 2];
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
            curCol = conv(palEntry);
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
                outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
            }
        }

        cycles--;
    }
}

extern uint8_t GENLockTestingImage[256 * 256 * 3];		// 8:8:8 RGB

void TickAsicFL1_Actual(int cycles,uint32_t(*conv)(uint16_t))
{
    uint8_t palIndex;
    uint16_t palEntry;
    uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
    uint32_t screenPtr = ASIC_CMD1 & 0x40 ? 0x30000 : 0x20000;
    screenPtr|=ASIC_SCROLL & 0xFFFF;
    static uint32_t lastCol;
    uint32_t curCol;
    uint32_t wrapOffset;
    uint16_t StartL = 33;
    uint16_t EndL = 289;
    uint8_t mode = ASIC_CMD2&0x01;
    int incrust = 0;
    outputTexture+=vClock*WIDTH + hClock;


    // Video addresses are expected to be aligned to 256/128 byte boundaries - this allows for wrap to occur for a given line

    while (cycles)
    {
        TickFL1DSP();
        if (FL1_KBD_InterruptPending()||VideoInterruptLatch)
        {
            DoScreenInterrupt();
        }

        // Quick and dirty video display no contention or bus cycles
        if (hClock>=120 && hClock<632 && vClock>=StartL && vClock<EndL)
        {
            wrapOffset = (screenPtr + (vClock - StartL) * 256) & 0xFFFFFF00;
            wrapOffset |= (screenPtr + ((hClock - 120) / 2)) & 0xFF;
            palIndex = PeekByte(wrapOffset);

            if (ASIC_CMD2 & 0x80)	// variable res
            {
                mode = palIndex & 0x80 ? 1 : 0;
            }
            if (ASIC_CMD2 & 0x02)	// encrustation  (border encrustation is not handled at present)
            {
                incrust = (palIndex & 0x80) == 0;
            }
            if (mode == 0)
            {
                if (ASIC_CMD2 & 0x20)	// Mask top bit of index
                    palIndex &= 0x7F;
                if (ASIC_CMD2 & 0x40)	// Mask top 2 bits (lores only)
                    palIndex &= 0x3f;
                palEntry = (PALETTE[palIndex * 2 + 1] << 8) | PALETTE[palIndex * 2];
            }
            else
            {
                if (((hClock - 120)) & 1)
                {
                    // MSB nibble
                    palIndex >>= 4;
                }
                if (ASIC_CMD2 & 0x20)	// Mask top bit of index
                    palIndex &= 0x7;
                else
                    palIndex &= 0xF;
                if (palIndex == 5)
                    palIndex = ASIC_MAG;
                else if (palIndex == 6)
                    palIndex = ASIC_YEL;
                else if (ASIC_PALMASK==0)
                {
                    //b0 Br | B0 B1       g0 Br | G0 G1 G2    r0 Br | R0 R1 R2
                    //------ + ------    ------ + -------- - ------ + -------- -
                    //  0  0 | 0  0        0  0 | 0  0  0      0  0 | 0  0  0
                    //  0  1 | 1  0        0  1 | 0  1  0      0  1 | 0  1  0
                    //  1  0 | 0  1        1  0 | 1  0  1      1  0 | 1  0  1
                    //  1  1 | 1  1        1  1 | 1  1  1      1  1 | 1  1  1
                    //------ + ------    ------ + -------- - ------ + -------- -

                    uint8_t physicalColour = (palIndex & 8) ? 0xB6 : 0x00;
                    physicalColour |= (palIndex & 4) ? 0x40 : 0x00;
                    physicalColour |= (palIndex & 2) ? 0x08 : 0x00;
                    physicalColour |= (palIndex & 1) ? 0x01 : 0x00;
                    palIndex = physicalColour;
                }
                palEntry = (PALETTE[palIndex * 2 + 1] << 8) | PALETTE[palIndex * 2];
            }

            if (incrust)
            {
                uint32_t pixelOffset = (vClock - StartL) * 256 * 3 + ((hClock - 120) / 2) * 3;
                uint32_t outputValue = GENLockTestingImage[pixelOffset+2];
                outputValue |= GENLockTestingImage[pixelOffset+1] << 8;
                outputValue |= GENLockTestingImage[pixelOffset+0] << 16;
                *outputTexture++ = outputValue;
            }
            else
            {
                curCol = conv(palEntry);
                if ((ASIC_CMD2 & 0x08) && (palIndex == ASIC_COLHOLD))
                {
                    *outputTexture++ = lastCol;
                }
                else
                {
                    *outputTexture++ = curCol;
                    lastCol = curCol;
                }
            }
        }
        else
        {
            palIndex = ASIC_BORD;
            palEntry = (PALETTE[palIndex * 2 + 1] << 8) | PALETTE[palIndex * 2];
            *outputTexture++=conv(palEntry);
        }

        hClock++;
        if ((hClock==2) && (ASIC_KINT==vClock) && ((ASIC_DIS&0x1)==0))			//  Flare 1 interrupt generated just after hsync
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
                outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
            }
        }

        cycles--;
    }
}

void ShowOffScreenFL1(uint32_t(*conv)(uint16_t))
{
    uint8_t palIndex;
    uint16_t palEntry;
    uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
    uint32_t screenPtr = ASIC_CMD1 & 0x40 ? 0x30000 : 0x20000;
    screenPtr|=ASIC_SCROLL & 0xFFFF;
    static uint32_t lastCol;
    uint32_t curCol;
    uint32_t wrapOffset;
    uint16_t StartL = 33;
    uint16_t EndL = 289;
    uint8_t mode = ASIC_CMD2&0x01;
    int incrust = 0;
    uint32_t cycles = WIDTH * HEIGHT;
    uint32_t hClock=0;
    uint32_t vClock=0;


    // Video addresses are expected to be aligned to 256/128 byte boundaries - this allows for wrap to occur for a given line

    while (cycles)
    {
        // Quick and dirty video display no contention or bus cycles
        if (hClock>=120 && hClock<632 && vClock>=StartL && vClock<EndL)
        {
            wrapOffset = (screenPtr + (vClock - StartL) * 256) & 0xFFFFFF00;
            wrapOffset |= (screenPtr + ((hClock - 120) / 2)) & 0xFF;
            palIndex = PeekByte(wrapOffset);

            if (ASIC_CMD2 & 0x80)	// variable res
            {
                mode = palIndex & 0x80 ? 1 : 0;
            }
            if (ASIC_CMD2 & 0x02)	// encrustation  (border encrustation is not handled at present)
            {
                incrust = (palIndex & 0x80) == 0;
            }
            if (mode == 0)
            {
                if (ASIC_CMD2 & 0x20)	// Mask top bit of index
                    palIndex &= 0x7F;
                if (ASIC_CMD2 & 0x40)	// Mask top 2 bits (lores only)
                    palIndex &= 0x3f;
                palEntry = (PALETTE[palIndex * 2 + 1] << 8) | PALETTE[palIndex * 2];
            }
            else
            {
                if (((hClock - 120)) & 1)
                {
                    // MSB nibble
                    palIndex >>= 4;
                }
                if (ASIC_CMD2 & 0x20)	// Mask top bit of index
                    palIndex &= 0x7;
                else
                    palIndex &= 0xF;
                if (palIndex == 5)
                    palIndex = ASIC_MAG;
                else if (palIndex == 6)
                    palIndex = ASIC_YEL;
                else if (ASIC_PALMASK==0)
                {
                    //b0 Br | B0 B1       g0 Br | G0 G1 G2    r0 Br | R0 R1 R2
                    //------ + ------    ------ + -------- - ------ + -------- -
                    //  0  0 | 0  0        0  0 | 0  0  0      0  0 | 0  0  0
                    //  0  1 | 1  0        0  1 | 0  1  0      0  1 | 0  1  0
                    //  1  0 | 0  1        1  0 | 1  0  1      1  0 | 1  0  1
                    //  1  1 | 1  1        1  1 | 1  1  1      1  1 | 1  1  1
                    //------ + ------    ------ + -------- - ------ + -------- -

                    uint8_t physicalColour = (palIndex & 8) ? 0xB6 : 0x00;
                    physicalColour |= (palIndex & 4) ? 0x40 : 0x00;
                    physicalColour |= (palIndex & 2) ? 0x08 : 0x00;
                    physicalColour |= (palIndex & 1) ? 0x01 : 0x00;
                    palIndex = physicalColour;
                }
                palEntry = (PALETTE[palIndex * 2 + 1] << 8) | PALETTE[palIndex * 2];
            }

            if (incrust)
            {
                uint32_t pixelOffset = (vClock - StartL) * 256 * 3 + ((hClock - 120) / 2) * 3;
                uint32_t outputValue = GENLockTestingImage[pixelOffset+2];
                outputValue |= GENLockTestingImage[pixelOffset+1] << 8;
                outputValue |= GENLockTestingImage[pixelOffset+0] << 16;
                *outputTexture++ = outputValue;
            }
            else
            {
                curCol = conv(palEntry);
                if ((ASIC_CMD2 & 0x08) && (palIndex == ASIC_COLHOLD))
                {
                    *outputTexture++ = lastCol;
                }
                else
                {
                    *outputTexture++ = curCol;
                    lastCol = curCol;
                }
            }
        }
        else
        {
            palIndex = ASIC_BORD;
            palEntry = (PALETTE[palIndex * 2 + 1] << 8) | PALETTE[palIndex * 2];
            *outputTexture++=conv(palEntry);
        }

        hClock++;
        if (hClock==(WIDTH))
        {
            hClock=0;
            vClock++;
            if (vClock==(HEIGHT))
            {
                vClock=0;
                outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
            }
        }

        cycles--;
    }
}
void ShowOffScreen(uint32_t(*conv)(uint16_t),uint32_t swapBuffer)
{
    uint8_t palIndex;
    uint16_t palEntry;
    uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
    uint32_t screenPtr = /*ASIC_SCROLL^*/swapBuffer;
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
                outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
            }
        }

        cycles--;
    }
}
extern unsigned char CP1_PALETTE[256 * 4];

void DoCP1Screen(int cycles)
{
    uint8_t palIndex;
    uint32_t palEntry;
    uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
    uint32_t screenPtr = ASIC_SCROLL&0x00FFFFFF;
    uint32_t curCol;
    uint32_t wrapOffset;
    outputTexture+=vClock*WIDTH + hClock;
    uint32_t* endOutput = (uint32_t*)(videoMemory[MAIN_WINDOW]);
    endOutput += WIDTH * HEIGHT;

    while (cycles)
    {
        // Quick and dirty video display no contention or bus cycles
        if (hClock>=120 && hClock<632 && vClock>20 && vClock<=220)
        {
            int width = (ASIC_CP1_MODE & 0x70)>>4;
            switch (width)
            {
                default:
                case 1:
                    wrapOffset = (screenPtr + ((vClock - 20) - 1) * 256) & 0xFFFFFF00;
                    wrapOffset |= (screenPtr + ((hClock - 120) / 2)) & 0xFF;
                    break;
                case 2:
                    wrapOffset = (screenPtr + ((vClock - 20) - 1) * 512) & 0xFFFFFE00;
                    wrapOffset |= (screenPtr + ((hClock - 120) / 1)) & 0x1FF;
                    break;
            }

            palIndex = PeekByte(wrapOffset);

            palEntry = (CP1_PALETTE[palIndex * 4 + 3] << 24) | (CP1_PALETTE[palIndex * 4 + 2] << 16) | (CP1_PALETTE[palIndex * 4 + 1] << 8) | (CP1_PALETTE[palIndex * 4 + 0] << 0);

            curCol=ConvPaletteCP1(palEntry);
            *outputTexture++=curCol;
        }
        else
        {
            *outputTexture++=ConvPaletteMSU(ASIC_BORD);
        }

        // This is a quick hack up of the screen functionality -- at present simply timing related to get interrupts to fire
        if (VideoInterruptLatch)
        {
            DoScreenInterrupt();		
        }

        if (ASIC_CP1_MODE2 & 4)	// VIDEO TIMER ENABLED
        {
            hClock++;
            if ((hClock == 631) && (ASIC_KINT == vClock) && ((ASIC_DIS & 0x1) == 1))			//  Docs state interrupt fires at end of active display of KINT line
            {
                VideoInterruptLatch = 1;
            }
            if (hClock == (WIDTH))
            {
                hClock = 0;
                vClock++;
                if (vClock == (HEIGHT))
                {
                    vClock = 0;
                    outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
                }
            }
        }
        cycles--;
    }
}


void TickAsicCP1(int cycles)
{
    TickBlitterCP1();
    DoCP1Screen(cycles);
}


void TickAsicMSU(int cycles)
{
    TickBlitterP89();
    TickAsic(cycles,ConvPaletteMSU);
}

void PrintAt(unsigned char* buffer,unsigned int width,unsigned char r,unsigned char g,unsigned char b,unsigned int x,unsigned int y,const char *msg,...);

extern uint8_t DISK_IMAGE[5632*2*80];

int16_t GetEDDY_Bit()
{
    int16_t toReturn = DISK_IMAGE[EDDY_Track*5632*2 + EDDY_Side*5632 + (EDDY_BitPos/8)];
    toReturn >>= EDDY_BitPos & 7;
    return EDDY_BitPos & 1;
}

void TickAsicP89(int cycles)
{
    TickBlitterP89();
    TickAsic(cycles,ConvPaletteP88);

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

extern uint8_t PotXValue;
extern uint8_t PotYValue;
extern uint8_t PotZValue;
extern uint8_t PotLPValue;
extern uint8_t PotRPValue;
extern uint8_t PotSpareValue;
extern uint8_t ASIC_FL1_GPO;

void ShowPotsDebug()
{
    PrintAt(videoMemory[MAIN_WINDOW],windowWidth[MAIN_WINDOW],255,255,255,1,1,"POTX(%02X) POTY(%02X) POTZ(%02X)", PotXValue, PotYValue, PotZValue);
    PrintAt(videoMemory[MAIN_WINDOW],windowWidth[MAIN_WINDOW],255,255,255,1,2,"POTLP(%02X) POTRP(%02X) POTSP(%02X)", PotLPValue, PotRPValue, PotSpareValue);
    PrintAt(videoMemory[MAIN_WINDOW],windowWidth[MAIN_WINDOW],255,255,255,1,3,"CHAIR0?(%02X) : GPO(%02X)", ASIC_CHAIR, ASIC_FL1_GPO);
}

void TickAsicP88(int cycles)
{
    TickBlitterP88();
    TickAsic(cycles,ConvPaletteP88);
}

void TickAsicFL1(int cycles)
{
    // There are 2 screens on FLARE 1 (they are hardwired unlike later versions) - 1 at 0x20000 and the other at 0x30000
    TickAsicFL1_Actual(cycles,ConvPaletteP88);
}

unsigned int offscreenAddress=0x30000;

void DebugDrawOffScreen()
{
    if (curSystem == ESS_FL1)
        ShowOffScreenFL1(ConvPaletteP88);
    else if (curSystem==ESS_MSU)
        ShowOffScreen(ConvPaletteMSU, offscreenAddress);
    else
        ShowOffScreen(ConvPaletteP88,0x10000);
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
    ASIC_BLTCON2=0;
    ASIC_BLTCMD=0;
    ASIC_BLTPC=0;				// 20 bit address
    ASIC_COLHOLD=0;					// Not changeable on later than Flare One revision
    ASIC_MAG = 0x55;			//FL1 - hires
    ASIC_YEL = 0x66;			//FL1 - hires
    ASIC_CMD1 = 0;				//FL1
    ASIC_CMD2 = 0;				//FL1

    ASIC_PROGWRD=0;
    ASIC_PROGADDR=0;
    ASIC_INTRA=0;
    ASIC_INTRD=0;
    ASIC_INTRCNT=0;

    ASIC_PALAW=0;
    ASIC_PALVAL=0;
    ASIC_PALCNT=0;
    ASIC_PALAR=0;
    ASIC_PALMASK=0;

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
