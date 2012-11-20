/*

	DSP memory handling

	Early revision of the DSP has an almost completely different memory map :(

*/

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <stdint.h>
#include <string.h>

#include "dsp.h"
#include "audio.h"
#include "system.h"


uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);

int doDSPDisassemble=0;
int doShowHostDSPWrites=0;
int doShowHostDSPReads=0;
int doShowDMA=0;

unsigned char DSP[4*1024];

uint16_t DSP_GetProgWord(uint16_t addr)
{
	addr&=0x1FF;		// 9 bits
	addr*=2;		// byte style addressing
	
	switch (curSystem)
	{
		case ESS_P88:
			return DSP[0x400 + addr + 0] | (DSP[0x400 + addr + 1]<<8);
		case ESS_MSU:
			return DSP[0x800 + addr + 0] | (DSP[0x800 + addr + 1]<<8);
	}

	return 0;
}

int doDSPDisassemble;

uint16_t DSP_GetDataWord(uint16_t addr)
{
	// I think the memory map was slightly configurable based on comments in sources, for now make Data start at 0
	addr&=0x1FF;		// 9 bits
	addr*=2;		// byte style addressing
	
	if (addr<0x200)
	{
		if (DSP[0x14B*2] & 0x80)		// Alternate memory mapping enabled
		{
#if ENABLE_DEBUG
			if (doDSPDisassemble)
			{
				printf("Reading from Alternate : %04X -> %04X\n",addr,DSP[0x300 + addr + 0] | (DSP[0x300 + addr + 1]<<8));
			}
#endif
			return DSP[0x300 + addr + 0] | (DSP[0x300 + addr + 1]<<8);
		}
		else
		{
#if ENABLE_DEBUG
			if (doDSPDisassemble)
			{
				printf("Reading from Rom : %04X -> %04X\n",addr,DSP[0x000 + addr + 0] | (DSP[0x000 + addr + 1]<<8));
			}
#endif
		}
	}
	if (addr>0x300 && addr<0x500)
	{
		if (DSP[0x14B*2] & 0x80)		// Alternate memory mapping enabled
		{
#if ENABLE_DEBUG
			if (doDSPDisassemble)
			{
				printf("Reading from Alternate : %04X -> %04X\n",addr,DSP[(addr-0x300) + 0] | (DSP[(addr-0x300) + 1]<<8));
			}
#endif
			return DSP[(addr-0x300) + 0] | (DSP[(addr-0x300) + 1]<<8);
		}
		else
		{
#if ENABLE_DEBUG
			if (doDSPDisassemble)
			{
				printf("Reading from Normal : %04X -> %04X\n",addr,DSP[0x000 + addr + 0] | (DSP[0x000 + addr + 1]<<8));
			}
#endif
		}
	}

	return DSP[0x000 + addr + 0] | (DSP[0x000 + addr + 1]<<8);
}

uint16_t DSP_DMAGetWord(uint32_t addr)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		printf("DSP DMA HOST->DSP %05X\n",addr&0xFFFFE);
	}
#endif
	return GetByte(addr&0xFFFFE)|(GetByte((addr&0xFFFFE) +1)<<8);
}

void DSP_DMASetWord(uint32_t addr,uint16_t word)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		printf("DSP DMA DSP->HOST %05X (%04X)\n",addr&0xFFFFE,word);
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
		printf("DSP DMA HOST->DSP %05X\n",addr&0xFFFFF);
	}
#endif
	return GetByte(addr&0xFFFFF);
}

void DSP_DMASetByte(uint32_t addr,uint8_t byte)
{
#if ENABLE_DEBUG
	if (doShowDMA)
	{
		printf("DSP DMA DSP->HOST %05X (%02X)\n",addr&0xFFFFF,byte);
	}
#endif
	SetByte(addr&0xFFFFF,byte);
}


