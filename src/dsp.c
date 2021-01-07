/*

	DSP memory handling

	Early revision of the DSP has an almost completely different memory map :(

*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if OS_WINDOWS
#include <conio.h>
#endif

#include "logfile.h"
#include "dsp.h"
#include "audio.h"
#include "system.h"

uint16_t DSP_STATUS=0;

extern uint8_t DSP_INTRUDE_HACK;

uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);

void DSP_POKE(uint16_t,uint16_t);
uint16_t DSP_PEEK(uint16_t);
void DSP_POKE_BYTE(uint16_t,uint8_t);
uint8_t DSP_PEEK_BYTE(uint16_t);
void FL1DSP_POKE(uint16_t,uint16_t);
uint16_t FL1DSP_PEEK(uint16_t);
void FL1DSP_POKE_BYTE(uint16_t,uint8_t);
uint8_t FL1DSP_PEEK_BYTE(uint16_t);

int doDSPDisassemble=0;
int doShowHostDSPWrites=0;
int doShowHostDSPReads=0;
int doShowDMA=0;

uint16_t DSP_DMAGetWord(uint32_t addr)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		CONSOLE_OUTPUT("DSP DMA HOST->DSP %05X\n",addr&0xFFFFE);
	}
#endif
	return GetByte(addr&0xFFFFE)|(GetByte((addr&0xFFFFE) +1)<<8);
}

void DSP_DMASetWord(uint32_t addr,uint16_t word)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		CONSOLE_OUTPUT("DSP DMA DSP->HOST %05X (%04X)\n",addr&0xFFFFE,word);
	}
#endif
	SetByte(addr&0xFFFFE,word&0xFF);
	SetByte((addr&0xFFFFE)+1,word>>8);
}

uint8_t DSP_DMAGetByte(uint32_t addr)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		CONSOLE_OUTPUT("DSP DMA HOST->DSP %05X\n",addr&0xFFFFF);
	}
#endif
	return GetByte(addr&0xFFFFF);
}

void DSP_DMASetByte(uint32_t addr,uint8_t byte)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		CONSOLE_OUTPUT("DSP DMA DSP->HOST %05X (%02X)\n",addr&0xFFFFF,byte);
	}
#endif
	SetByte(addr&0xFFFFF,byte);
}

void DSP_InitDataWord(uint16_t addr,uint16_t word)		// Used only during initialisation (ignores mappings,ROM)
{
	DSP_POKE(addr,word);
}

void DSP_RAM_INIT()
{
	int a;

	const uint16_t SIN_TAB[256] = {
   0x0000, 0x0324, 0x0647, 0x096a, 0x0c8b, 0x0fab, 0x12c8, 0x15e2, 
   0x18f8, 0x1c0b, 0x1f19, 0x2223, 0x2528, 0x2826, 0x2b1f, 0x2e11,
   0x30fb, 0x33de, 0x36ba, 0x398c, 0x3c56, 0x3f17, 0x41ce, 0x447a, 
   0x471c, 0x49b4, 0x4c3f, 0x4ebf, 0x5133, 0x539b, 0x55f5, 0x5842,
   0x5a82, 0x5cb4, 0x5ed7, 0x60ec, 0x62f2, 0x64e8, 0x66cf, 0x68a6, 
   0x6a6d, 0x6c24, 0x6dca, 0x6f5f, 0x70e2, 0x7255, 0x73b5, 0x7504,
   0x7641, 0x776c, 0x7884, 0x798a, 0x7a7d, 0x7b5d, 0x7c29, 0x7ce3, 
   0x7d8a, 0x7e1d, 0x7e9d, 0x7f09, 0x7f62, 0x7fa7, 0x7fd8, 0x7ff6,
   0x7fff, 0x7ff6, 0x7fd8, 0x7fa7, 0x7f62, 0x7f09, 0x7e9d, 0x7e1d, 
   0x7d8a, 0x7ce3, 0x7c29, 0x7b5d, 0x7a7d, 0x798a, 0x7884, 0x776c,
   0x7641, 0x7504, 0x73b5, 0x7255, 0x70e2, 0x6f5f, 0x6dca, 0x6c24, 
   0x6a6d, 0x68a6, 0x66cf, 0x64e8, 0x62f2, 0x60ec, 0x5ed7, 0x5cb4,
   0x5a82, 0x5842, 0x55f5, 0x539b, 0x5133, 0x4ebf, 0x4c3f, 0x49b4, 
   0x471c, 0x447a, 0x41ce, 0x3f17, 0x3c56, 0x398c, 0x36ba, 0x33de,
   0x30fb, 0x2e11, 0x2b1f, 0x2826, 0x2528, 0x2223, 0x1f19, 0x1c0b, 
   0x18f8, 0x15e2, 0x12c8, 0x0fab, 0x0c8b, 0x096a, 0x0647, 0x0324,
   0x0000, 0xfcdc, 0xf9b9, 0xf696, 0xf375, 0xf055, 0xed38, 0xea1e, 
   0xe708, 0xe3f5, 0xe0e7, 0xdddd, 0xdad8, 0xd7da, 0xd4e1, 0xd1ef,
   0xcf05, 0xcc22, 0xc946, 0xc674, 0xc3aa, 0xc0e9, 0xbe32, 0xbb86, 
   0xb8e4, 0xb64c, 0xb3c1, 0xb141, 0xaecd, 0xac65, 0xaa0b, 0xa7be,
   0xa57e, 0xa34c, 0xa129, 0x9f14, 0x9d0e, 0x9b18, 0x9931, 0x975a, 
   0x9593, 0x93dc, 0x9236, 0x90a1, 0x8f1e, 0x8dab, 0x8c4b, 0x8afc,
   0x89bf, 0x8894, 0x877c, 0x8676, 0x8583, 0x84a3, 0x83d7, 0x831d, 
   0x8276, 0x81e3, 0x8163, 0x80f7, 0x809e, 0x8059, 0x8028, 0x800a,
   0x8000, 0x800a, 0x8028, 0x8059, 0x809e, 0x80f7, 0x8163, 0x81e3, 
   0x8276, 0x831d, 0x83d7, 0x84a3, 0x8583, 0x8676, 0x877c, 0x8894,
   0x89bf, 0x8afc, 0x8c4b, 0x8dab, 0x8f1e, 0x90a1, 0x9236, 0x93dc, 
   0x9593, 0x975a, 0x9931, 0x9b18, 0x9d0e, 0x9f14, 0xa129, 0xa34c,
   0xa57e, 0xa7be, 0xaa0b, 0xac65, 0xaecd, 0xb141, 0xb3c1, 0xb64c, 
   0xb8e4, 0xbb86, 0xbe32, 0xc0e9, 0xc3aa, 0xc674, 0xc946, 0xcc22,
   0xcf05, 0xd1ef, 0xd4e1, 0xd7da, 0xdad8, 0xdddd, 0xe0e7, 0xe3f5, 
   0xe708, 0xea1e, 0xed38, 0xf055, 0xf375, 0xf696, 0xf9b9, 0xfcdc,
};

	for (a=0;a<256;a++)
	{
		DSP_InitDataWord(a,SIN_TAB[a]);
	}

	DSP_InitDataWord(0x100,0x0000);		// Constants
	DSP_InitDataWord(0x101,0x0001);		// Constants
	DSP_InitDataWord(0x102,0x0002);		// Constants
	DSP_InitDataWord(0x103,0x0004);		// Constants
	DSP_InitDataWord(0x104,0x0008);		// Constants
	DSP_InitDataWord(0x105,0x0010);		// Constants
	DSP_InitDataWord(0x106,0x0020);		// Constants
	DSP_InitDataWord(0x107,0x0040);		// Constants
	DSP_InitDataWord(0x108,0x0080);		// Constants
	DSP_InitDataWord(0x109,0xFFFF);		// Constants
	DSP_InitDataWord(0x10A,0xFFFE);		// Constants
	DSP_InitDataWord(0x10B,0xFFFC);		// Constants
	DSP_InitDataWord(0x10C,0x8000);		// Constants

	DSP_STATUS=0;
}

void DSP_STEP(void);
void FL1DSP_STEP(void);

// DAC is signed on slip dsp
void DSP_SetDAC(uint8_t channels,int16_t value)
{
	// Bleep
	if (channels&1)
	{
		_AudioAddData(0,value>>2);			//(14 bit DAC)
	}
	if (channels&2)
	{
		_AudioAddData(1,value>>2);			//(14 bit DAC)
	}
}

uint16_t FL1DSP_DMAGetWord(uint32_t addr)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		CONSOLE_OUTPUT("DSP DMA HOST->DSP %05X\n",addr&0xFFFFE);
	}
#endif
	return GetByte(addr&0xFFFFE)|(GetByte((addr&0xFFFFE) +1)<<8);
}

void FL1DSP_DMASetWord(uint32_t addr,uint16_t word)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		CONSOLE_OUTPUT("DSP DMA DSP->HOST %05X (%04X)\n",addr&0xFFFFE,word);
	}
#endif
	SetByte(addr&0xFFFFE,word&0xFF);
	SetByte((addr&0xFFFFE)+1,word>>8);
}

uint8_t FL1DSP_DMAGetByte(uint32_t addr)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		CONSOLE_OUTPUT("DSP DMA HOST->DSP %05X\n",addr&0xFFFFF);
	}
#endif
	return GetByte(addr&0xFFFFF);
}

void FL1DSP_DMASetByte(uint32_t addr,uint8_t byte)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		CONSOLE_OUTPUT("DSP DMA DSP->HOST %05X (%02X)\n",addr&0xFFFFF,byte);
	}
#endif
	SetByte(addr&0xFFFFF,byte);
}

// dac is unsigned on flare1
void FL1DSP_SetDAC(uint8_t channels,uint16_t value)
{
	if (channels&1)
	{
		_AudioAddData(0,value>>2);			//(14 bit DAC)
	}
	if (channels&2)
	{
		_AudioAddData(1,value>>2);			//(14 bit DAC)
	}
}


#if ENABLE_DEBUG

extern uint8_t *DSP_DIS_[32];			// FROM EDL

extern uint16_t	DSP_DEBUG_PC;

void DSP_DUMP_REGISTERS()
{
	CONSOLE_OUTPUT("--------\n");
	CONSOLE_OUTPUT("FLAGS = C\n");
	CONSOLE_OUTPUT("        %s\n",	DSP_PEEK(0x147)&0x10?"1":"0");
	CONSOLE_OUTPUT("IX = %04X\n",DSP_PEEK(0x141));
	CONSOLE_OUTPUT("MZ0= %04X\n",DSP_PEEK(0x145));
	CONSOLE_OUTPUT("MZ1= %04X\n",DSP_PEEK(0x146));
	CONSOLE_OUTPUT("MZ2= %04X\n",DSP_PEEK(0x147)&0xF);
	CONSOLE_OUTPUT("MDE= %04X\n",DSP_PEEK(0x14B));
	CONSOLE_OUTPUT("X  = %04X\n",DSP_PEEK(0x14C));
	CONSOLE_OUTPUT("AZ = %04X\n",DSP_PEEK(0x14D));
	CONSOLE_OUTPUT("DMD= %04X\n",DSP_PEEK(0x144));
	uint32_t dma0=DSP_PEEK(0x142);
	uint32_t dma1=DSP_PEEK(0x143);
	CONSOLE_OUTPUT("DMA0= %04X\n",dma0);
	CONSOLE_OUTPUT("DMA1= %04X    (HOLD=%d)(RW=%d)(BW=%d)(DMA ADDR=%06X)\n",dma1,(dma1&0x800)>>11,(dma1&0x400)>>10,(dma1&0x200)>>9,((dma1&0xF)<<16)|(dma0));
	CONSOLE_OUTPUT("--------\n");
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
	uint16_t word=DSP_PEEK(0x400+address);
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
		CONSOLE_OUTPUT("UNKNOWN AT : %04X\n",address);
		CONSOLE_OUTPUT("%04X ",DSP_PEEK(0x400+address));
		CONSOLE_OUTPUT("\n");
		DSP_DUMP_REGISTERS();
		exit(-1);
	}

	if (registers)
	{
		DSP_DUMP_REGISTERS();
	}
	CONSOLE_OUTPUT("%04X :",address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see

	CONSOLE_OUTPUT("%04X ",DSP_PEEK(0x400+address));
	CONSOLE_OUTPUT("   ");
	CONSOLE_OUTPUT("%s\n",retVal);

	return 1;
}

#endif

#define RATE_ADJUST	(0)			//TODO this should be read from the MODE register and it should affect the DAC conversion speed not the DSP execution speed

#define FL1_RATE_ADJUST	(1)		//FL1 DSP runs at 6Mhz not 12Mhz

extern int emulateDSP;

void TickDSP()
{
#if !DISABLE_DSP
	static int iamslow=RATE_ADJUST;
	int running=(DSP_STATUS&0x10) && emulateDSP;

	if (running)
	{
		if (iamslow==0)
		{

#if ENABLE_DEBUG
		if (DSP_DEBUG_PC==0x56)
		{
			//doDSPDisassemble=1;
		}
		if (doDSPDisassemble)
		{
			DSP_Disassemble(DSP_DEBUG_PC,1);
			//exit(111);
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

void TickFL1DSP()
{
#if !DISABLE_DSP
	static int iamslow=FL1_RATE_ADJUST;
	int running=(DSP_STATUS&0x1) && emulateDSP;

	if (running)
	{
		if (iamslow==0)
		{
			FL1DSP_STEP();

			iamslow=FL1_RATE_ADJUST;
		}
		else
			iamslow--;
	}
#endif
}

void DSP_TranslateInstructionFL1(uint16_t addr,uint16_t pWord)
{
	uint16_t pAddr=(pWord&0x7FF)*2;		// bottom 11 bits - multiply 2 because word addresses make less sense to me at moment
	uint16_t pOpcode=(pWord&0xF800)>>11;		// top 5 bits?

	// Quick test

	switch (pOpcode)
	{
		case 0:
			CONSOLE_OUTPUT("  MOV (%04X),MZ0\n",pAddr);
			break;
		case 1:
			CONSOLE_OUTPUT("  MOV (%04X),MZ1\n",pAddr);
			break;
		case 2:
			CONSOLE_OUTPUT("  MOV MX,(%04X)\n",pAddr);
			break;
		case 3:
			CONSOLE_OUTPUT("  MOV MY,(%04X)\n",pAddr);
			break;
		case 4:
			CONSOLE_OUTPUT("  MOV (%04X),CMPR\n",pAddr);
			break;
		case 5:
			CONSOLE_OUTPUT("  MOV DMA0,(%04X)\n",pAddr);
			break;
		case 6:
			CONSOLE_OUTPUT("  MOV DMA1,(%04X)\n",pAddr);
			break;
		case 7:
			CONSOLE_OUTPUT("  MOV DMD,(%04X)\n",pAddr);
			break;
		case 8:
			CONSOLE_OUTPUT("  MOV (%04X),DMD\n",pAddr);
			break;
		case 9:
			CONSOLE_OUTPUT("  NOP (%04X)\n",pAddr);
			break;
		case 10:
			CONSOLE_OUTPUT("  MOV (%04X),INTRA\n",pAddr);
			break;
		case 11:
			CONSOLE_OUTPUT("  OFFSET (%04X)\n",pAddr);
			break;
		case 12:
			CONSOLE_OUTPUT("  MOV (%04X),PC\n",pAddr);
			break;
		case 13:
			CONSOLE_OUTPUT("  MOV AX,(%04X)\n",pAddr);
			break;
		case 14:
			CONSOLE_OUTPUT("  MOV (%04X),AX\n",pAddr);
			break;
		case 15:
			CONSOLE_OUTPUT("  MOV (%04X),AZ\n",pAddr);
			break;
		case 16:
			CONSOLE_OUTPUT("  ADD (%04X)\n",pAddr);
			break;
		case 17:
			CONSOLE_OUTPUT("  SUB (%04X)\n",pAddr);
			break;
		case 18:
			CONSOLE_OUTPUT("  AND (%04X)\n",pAddr);
			break;
		case 19:
			CONSOLE_OUTPUT("  OR (%04X)\n",pAddr);
			break;
		case 20:
			CONSOLE_OUTPUT("  ADC (%04X)\n",pAddr);
			break;
		case 21:
			CONSOLE_OUTPUT("  SBC (%04X)\n",pAddr);
			break;
		case 22:
			CONSOLE_OUTPUT("  ADDinC (%04X)\n",pAddr);
			break;
		case 23:
			CONSOLE_OUTPUT("  MOV AZ,(%04X)\n",pAddr);
			break;
		case 24:
			CONSOLE_OUTPUT("  MOV DAC0,(%04X)\n",pAddr);
			break;
		case 25:
			CONSOLE_OUTPUT("  MOV DAC1,(%04X)\n",pAddr);
			break;
		case 26:
			CONSOLE_OUTPUT("  MOV DAC2,(%04X)\n",pAddr);
			break;
		case 27:
			CONSOLE_OUTPUT("  MOV DAC12,(%04X)\n",pAddr);
			break;
		case 28:
			CONSOLE_OUTPUT("  NOPA (%04X)\n",pAddr);
			break;
		case 29:
			CONSOLE_OUTPUT("  MOV PC,(%04X)\n",pAddr);
			break;
		case 30:
			CONSOLE_OUTPUT("  NOPR (%04X)\n",pAddr);
			break;
		case 31:
			CONSOLE_OUTPUT("  INTRUDE\n");
			break;
	}
}
 
void DSP_TranslateInstruction(uint16_t addr,uint16_t pWord)
{
	uint16_t pAddr=(pWord&0x1FF)*2;		// bottom 9 bits - multiply 2 because word addresses make less sense to me at moment
	uint16_t pOpcode=(pWord&0xF800)>>11;		// top 5 bits?
	uint8_t isConditional=(pWord&0x0400)>>10;
	uint8_t isIndexed=(pWord&0x0200)>>9;

	// Quick test

	switch (pOpcode)
	{
		case 0:
			CONSOLE_OUTPUT("%s MOV (%04X%s),MZ0\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 1:
			CONSOLE_OUTPUT("%s MOV (%04X%s),MZ1\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 2:
			CONSOLE_OUTPUT("%s MOV MZ0,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 3:
			CONSOLE_OUTPUT("%s MOV MZ1,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 4:
			CONSOLE_OUTPUT("%s CCF\n",isConditional?"IF C THEN":"");
			break;
		case 5:
			CONSOLE_OUTPUT("%s MOV DMA0,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 6:
			CONSOLE_OUTPUT("%s MOV DMA1,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 7:
			CONSOLE_OUTPUT("%s MOV DMD,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 8:
			CONSOLE_OUTPUT("%s MOV (%04X%s),DMD\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 9:
			CONSOLE_OUTPUT("%s MAC (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 10:
			CONSOLE_OUTPUT("%s MOV MODE,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 11:
			CONSOLE_OUTPUT("%s MOV IX,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 12:
			CONSOLE_OUTPUT("%s MOV (%04X%s),PC\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 13:
			CONSOLE_OUTPUT("%s MOV X,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 14:
			CONSOLE_OUTPUT("%s MOV (%04X%s),X\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 15:
			CONSOLE_OUTPUT("%s MULT (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 16:
			CONSOLE_OUTPUT("%s ADD (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 17:
			CONSOLE_OUTPUT("%s SUB (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 18:
			CONSOLE_OUTPUT("%s AND (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 19:
			CONSOLE_OUTPUT("%s OR (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 20:
			CONSOLE_OUTPUT("%s ADC (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 21:
			CONSOLE_OUTPUT("%s SBC (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 22:
			CONSOLE_OUTPUT("%s MOV (%04X%s),AZ\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 23:
			CONSOLE_OUTPUT("%s MOV AZ,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 24:
			CONSOLE_OUTPUT("%s MOV (%04X%s),Z2\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 25:
			CONSOLE_OUTPUT("%s MOV DAC1,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 26:
			CONSOLE_OUTPUT("%s MOV DAC2,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 27:
			CONSOLE_OUTPUT("%s MOV DAC12,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 28:
			CONSOLE_OUTPUT("%s GAI (%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 29:
			CONSOLE_OUTPUT("%s MOV PC,(%04X%s)\n",isConditional?"IF C THEN":"",pAddr,isIndexed?"+IX":"");
			break;
		case 30:
			CONSOLE_OUTPUT("%s NOP\n",isConditional?"IF C THEN":"");
			break;
		case 31:
			CONSOLE_OUTPUT("%s INTRUDE\n",isConditional?"IF C THEN":"");
			break;
	}
}

void ASIC_HostDSPMemWriteMSU(uint16_t addr,uint8_t byte)
{
	if (addr>=0x800 && addr<0xE00 )
	{
#if ENABLE_DEBUG
		if (addr&1)
		{
			if (doShowHostDSPWrites)
			{
				uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
				CONSOLE_OUTPUT("Host Write To DSP Prog %04X <- %04X ",addr-1,pWord);
				DSP_TranslateInstruction(addr-1,pWord);
			}
		}
#endif
		DSP_POKE_BYTE(addr,byte);
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
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP Registers : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			DSP_POKE_BYTE(addr,byte);
		}
		if (addr>=0x200 && addr<0x280)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP Constants (ignored) : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			// Don't preform write (this is rom space)
		}
		if (addr<0x200)
		{
			if (DSP_PEEK_BYTE(0x14B*2) & 0x80)		// Alternate memory mapping enabled
			{
#if ENABLE_DEBUG
				if (addr&1)
				{
					if (doShowHostDSPWrites)
					{
						uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
						CONSOLE_OUTPUT("Host Write To DSP Data (Alternate Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
					}
				}
#endif
				DSP_POKE_BYTE(addr+0x300,byte);
			}
			else
			{
#if ENABLE_DEBUG
				if (addr&1)
				{
					if (doShowHostDSPWrites)
					{
						uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
						CONSOLE_OUTPUT("Host Write To DSP ROM (ignored) (Normal Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
					}
				}
#endif
				// Don't preform write (this is rom space)
			}
		}
		if (addr>=0x300 && addr<0x500)
		{
			if (DSP_PEEK_BYTE(0x14B*2) & 0x80)		// Alternate memory mapping enabled
			{
#if ENABLE_DEBUG
				if (addr&1)
				{
					if (doShowHostDSPWrites)
					{
						uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
						CONSOLE_OUTPUT("Host Write To DSP ROM (ignored) (Alternate Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
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
						uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
						CONSOLE_OUTPUT("Host Write To DSP Data (Normal Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
					}
				}
#endif
				DSP_POKE_BYTE(addr,byte);
			}
		}
		if (addr>=0x500 && addr<0x800)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP Data : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			DSP_POKE_BYTE(addr,byte);
		}
		if (addr>=0xE00)
		{
#if ENABLE_DEBUG
			if (doShowHostDSPWrites)
			{
				CONSOLE_OUTPUT("Host Write to DSP Data (Unknown (FF0 status!)) : %04X\n",addr);
			}
#endif
			if (addr==0xFF0)
			{
				DSP_STATUS&=0xFF00;
				DSP_STATUS|=byte;
				return;
			}
			if (addr==0xFF1)
			{
				DSP_STATUS&=0x00FF;
				DSP_STATUS|=byte<<8;
				return;
			}
			DSP_POKE_BYTE(addr,byte);
		}
	}
}

uint8_t ASIC_HostDSPMemReadMSU(uint16_t addr)
{
	if (addr>=0x800 && addr<0xE00)
	{
#if ENABLE_DEBUG
		if (doShowHostDSPReads)
		{
			CONSOLE_OUTPUT("Host DSP Prog Read (TODO deny when running) : %04X\n",addr);
		}
#endif
		return DSP_PEEK_BYTE(addr);
	}
	else
	{
		if (addr<0x800)
		{
			if (DSP_PEEK_BYTE(0x14B*2) & 0x80)		// Alternate memory mapping enabled
			{
#if ENABLE_DEBUG
				if (doShowHostDSPReads)
				{
					CONSOLE_OUTPUT("Host DSP Data Read (Alternate Map) : %04X\n",addr);
				}
#endif
				if (addr<0x200)
				{
					return DSP_PEEK_BYTE(addr+0x300);
				}
				if (addr>=0x300 && addr<0x500)
				{
					return DSP_PEEK_BYTE(addr-0x300);
				}
			}
			else
			{
#if ENABLE_DEBUG
				if (doShowHostDSPReads)
				{
					CONSOLE_OUTPUT("Host DSP Data Read (Normal Map) : %04X\n",addr);
				}
#endif
				return DSP_PEEK_BYTE(addr);
			}
		}
		else
		{
#if ENABLE_DEBUG
			if (doShowHostDSPReads)
			{
				CONSOLE_OUTPUT("Host DSP Data Read (Unknown (FF0 status!)) : %04X\n",addr);
			}
#endif
		}
	}
	if (addr==0xFF0)
	{
		return DSP_STATUS&0XFF;
	}
	if (addr==0xFF1)
	{
		return DSP_STATUS>>8;
	}
	return DSP_PEEK_BYTE(addr);
}

void ASIC_HostDSPMemWriteP89(uint16_t addr,uint8_t byte)
{
	if (addr>=0x400 && addr<0x600 )
	{
#if ENABLE_DEBUG
		if (addr&1)
		{
			if (doShowHostDSPWrites)
			{
				uint16_t pWord = DSP_PEEK_BYTE((addr+0x400)-1) | (byte<<8);
				CONSOLE_OUTPUT("Host Write To DSP Prog %04X <- %04X ",addr-1,pWord);
				DSP_TranslateInstruction(addr-1,pWord);
			}
		}
#endif
		DSP_POKE_BYTE(addr+0x400,byte);
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
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP Registers : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			DSP_POKE_BYTE(addr,byte);
		}
		if (addr>=0x200 && addr<0x280)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP Constants (ignored) : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			// Don't preform write (this is rom space)
		}
		if (addr<0x200)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP ROM (ignored) (Normal Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
		}
		if (addr>=0x300 && addr<0x400)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP Data (Normal Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			DSP_POKE_BYTE(addr,byte);
		}
		if (addr>=0x600)
		{
#if ENABLE_DEBUG
			if (doShowHostDSPWrites)
			{
				CONSOLE_OUTPUT("Host Write to DSP Data (Unknown (600 status!)) : %04X<-%02X\n",addr,byte);
			}
#endif
			if (addr==0x600)
			{
				DSP_STATUS&=0xFF00;
				DSP_STATUS|=byte;
				return;
			}
			if (addr==0x601)
			{
				DSP_STATUS&=0x00FF;
				DSP_STATUS|=byte<<8;
				return;
			}
			DSP_POKE_BYTE(addr,byte);
		}
	}
}



void ASIC_HostDSPMemWriteP88(uint16_t addr,uint8_t byte)
{
	if (addr>=0x400 && addr<0x600 )
	{
#if ENABLE_DEBUG
		if (addr&1)
		{
			if (doShowHostDSPWrites)
			{
				uint16_t pWord = DSP_PEEK_BYTE((addr+0x400)-1) | (byte<<8);
				CONSOLE_OUTPUT("Host Write To DSP Prog %04X <- %04X ",addr-1,pWord);
				DSP_TranslateInstruction(addr-1,pWord);
			}
		}
#endif
		DSP_POKE_BYTE(addr+0x400,byte);
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
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP Registers : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			DSP_POKE_BYTE(addr,byte);
		}
		if (addr>=0x200 && addr<0x280)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP Constants (ignored) : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			// Don't preform write (this is rom space)
		}
		if (addr<0x200)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP ROM (ignored) (Normal Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
		}
		if (addr>=0x300 && addr<0x400)
		{
#if ENABLE_DEBUG
			if (addr&1)
			{
				if (doShowHostDSPWrites)
				{
					uint16_t pWord = DSP_PEEK_BYTE(addr-1) | (byte<<8);
					CONSOLE_OUTPUT("Host Write To DSP Data (Normal Memory Mapping) : %04X <- %04X\n",addr-1,pWord);
				}
			}
#endif
			DSP_POKE_BYTE(addr,byte);
		}
		if (addr>=0x600)
		{
#if ENABLE_DEBUG
			if (doShowHostDSPWrites)
			{
				CONSOLE_OUTPUT("Host Write to DSP Data (Unknown (600 status!)) : %04X\n",addr);
			}
#endif
			if (addr==0x600)
			{
				DSP_STATUS&=0xFF00;
				DSP_STATUS|=byte;
				return;
			}
			if (addr==0x601)
			{
				DSP_STATUS&=0x00FF;
				DSP_STATUS|=byte<<8;
				return;
			}
			DSP_POKE_BYTE(addr,byte);
		}
	}
}

uint8_t ASIC_HostDSPMemReadP89(uint16_t addr)
{
	if (addr>=0x400 && addr<0x600)
	{
#if ENABLE_DEBUG
		if (doShowHostDSPReads)
		{
			CONSOLE_OUTPUT("Host DSP Prog Read (TODO deny when running) : %04X\n",addr);
		}
#endif
		return DSP_PEEK_BYTE(addr+0x400);
	}
	else
	{
		if (addr<0x400)
		{
#if ENABLE_DEBUG
			if (doShowHostDSPReads)
			{
				CONSOLE_OUTPUT("Host DSP Data Read (Normal Map) : %04X\n",addr);
			}
#endif
			return DSP_PEEK_BYTE(addr);
		}
		else
		{
#if ENABLE_DEBUG
			if (doShowHostDSPReads)
			{
				CONSOLE_OUTPUT("Host DSP Data Read (Unknown (600 status!)) : %04X\n",addr);
			}
#endif
		}
	}
	if (addr==0x600)
	{
		return DSP_STATUS&0XFF;
	}
	if (addr==0x601)
	{
		return DSP_STATUS>>8;
	}

	return DSP_PEEK_BYTE(addr);
}

uint8_t ASIC_HostDSPMemReadP88(uint16_t addr)
{
	if (addr>=0x400 && addr<0x600)
	{
#if ENABLE_DEBUG
		if (doShowHostDSPReads)
		{
			CONSOLE_OUTPUT("Host DSP Prog Read (TODO deny when running) : %04X\n",addr);
		}
#endif
		return DSP_PEEK_BYTE(addr+0x400);
	}
	else
	{
		if (addr<0x400)
		{
#if ENABLE_DEBUG
			if (doShowHostDSPReads)
			{
				CONSOLE_OUTPUT("Host DSP Data Read (Normal Map) : %04X\n",addr);
			}
#endif
			return DSP_PEEK_BYTE(addr);
		}
		else
		{
#if ENABLE_DEBUG
			if (doShowHostDSPReads)
			{
				CONSOLE_OUTPUT("Host DSP Data Read (Unknown (600 status!)) : %04X\n",addr);
			}
#endif
		}
	}
	if (addr==0x600)
	{
		DSP_INTRUDE_HACK ^= 0x04;
		return (DSP_STATUS&0XFF) | DSP_INTRUDE_HACK;
	}
	if (addr==0x601)
	{
		return DSP_STATUS>>8;
	}

	return DSP_PEEK_BYTE(addr);
}
