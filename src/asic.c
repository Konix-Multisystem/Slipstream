/*

	ASIC test

	Currently contains some REGISTERS and some video hardware - will move to EDL eventually

	Need to break this up some more, blitter going here temporarily
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

int doShowBlits=0;
int doShowHostDSPWrites=0;
int doShowHostDSPReads=0;

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
uint8_t		ASIC_BLTCMD=0;
uint32_t	ASIC_BLTPC=0;				// 20 bit address

uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);

void TickBlitter()
{
	// Step one, make the blitter "free"

	if (ASIC_BLTCMD & 1)
	{
		int a,b;
		uint8_t BLT_OUTER_CMD=ASIC_BLTCMD;		// First time through we don't read the command		-- Note the order of data appears to differ from the docs!!
		uint32_t BLT_OUTER_SRC;
		uint32_t BLT_OUTER_DST;
		uint8_t BLT_OUTER_MODE;
		uint8_t BLT_OUTER_CPLG;
		uint8_t BLT_OUTER_CNT;
		uint16_t BLT_INNER_CNT;
		uint8_t BLT_INNER_STEP;
		uint8_t BLT_INNER_PAT;


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
		BLT_OUTER_SRC|=(GetByte(ASIC_BLTPC)&0xF)<<16;		// TODO flags
		ASIC_BLTPC++;
		

		BLT_OUTER_CNT=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;


		BLT_OUTER_DST=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		BLT_OUTER_DST|=GetByte(ASIC_BLTPC)<<8;
		ASIC_BLTPC++;
		BLT_OUTER_DST|=(GetByte(ASIC_BLTPC)&0xF)<<16;		// TODO flags
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
		for (a=0;a<BLT_OUTER_CNT;a++)
		{
			uint8_t tmp=0;

			for (b=0;b<BLT_OUTER_CNT;b++)
			{
				if ((BLT_OUTER_CMD&0x80) && b==0)
				{
					tmp=GetByte(BLT_OUTER_SRC);
					BLT_OUTER_SRC++;
				}

				// hard coded test
				if (BLT_OUTER_MODE&0x80)
				{
					if (BLT_OUTER_MODE&0x04)
					{
						if (tmp& (1<<b))
						{
							SetByte(BLT_OUTER_DST,BLT_INNER_PAT);
						}
					}
					else
					{
						SetByte(BLT_OUTER_DST,BLT_INNER_PAT);
					}
				}
//				else
//				{
//					tmp=GetByte(BLT_OUTER_SRC);
//					BLT_OUTER_SRC++;
//					SetByte(BLT_OUTER_DST,tmp);
//				}

				BLT_OUTER_DST++;
			}

			BLT_OUTER_DST+=BLT_INNER_STEP;
		}
		
		ASIC_BLTPC++;		// skip segment address
		BLT_OUTER_CMD=GetByte(ASIC_BLTPC);
		ASIC_BLTPC++;
		}
		while (BLT_OUTER_CMD&1);

//		exit(1);

	}
}



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
			TickBlitter();
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
	if (DSP[0xFF0]&0x10)
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