void DSP_SetDataWord(uint16_t addr,uint16_t word)
{
	// I think the memory map was slightly configurable based on comments in sources, for now make Data start at 0
	addr&=0x1FF;		// 9 bits
	addr*=2;		// byte style addressing
	
	if (addr<0x200)
	{
		if (DSP[0x14B*2] & 0x80)		// Alternate memory mapping enabled
		{
#if ENABLE_DEBUG
			if (doDSPDisassemble)
			{
				printf("Writing to Alternate : %04X <- %04X\n",addr,word);
			}
#endif
			DSP[0x300 + addr + 0]=word&0xFF;
			DSP[0x300 + addr + 1]=word>>8;
		}
		else
		{
#if ENABLE_DEBUG
			if (doDSPDisassemble)
			{
				printf("Writing to ROM (ignored)!! : %04X <- %04X\n",addr,word);
			}
#endif
			return;
		}
	}
	if (addr>0x300 && addr<0x500)
	{
		if (DSP[0x14B*2] & 0x80)		// Alternate memory mapping enabled
		{
#if ENABLE_DEBUG
			if (doDSPDisassemble)
			{
				printf("Writing to Alternate : %04X <- %04X\n",addr,word);
			}
#endif
			DSP[(addr - 0x300) + 0]=word&0xFF;
			DSP[(addr - 0x300) + 1]=word>>8;
		}
		else
		{
#if ENABLE_DEBUG
			if (doDSPDisassemble)
			{
				printf("Writing to Normal : %04X <- %04X\n",addr,word);
			}
#endif
		}
	}
	DSP[0x000 + addr + 0]=word&0xFF;
	DSP[0x000 + addr + 1]=word>>8;
}

void DSP_InitDataWord(uint16_t addr,uint16_t word)		// Used only during initialisation (ignores mappings,ROM)
{
	addr&=0x1FF;		// 9 bits
	addr*=2;		// byte style addressing
	DSP[0x000 + addr + 0]=word&0xFF;
	DSP[0x000 + addr + 1]=word>>8;
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
}



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
extern uint16_t	DSP_DMD;
	
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
	printf("DMD= %04X\n",DSP_DMD);
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

#define RATE_ADJUST	(1)			//TODO this should be read from the MODE register and it should affect the DAC conversion speed not the DSP execution speed

void TickDSP()
{
#if !DISABLE_DSP
	static int iamslow=RATE_ADJUST;
	static int running=0;
	
	switch (curSystem)
	{
		case ESS_MSU:
			running=DSP[0xFF0]&0x10;
			break;
		case ESS_P88:
			running=DSP[0x600]&0x10;
			iamslow=0;
			break;
	}
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
//			exit(111);
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

void ASIC_HostDSPMemWriteMSU(uint16_t addr,uint8_t byte)
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

uint8_t ASIC_HostDSPMemReadMSU(uint16_t addr)
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

void ASIC_HostDSPMemWriteP88(uint16_t addr,uint8_t byte)
{
	if (addr>=0x400 && addr<0x600 )
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
		}
		if (addr>=0x300 && addr<0x400)
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
		if (addr>=0x600)
		{
#if ENABLE_DEBUG
			if (doShowHostDSPWrites)
			{
				printf("Host Write to DSP Data (Unknown (600 status!)) : %04X\n",addr);
			}
#endif
			DSP[addr]=byte;
		}
	}
}

uint8_t ASIC_HostDSPMemReadP88(uint16_t addr)
{
	if (addr>=0x400 && addr<0x600)
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
		if (addr<0x400)
		{
#if ENABLE_DEBUG
			if (doShowHostDSPReads)
			{
				printf("Host DSP Data Read (Normal Map) : %04X\n",addr);
			}
#endif
			return DSP[addr];
		}
		else
		{
#if ENABLE_DEBUG
			if (doShowHostDSPReads)
			{
				printf("Host DSP Data Read (Unknown (600 status!)) : %04X\n",addr);
			}
#endif
		}
	}
	return DSP[addr];
}
