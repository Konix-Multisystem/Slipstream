/*

	ASIC test

	Currently contains some REGISTERS and some video hardware - will move to EDL eventually

	Need to break this up some more, blitter going here temporarily
*/


#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <stdint.h>
#include <string.h>

#include "video.h"
#include "audio.h"
#include "asic.h"

#define RGB444_RGB8(x)		( ((x&0x000F)<<4) | ((x&0x00F0)<<8) | ((x&0x0F00)<<12) )				// Old slipstream is 444 format
#define RGB565_RGB8(x)		( ((x&0xF800)<<8) | ((x&0x07E0) <<5) | ((x&0x001F)<<3) )				// Later revisions are 565

#define BLTDDBG(...)		//printf(__VA_ARGS__);

void INTERRUPT(uint8_t);

int hClock=0;
int vClock=0;
int VideoInterruptLatch=0;

int doShowBlits=1;
int doShowHostDSPWrites=0;
int doShowHostDSPReads=0;

// Current ASIC registers

uint16_t	ASIC_KINT=0x00FF;
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
uint8_t		ASIC_BLTCMD=0;
uint32_t	ASIC_BLTPC=0;				// 20 bit address

uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);


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

void DoBlit();

void TickBlitterMSU()								// TODO - make this more modular!!!
{
	// Step one, make the blitter "free"
#if ENABLE_DEBUG
	if (doShowBlits)
	{
		printf("Blitter Command : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
			ASIC_BLTCMD&0x02?1:0,
			ASIC_BLTCMD&0x04?1:0,
			ASIC_BLTCMD&0x08?1:0,
			ASIC_BLTCMD&0x10?1:0,
			ASIC_BLTCMD&0x20?1:0,
			ASIC_BLTCMD&0x40?1:0,
			ASIC_BLTCMD&0x80?1:0);
	}
#endif

	if (ASIC_BLTCMD & 1)
	{
		BLT_OUTER_CMD=ASIC_BLTCMD;		// First time through we don't read the command		-- Note the order of data appears to differ from the docs - This is true of MSU version!!

		do
		{
#if ENABLE_DEBUG
		if (doShowBlits)
		{
			printf("Starting Blit : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
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
			printf("Unsupported BLT CMD type\n");
			exit(1);
		}


		if (doShowBlits)
		{
			printf("Fetching Program Sequence :\n");
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

#if ENABLE_DEBUG
		if (doShowBlits)
		{
			printf("Src Address : %05X\n",BLT_OUTER_SRC&0xFFFFF);
			printf("Outer Cnt : %02X\n",BLT_OUTER_CNT);
			printf("Dst Address : %05X\n",BLT_OUTER_DST&0xFFFFF);
			printf("Comp Logic : %02X\n",BLT_OUTER_CPLG);
			printf("Inner Count : %02X\n",BLT_INNER_CNT);
			printf("Mode Control : %02X\n",BLT_OUTER_MODE);
			printf("Pattern : %02X\n",BLT_INNER_PAT);
			printf("Step : %02X\n",BLT_INNER_STEP);
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

void TickBlitterP88()
{
	// Step one, make the blitter "free"
#if ENABLE_DEBUG
	if (doShowBlits)
	{
		printf("Blitter Command : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
			ASIC_BLTCMD&0x02?1:0,
			ASIC_BLTCMD&0x04?1:0,
			ASIC_BLTCMD&0x08?1:0,
			ASIC_BLTCMD&0x10?1:0,
			ASIC_BLTCMD&0x20?1:0,
			ASIC_BLTCMD&0x40?1:0,
			ASIC_BLTCMD&0x80?1:0);
	}
#endif

	if (ASIC_BLTCMD & 1)
	{
		BLT_OUTER_CMD=ASIC_BLTCMD;		// First time through we don't read the command	

		do
		{
#if ENABLE_DEBUG
		if (doShowBlits)
		{
			printf("Starting Blit : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
				BLT_OUTER_CMD&0x02?1:0,
				BLT_OUTER_CMD&0x04?1:0,
				BLT_OUTER_CMD&0x08?1:0,
				BLT_OUTER_CMD&0x10?1:0,
				BLT_OUTER_CMD&0x20?1:0,
				BLT_OUTER_CMD&0x40?1:0,
				BLT_OUTER_CMD&0x80?1:0);
		}

		if (BLT_OUTER_CMD&0x46)
		{
			printf("Unsupported BLT CMD type\n");
			exit(1);
		}


		if (doShowBlits)
		{
			printf("Fetching Program Sequence :\n");
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


#if ENABLE_DEBUG
		if (doShowBlits)
		{
			printf("BLIT CMD : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
				BLT_OUTER_CMD&0x02?1:0,
				BLT_OUTER_CMD&0x04?1:0,
				BLT_OUTER_CMD&0x08?1:0,
				BLT_OUTER_CMD&0x10?1:0,
				BLT_OUTER_CMD&0x20?1:0,
				BLT_OUTER_CMD&0x40?1:0,
				BLT_OUTER_CMD&0x80?1:0);
			printf("Src Address : %05X\n",BLT_OUTER_SRC&0xFFFFF);
			printf("Src Flags : SRCCMP (%d) , SWRAP (%d) , SSIGN (%d) , SRCA-1 (%d)\n",
				BLT_OUTER_SRC_FLAGS&0x10?1:0,
				BLT_OUTER_SRC_FLAGS&0x20?1:0,
				BLT_OUTER_SRC_FLAGS&0x40?1:0,
				BLT_OUTER_SRC_FLAGS&0x80?1:0);
			printf("Dst Address : %05X\n",BLT_OUTER_DST&0xFFFFF);
			printf("Dst Flags : DSTCMP (%d) , DWRAP (%d) , DSIGN (%d) , DSTA-1 (%d)\n",
				BLT_OUTER_DST_FLAGS&0x10?1:0,
				BLT_OUTER_DST_FLAGS&0x20?1:0,
				BLT_OUTER_DST_FLAGS&0x40?1:0,
				BLT_OUTER_DST_FLAGS&0x80?1:0);
			printf("BLT_MODE : STEP-1 (%d) , ILCNT (%d) , CMPBIT (%d) , LINDR (%d) , YFRAC (%d) , RES0 (%d) , RES1 (%d), PATSEL (%d)\n",
				BLT_OUTER_MODE&0x01?1:0,
				BLT_OUTER_MODE&0x02?1:0,
				BLT_OUTER_MODE&0x04?1:0,
				BLT_OUTER_MODE&0x08?1:0,
				BLT_OUTER_MODE&0x10?1:0,
				BLT_OUTER_MODE&0x20?1:0,
				BLT_OUTER_MODE&0x40?1:0,
				BLT_OUTER_MODE&0x80?1:0);
			printf("BLT_COMP : CMPEQ (%d) , CMPNE (%d) , CMPGT (%d) , CMPLN (%d) , LOG0 (%d) , LOG1 (%d) , LOG2 (%d), LOG3 (%d)\n",
				BLT_OUTER_CPLG&0x01?1:0,
				BLT_OUTER_CPLG&0x02?1:0,
				BLT_OUTER_CPLG&0x04?1:0,
				BLT_OUTER_CPLG&0x08?1:0,
				BLT_OUTER_CPLG&0x10?1:0,
				BLT_OUTER_CPLG&0x20?1:0,
				BLT_OUTER_CPLG&0x40?1:0,
				BLT_OUTER_CPLG&0x80?1:0);
			printf("Outer Cnt : %02X\n",BLT_OUTER_CNT);
			printf("Inner Count : %02X\n",BLT_INNER_CNT);
			printf("Step : %02X\n",BLT_INNER_STEP);
			printf("Pattern : %02X\n",BLT_INNER_PAT);
			//getch();
		}
#endif

		DoBlit();

		BLT_OUTER_CMD=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
#if ENABLE_DEBUG
		if (doShowBlits)
		{
			printf("Next Blit : COLST (%d) , PARRD (%d) , SCRUP (%d) , DSTUP (%d) , SRCEN (%d) , DSTEN (%d) , SCRENF (%d)\n",
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

uint32_t ADDRESSGENERATOR_SRCADDRESS;			// 21 bit  - LSB = nibble
uint32_t ADDRESSGENERATOR_DSTADDRESS;			// 21 bit  - LSB = nibble

uint16_t DATAPATH_SRCDATA;
uint8_t DATAPATH_DSTDATA;
uint8_t DATAPATH_PATDATA;
uint16_t DATAPATH_DATAOUT;

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

uint16_t BLT_INNER_CUR_CNT;

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
	BLTDDBG("SRCADDR %06X (%d)\n",ADDRESSGENERATOR_SRCADDRESS,step);
	if (BLT_OUTER_SRC_FLAGS&0x40)				// SSIGN
		step*=-1;
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
	BLTDDBG("DSTADDR %06X (%d)\n",ADDRESSGENERATOR_DSTADDRESS,step);
	if (BLT_OUTER_DST_FLAGS&0x40)				// DSIGN
		step*=-1;
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
			if (ADDRESSGENERATOR_DSTADDRESS&1)			// ODD address LSNibble
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
			DATAPATH_DSTDATA=GetByte(ADDRESSGENERATOR_SRCADDRESS>>1);
			break;
		case 0x60:				//16 bits (N/A)  - Not sure if this should read both (since destination is only 8 bits!!!
			DATAPATH_DSTDATA=(GetByte(ADDRESSGENERATOR_SRCADDRESS>>1)<<8)|GetByte((ADDRESSGENERATOR_SRCADDRESS>>1)+1);
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

void DoBlitInner()
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
			// TODO - check for collision stop
		}
		else
		{
			AddressGeneratorDestinationWrite();
		}

		AddressGeneratorDestinationUpdate();

		BLT_INNER_CUR_CNT--;

	}while (BLT_INNER_CUR_CNT&0x1FF);
}


void DoBlitOuter()
{
	uint8_t outerCnt = BLT_OUTER_CNT;

	uint16_t step = (BLT_INNER_STEP<<1)|(BLT_OUTER_MODE&1);			// STEP-1  (nibble bit is used only in high resolution mode according to docs, hmmm)
	uint16_t innerCnt = ((BLT_OUTER_MODE&0x2)<<7) | BLT_INNER_CNT;			//TODO PARRD will cause this (and BLT_INNER_PAT and BLT_INNER_STEP) to need to be re-read 

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
		DoBlitInner();

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
	}
}

void DoBlit()
{
	ADDRESSGENERATOR_SRCADDRESS=(BLT_OUTER_SRC<<1) | ((BLT_OUTER_SRC_FLAGS&0x80)>>7);
	ADDRESSGENERATOR_DSTADDRESS=(BLT_OUTER_DST<<1) | ((BLT_OUTER_DST_FLAGS&0x80)>>7);
	BLTDDBG("SRCADDR : %06X\n",ADDRESSGENERATOR_SRCADDRESS);
	BLTDDBG("DSTADDR : %06X\n",ADDRESSGENERATOR_DSTADDRESS);

	DoBlitOuter();
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
			TickBlitterMSU();
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
				printf("256 Colour Screen Mode\n");
			else
				printf("16 Colour Screen Mode\n");
			if (byte&0xFE)
				printf("Warning unhandled MODE bits set - likely to be an emulation mismatch\n");
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
				printf("Warning PMASK!=0 - likely to be an emulation mismatch\n");
			break;
		case 0x0010:
			ASIC_INDEX=byte;
			if (byte!=0)
				printf("Warning INDEX!=0 - likely to be an emulation mismatch\n");
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
				printf("Warning MEM!=0 - (mem banking not implemented)\n");
			break;
		case 0x0015:
			ASIC_DIAG=byte;
			if (byte!=0)
				printf("Warning DIAG!=0 - (Diagnostics not implemented)\n");
			break;
		case 0x0016:
			ASIC_DIS=byte;
			if (byte&0xFE)
				printf("Warning unhandled DIS bits set (%02X)- (Other intterupts not implemented)\n",byte);
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
			TickBlitterP88();
			break;
		case 0x0034:
			ASIC_BLTCON=byte;
			if (byte!=0)
				printf("Warning BLTCON!=0 - (Blitter control not implemented)\n");
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
		default:
#if ENABLE_DEBUG
			if (warnIgnore)
			{
				printf("ASIC READ IGNORE %04X - TODO?\n",port);
			}
#endif
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

extern unsigned char PALETTE[256*2];
void DSP_STEP(void);

void DSP_SetDAC(uint8_t channels,uint16_t value)
{
	// Bleep
	if (channels&1)
	{
		_AudioAddData(0,value);			//(14 bit DAC)
	}
	if (channels&2)
	{
		_AudioAddData(1,value);			//(14 bit DAC)
	}
}

#if ENABLE_DEBUG

extern uint8_t *DSP_DIS_[32];			// FROM EDL

extern uint16_t	DSP_DEBUG_PC;

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
	int cond=(word&0x400)>>9;
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
			if (*sPtr=='.')
			{
				if (cond)
				{
					*dPtr++=*sPtr;
					*dPtr++='C';
				}
				if (index)
				{
					*dPtr++=*sPtr;
					*dPtr++='X';
				}
			}
			else
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

			sprintf(sprintBuffer,"%s",DSP_LookupAddress(data));
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

int doDSPDisassemble=0;

#define RATE_ADJUST	(1)			//TODO this should be read from the MODE register and it should affect the DAC conversion speed not the DSP execution speed

void DoDSP()
{
#if !DISABLE_DSP
	static int iamslow=RATE_ADJUST;
	if (DSP[0xFF0]&0x10)
	{
		if (iamslow==0)
		{

#if ENABLE_DEBUG
		if (DSP_DEBUG_PC==0x78)
		{
			doDSPDisassemble=0;
		}
		if (doDSPDisassemble)
		{
			DSP_Disassemble(DSP_DEBUG_PC,1);
		}
#endif
		DSP_STEP();

			iamslow=RATE_ADJUST;
		}
		else
			iamslow--;
	}
#endif
}

uint8_t PeekByte(uint32_t addr);

uint32_t ConvPaletteMSU(uint16_t pal)
{
	return RGB565_RGB8(pal);
}

uint32_t ConvPaletteP88(uint16_t pal)
{
	return RGB444_RGB8(pal);
}

void TickAsic(int cycles,uint32_t(*conv)(uint16_t))
{
	uint8_t palIndex;
	uint16_t palEntry;
	uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	uint32_t screenPtr = ASIC_SCROLL;
	uint32_t wrapOffset;
	uint16_t StartL = ((ASIC_STARTH&1)<<8)|ASIC_STARTL;
	uint16_t EndL = ((ASIC_ENDH&1)<<8)|ASIC_ENDL;
	outputTexture+=vClock*WIDTH + hClock;

	// Video addresses are expected to be aligned to 256/128 byte boundaries - this allows for wrap to occur for a given line

	while (cycles)
	{
		DoDSP();

		// This is a quick hack up of the screen functionality -- at present simply timing related to get interrupts to fire
		if (VideoInterruptLatch)
		{
			INTERRUPT(0x21);
		}

		// Quick and dirty video display no contention or bus cycles
		if (hClock>=120 && hClock<632 && vClock>StartL && vClock<=EndL)
		{
			switch (ASIC_MODE&1)
			{
				case 0:			// LoRes (2 nibbles per pixel)
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

				case 1:			// MediumRes (1 byte per pixel)
					wrapOffset=(screenPtr+((vClock-StartL)-1)*256)&0xFFFFFF00;
					wrapOffset|=(screenPtr+((hClock-120)/2))&0xFF;

					palIndex = PeekByte(wrapOffset);
					palEntry = (PALETTE[palIndex*2+1]<<8)|PALETTE[palIndex*2];

					break;
			}
			*outputTexture++=conv(palEntry);
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
			}
		}

		cycles--;
	}
}

void TickAsicMSU(int cycles)
{
	TickAsic(cycles,ConvPaletteMSU);
}

void TickAsicP88(int cycles)
{
	TickAsic(cycles,ConvPaletteP88);
}

void ASIC_HostDSPMemWrite(uint16_t addr,uint8_t byte)
{
	if (addr>=0x800 && addr<0xE00 )
	{
#if ENABLE_DEBUG
		if (addr&1)
		{
			if (doShowHostDSPWrites)
			{
				uint16_t pWord = DSP[addr-1] | (byte<<8);
				uint16_t pAddr=(pWord&0x1FF)*2;		// bottom 9 bits - multiply 2 because word addresses make less sense to me at moment
				uint16_t pOpcode=(pWord&0xF800)>>11;		// top 5 bits?
				uint8_t isConditional=(pWord&0x0400)>>10;
				uint8_t isIndexed=(pWord&0x0200)>>9;

				printf("Host Write To DSP Prog %04X <- %04X ",addr-1,pWord);

				// Quick test

				switch (pOpcode)
				{
					case 0:
						printf("%s MOV (%04X%s),MZ0\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 1:
						printf("%s MOV (%04X%s),MZ1\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 2:
						printf("%s MOV MZ0,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 3:
						printf("%s MOV MZ1,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 4:
						printf("%s CCF\n",isConditional?"IF C THEN":"");
						break;
					case 5:
						printf("%s MOV DMA0,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 6:
						printf("%s MOV DMA1,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 7:
						printf("%s MOV DMD,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 8:
						printf("%s MOV (%04X%s),DMD\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 9:
						printf("%s MAC (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 10:
						printf("%s MOV MODE,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 11:
						printf("%s MOV IX,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 12:
						printf("%s MOV (%04X%s),PC\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 13:
						printf("%s MOV X,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 14:
						printf("%s MOV (%04X%s),X\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 15:
						printf("%s MULT (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 16:
						printf("%s ADD (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 17:
						printf("%s SUB (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 18:
						printf("%s AND (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 19:
						printf("%s OR (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 20:
						printf("%s ADC (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 21:
						printf("%s SBC (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 22:
						printf("%s MOV (%04X%s),AZ\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 23:
						printf("%s MOV AZ,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 24:
						printf("%s MOV (%04X%s),Z2\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 25:
						printf("%s MOV DAC1,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 26:
						printf("%s MOV DAC2,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 27:
						printf("%s MOV DAC12,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 28:
						printf("%s GAI (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 29:
						printf("%s MOV PC,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
						break;
					case 30:
						printf("%s NOP\n",isConditional?"IF C THEN":"");
						break;
					case 31:
						printf("%s INTRUDE\n",isConditional?"IF C THEN":"");
						break;
				}
			}
		}
#endif
		DSP[addr]=byte;
	}
	else
	{
		if (addr>=0x280 && addr<0x300)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP[addr-1] | (byte<<8);
					printf("Host Write To DSP Registers : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			DSP[addr]=byte;
		}
		if (addr>=0x200 && addr<0x280)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP[addr-1] | (byte<<8);
					printf("Host Write To DSP Constants (ignored) : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			// Don't preform write (this is rom space)
		}
		if (addr<0x200)
		{
			if (DSP[0x14B*2] & 0x80)		// Alternate memory mapping enabled
			{
#if ENABLE_DEBUG
				if (addr&1)
				{
					if (doShowHostDSPWrites)
					{
						uint16_t pWord = DSP[addr-1] | (byte<<8);
						printf("Host Write To DSP Data (Alternate Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
					}
				}
#endif
				DSP[addr+0x300]=byte;
			}
			else
			{
#if ENABLE_DEBUG
				if (addr&1)
				{
					if (doShowHostDSPWrites)
					{
						uint16_t pWord = DSP[addr-1] | (byte<<8);
						printf("Host Write To DSP ROM (ignored) (Normal Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
					}
				}
#endif
				// Don't preform write (this is rom space)
			}
		}
		if (addr>=0x300 && addr<0x500)
		{
			if (DSP[0x14B*2] & 0x80)		// Alternate memory mapping enabled
			{
#if ENABLE_DEBUG
				if (addr&1)
				{
					if (doShowHostDSPWrites)
					{
						uint16_t pWord = DSP[addr-1] | (byte<<8);
						printf("Host Write To DSP ROM (ignored) (Alternate Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
					}
				}
#endif
				// Don't preform write (this is rom space)
			}
			else
			{
#if ENABLE_DEBUG
				if (addr&1)
				{
					if (doShowHostDSPWrites)
					{
						uint16_t pWord = DSP[addr-1] | (byte<<8);
						printf("Host Write To DSP Data (Normal Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
					}
				}
#endif
				DSP[addr]=byte;
			}
		}
		if (addr>=0x500 && addr<0x800)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP[addr-1] | (byte<<8);
					printf("Host Write To DSP Data : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			DSP[addr]=byte;
		}
		if (addr>=0xE00)
		{
#if ENABLE_DEBUG
			if (doShowHostDSPWrites)
			{
				printf("Host Write to DSP Data (Unknown (FF0 status!)) : %04X\n",addr);
			}
#endif
			DSP[addr]=byte;
		}
	}
}

uint8_t ASIC_HostDSPMemRead(uint16_t addr)
{
	if (addr>=0x800 && addr<0xE00)
	{
#if ENABLE_DEBUG
		if (doShowHostDSPReads)
		{
			printf("Host DSP Prog Read (TODO deny when running) : %04X\n",addr);
		}
#endif
		return DSP[addr];
	}
	else
	{
		if (addr<0x800)
		{
			if (DSP[0x14B*2] & 0x80)		// Alternate memory mapping enabled
			{
#if ENABLE_DEBUG
				if (doShowHostDSPReads)
				{
					printf("Host DSP Data Read (Alternate Map) : %04X\n",addr);
				}
#endif
				if (addr<0x200)
				{
					return DSP[addr+0x300];
				}
				if (addr>=0x300 && addr<0x500)
				{
					return DSP[addr-0x300];
				}
			}
			else
			{
#if ENABLE_DEBUG
				if (doShowHostDSPReads)
				{
					printf("Host DSP Data Read (Normal Map) : %04X\n",addr);
				}
#endif
				return DSP[addr];
			}
		}
		else
		{
#if ENABLE_DEBUG
			if (doShowHostDSPReads)
			{
				printf("Host DSP Data Read (Unknown (FF0 status!)) : %04X\n",addr);
			}
#endif
		}
	}
	return DSP[addr];
}
