/*
 * Slipstream emulator
 *
 * expects MSU files
 *
 * Currently 8086 cpu
 *
 * Assuming PAL (was european after all)
 */

#include <GL/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "video.h"
#include "audio.h"
#include "keys.h"
#include "asic.h"

#define SEGTOPHYS(seg,off)	( ((seg)<<4) + (off) )				// Convert Segment,offset pair to physical address

unsigned char RAM[256*1024];
unsigned char DSP[4*1024];
unsigned char PALETTE[256*2];

uint16_t DSP_GetProgWord(uint16_t addr)
{
	addr&=0x1FF;		// 9 bits
	addr*=2;		// byte style addressing
	
	return DSP[0x800 + addr + 0] | (DSP[0x800 + addr + 1]<<8);
}

void DSP_SetProgWord(uint16_t addr,uint16_t word)
{
	if (addr<4*1024-1)
	{
		DSP[addr]=word&0xFF;
		DSP[addr+1]=word>>8;
		return;
	}
#if ENABLE_DEBUG
	printf("Out of Bounds Write from DSP %04X<-%04X\n",addr,word);
	exit(1);
#endif
}

uint16_t DSP_GetDataWord(uint16_t addr)
{
	// I think the memory map was slightly configurable based on comments in sources, for now make Data start at 0
	addr&=0x1FF;		// 9 bits
	addr*=2;		// byte style addressing
	
	return DSP[0x000 + addr + 0] | (DSP[0x000 + addr + 1]<<8);
}

void DSP_SetDataWord(uint16_t addr,uint16_t word)
{
	// I think the memory map was slightly configurable based on comments in sources, for now make Data start at 0
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
		DSP_SetDataWord(a,SIN_TAB[a]);
	}

	DSP_SetDataWord(0x100,0x0000);		// Constants
	DSP_SetDataWord(0x101,0x0001);		// Constants
	DSP_SetDataWord(0x102,0x0002);		// Constants
	DSP_SetDataWord(0x103,0x0004);		// Constants
	DSP_SetDataWord(0x104,0x0008);		// Constants
	DSP_SetDataWord(0x105,0x0010);		// Constants
	DSP_SetDataWord(0x106,0x0020);		// Constants
	DSP_SetDataWord(0x107,0x0040);		// Constants
	DSP_SetDataWord(0x108,0x0080);		// Constants
	DSP_SetDataWord(0x109,0xFFFF);		// Constants
	DSP_SetDataWord(0x10A,0xFFFE);		// Constants
	DSP_SetDataWord(0x10B,0xFFFC);		// Constants
	DSP_SetDataWord(0x10C,0x8000);		// Constants
}



uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);
uint8_t GetPortB(uint16_t port);
void SetPortB(uint16_t port,uint8_t byte);
uint16_t GetPortW(uint16_t port);
void SetPortW(uint16_t port,uint16_t word);

int masterClock=0;

extern uint8_t *DIS_[256];			// FROM EDL
extern uint8_t *DIS_XX00000010[256];			// FROM EDL
extern uint8_t *DIS_XX00000011[256];			// FROM EDL
extern uint8_t *DIS_XX00001011[256];			// FROM EDL
extern uint8_t *DIS_XX00110011[256];			// FROM EDL
extern uint8_t *DIS_XX00111011[256];			// FROM EDL
extern uint8_t *DIS_XX10000000[256];			// FROM EDL
extern uint8_t *DIS_XX10000001[256];			// FROM EDL
extern uint8_t *DIS_XX10000011[256];			// FROM EDL
extern uint8_t *DIS_XX10000110[256];			// FROM EDL
extern uint8_t *DIS_XX10001001[256];			// FROM EDL
extern uint8_t *DIS_XX10001010[256];			// FROM EDL
extern uint8_t *DIS_XX10001011[256];			// FROM EDL
extern uint8_t *DIS_XX10001100[256];			// FROM EDL
extern uint8_t *DIS_XX10001110[256];			// FROM EDL
extern uint8_t *DIS_XX11000111[256];			// FROM EDL
extern uint8_t *DIS_XX11010000[256];			// FROM EDL
extern uint8_t *DIS_XX11010001[256];			// FROM EDL
extern uint8_t *DIS_XX11010011[256];			// FROM EDL
extern uint8_t *DIS_XX11110110[256];			// FROM EDL
extern uint8_t *DIS_XX11111110[256];			// FROM EDL
extern uint8_t *DIS_XX11111111[256];			// FROM EDL

extern uint16_t	AX;
extern uint16_t	BX;
extern uint16_t	CX;
extern uint16_t	DX;
extern uint16_t	SP;
extern uint16_t	BP;
extern uint16_t	SI;
extern uint16_t	DI;
extern uint16_t	CS;
extern uint16_t	DS;
extern uint16_t	ES;
extern uint16_t	SS;
extern uint16_t	IP;
extern uint16_t FLAGS;

int doDebug=0;
int doShowPortStuff=0;
uint32_t doDebugTrapWriteAt=0xFFFFF;
int debugWatchWrites=0;
int debugWatchReads=0;

int HandleLoadSection(FILE* inFile)
{
	uint16_t	segment,offset;
	uint16_t	size;
	int		a=0;
	uint8_t		byte;

	if (2!=fread(&segment,1,2,inFile))
	{
		printf("Failed to read segment for LoadSection\n");
		exit(1);
	}
	if (2!=fread(&offset,1,2,inFile))
	{
		printf("Failed to read offset for LoadSection\n");
		exit(1);
	}
	fseek(inFile,2,SEEK_CUR);		// skip unknown
	if (2!=fread(&size,1,2,inFile))
	{
		printf("Failed to read size for LoadSection\n");
		exit(1);
	}

	printf("Found Section Load Memory : %04X:%04X   (%08X bytes)\n",segment,offset,size);

	for (a=0;a<size;a++)
	{
		if (1!=fread(&byte,1,1,inFile))
		{
			printf("Failed to read data from LoadSection\n");
			exit(1);
		}
		SetByte(a+SEGTOPHYS(segment,offset),byte);
	}

	return 8+size;
}

int HandleExecuteSection(FILE* inFile)
{
	uint16_t	segment,offset;
	
	if (2!=fread(&segment,1,2,inFile))
	{
		printf("Failed to read segment for ExecuteSection\n");
		exit(1);
	}
	if (2!=fread(&offset,1,2,inFile))
	{
		printf("Failed to read offset for ExecuteSection\n");
		exit(1);
	}

	CS=segment;
	IP=offset;

	printf("Found Section Execute : %04X:%04X\n",segment,offset);

	return 4;
}

int LoadMSU(const char* fname)					// Load an MSU file which will fill some memory regions and give us our booting point
{
	unsigned int expectedSize=0;
	FILE* inFile = fopen(fname,"rb");
	fseek(inFile,0,SEEK_END);
	expectedSize=ftell(inFile);
	fseek(inFile,0,SEEK_SET);

	while (expectedSize)
	{
		unsigned char sectionType;

		// Read a byte
		if (1!=fread(&sectionType,1,1,inFile))
		{
			printf("Failed to read section header\n");
			return 1;
		}
		expectedSize--;

		switch (sectionType)
		{
			case 0xC8:
				expectedSize-=HandleLoadSection(inFile);
				break;
			case 0xCA:
				expectedSize-=HandleExecuteSection(inFile);
				break;
			default:
				printf("Unknown section type @%ld : %02X\n",ftell(inFile)-1,sectionType);
				return 1;
		}
	}

	fclose(inFile);

	return 0;
}

uint8_t GetByte(uint32_t addr)
{
	addr&=0xFFFFF;
#if ENABLE_DEBUG
	if (debugWatchReads)
	{
		printf("Reading from address : %05X->\n",addr);
	}
#endif
	if (addr<128*1024)
	{
		return RAM[addr];
	}
	if (addr>=0xC1000 && addr<=0xC1FFF)
	{
		return DSP[addr-0xC1000];
	}
#if ENABLE_DEBUG
	printf("GetByte : %05X - TODO\n",addr);
#endif
	return 0xAA;
}

uint8_t PeekByte(uint32_t addr)
{
#if ENABLE_DEBUG
	uint8_t ret;
	int tmp=debugWatchReads;
	debugWatchReads=0;
	ret=GetByte(addr);
	debugWatchReads=tmp;
	return ret;
#else
	return GetByte(addr);
#endif
}

void SetByte(uint32_t addr,uint8_t byte)
{
	addr&=0xFFFFF;
#if ENABLE_DEBUG
	if (addr==doDebugTrapWriteAt)
	{
		printf("STOMP STOMP STOMP\n");
	}
	if (debugWatchWrites)
	{
		printf("Writing to address : %05X<-%02X\n",addr,byte);
	}
#endif
	if (addr<128*1024)
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
#if ENABLE_DEBUG
		if ((addr-0xC1000)>=0x800 && (addr-0xC1000)<0xE00 )
		{
			if (addr&1)
			{
				uint16_t pWord = DSP[addr-0xC1001] | (byte<<8);
				uint16_t pAddr=(pWord&0x1FF)*2;		// bottom 9 bits - multiply 2 because word addresses make less sense to me at moment
				uint16_t pOpcode=(pWord&0xF800)>>11;		// top 5 bits?
				uint8_t isConditional=(pWord&0x0400)>>10;
				uint8_t isIndexed=(pWord&0x0200)>>9;

				printf("Program Dump %04X  ",pWord);
				
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
		else
		{
			if ((addr-0xC1000)>=0x300 && (addr-0xC1000)<0x800 )
			{
				if (addr&1)
				{
					uint16_t pWord = DSP[addr-0xC1001] | (byte<<8);
					printf("Data Dump : %04X <- %04X\n",addr-0xC1001,pWord);
				}
			}
			else
			{
				printf("Write To DSP : offset : %04X <- %02X\n",addr-0xC1000,byte);
			}
		}
#endif
		DSP[addr-0xC1000]=byte;
		return;
	}
#if ENABLE_DEBUG
	printf("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
#endif
}

void DebugWPort(uint16_t port)
{
#if ENABLE_DEBUG
	switch (port)
	{
		case 0x0000:
			printf("KINT ??? Vertical line interrupt location (Word address)\n");
			break;
		case 0x0004:
			printf("STARTL - screen line start (Byte address)\n");
			break;
		case 0x0010:
			printf("SCROLL1 - TL pixel address LSB (Word address) - byte width\n");
			break;
		case 0x0012:
			printf("SCROLL2 - TL pixel address middle byte (Word address) - byte width\n");
			break;
		case 0x0014:
			printf("SCROLL3 - TL pixel address MSB (Word address) - byte width\n");
			break;
		case 0x0016:
			printf("ACK - interrupt acknowledge (Byte address)\n");
			break;
		case 0x0018:
			printf("MODE - screen mode (Byte address)\n");
			break;
		case 0x001A:
			printf("BORD - border colour (Word address)  - Little Endian if matching V1\n");
			break;
		case 0x001E:
			printf("PMASK - palette mask? (Word address) - only a byte documented\n");
			break;
		case 0x0020:
			printf("INDEX - palette index (Word address) - only a byte documented\n");
			break;
		case 0x0022:
			printf("ENDL - screen line end (Byte address)\n");
			break;
		case 0x0026:
			printf("MEM - memory configuration (Byte address)\n");
			break;
		case 0x002A:
			printf("DIAG - diagnostics (Byte address)\n");
			break;
		case 0x002C:
			printf("DIS - disable interupts (Byte address)\n");
			break;
		case 0x0044:
			printf("BLTCON - blitter control (Word address) - only a byte documented, but perhaps step follows?\n");
			break;
		case 0x00C0:
			printf("ADP - (Word address) - Anologue/digital port reset?\n");
			break;
		case 0x00E0:
			printf("???? - (Byte address) - number pad reset?\n");
			break;
		default:
			printf("PORT WRITE UNKNOWN - TODO\n");
			exit(-1);
			break;
	}
#endif
}

void DebugRPort(uint16_t port)
{
#if ENABLE_DEBUG
	switch (port)
	{
		case 0x0C:
			printf("???? - (Byte Address) - controller buttons...\n");
			break;
		case 0x80:
			printf("???? - (Word Address) - Possibly controller digital button status\n");
			break;
		case 0xC0:
			printf("ADP - (Word Address) - Analogue/digital port status ? \n");
			break;
		case 0xE0:
			printf("???? - (Byte Address) - Numberic pad read ? \n");
			break;
		default:
			printf("PORT READ UNKNOWN - TODO\n");
			exit(-1);
			break;
	}
#endif
}

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

uint8_t GetPortB(uint16_t port)
{
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
				printf("Warning unknown numPadRowSelectValue : %02X\n",numPadRowSelect);
#endif
				return 0xFF;
		}
	}

#if ENABLE_DEBUG
	if (doShowPortStuff)
	{
		printf("GetPortB : %04X - TODO\n",port);
		DebugRPort(port);
	}
#endif
	return 0x00;
}


void SetPortB(uint16_t port,uint8_t byte)
{
	switch (port)
	{
		case 0xC0:
			ADPSelect=byte;
			break;
		case 0xE0:
			numPadRowSelect=byte;
			break;
		default:
			ASIC_Write(port,byte,doShowPortStuff);
			break;
	}
#if ENABLE_DEBUG
	if (doShowPortStuff)
	{
		DebugWPort(port);
	}
#endif
}

uint16_t GetPortW(uint16_t port)
{
#if ENABLE_DEBUG
	if (doShowPortStuff)
	{
		printf("GetPortW : %04X - TODO\n",port);
		DebugRPort(port);
	}
#endif
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
	return 0x0000;
}

void SetPortW(uint16_t port,uint16_t word)
{
	ASIC_Write(port,word&0xFF,doShowPortStuff);
	ASIC_Write(port+1,word>>8,doShowPortStuff);
	if (port==0xC0)
	{
		ADPSelect=word&0xFF;
	}
#if ENABLE_DEBUG
	if (doShowPortStuff)
	{
		DebugWPort(port);
	}
#endif
}

void TickKeyboard()
{
	int a;
	static const int keyToJoy_KB[16]={	0,0,GLFW_KEY_KP_1,GLFW_KEY_KP_3,GLFW_KEY_KP_4,GLFW_KEY_KP_6,GLFW_KEY_KP_8,GLFW_KEY_KP_2,		// Joystick 2
						0,0,GLFW_KEY_Z,GLFW_KEY_X,GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_UP,GLFW_KEY_DOWN};			// Joystick 1
	static const int keyToJoy_JY[16]={	0,0,GLFW_KEY_KP_1,GLFW_KEY_KP_3,GLFW_KEY_KP_4,GLFW_KEY_KP_6,GLFW_KEY_KP_8,GLFW_KEY_KP_2,		// Joystick 2
						0,0,0x10000002,0x10000001,0x1000000D,0x1000000B,0x1000000A,0x1000000C};				// Joystick 1 - Mapped to joysticks (hence special numbers)
	const int* keyToJoy=keyToJoy_KB;
	if (JoystickPresent())
	{
		keyToJoy=keyToJoy_JY;
	}

	for (a=0;a<16;a++)
	{
		if (KeyDown(GLFW_KEY_F1+a))
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

#if ENABLE_DEBUG
void DUMP_REGISTERS()
{
	printf("--------\n");
	printf("FLAGS = O  D  I  T  S  Z  -  A  -  P  -  C\n");
	printf("        %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s\n",
			FLAGS&0x800 ? "1" : "0",
			FLAGS&0x400 ? "1" : "0",
			FLAGS&0x200 ? "1" : "0",
			FLAGS&0x100 ? "1" : "0",
			FLAGS&0x080 ? "1" : "0",
			FLAGS&0x040 ? "1" : "0",
			FLAGS&0x020 ? "1" : "0",
			FLAGS&0x010 ? "1" : "0",
			FLAGS&0x008 ? "1" : "0",
			FLAGS&0x004 ? "1" : "0",
			FLAGS&0x002 ? "1" : "0",
			FLAGS&0x001 ? "1" : "0");
	printf("AX= %04X\n",AX);
	printf("BX= %04X\n",BX);
	printf("CX= %04X\n",CX);
	printf("DX= %04X\n",DX);
	printf("SP= %04X\n",SP);
	printf("BP= %04X\n",BP);
	printf("SI= %04X\n",SI);
	printf("DI= %04X\n",DI);
	printf("CS= %04X\n",CS);
	printf("DS= %04X\n",DS);
	printf("ES= %04X\n",ES);
	printf("SS= %04X\n",SS);
	printf("--------\n");
}

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength)
{
	static char segOveride[2048];
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

	uint8_t byte = PeekByte(address);
	if (byte>realLength)
	{
		sprintf(temporaryBuffer,"UNKNOWN OPCODE");
		return temporaryBuffer;
	}
	else
	{
		const char* mnemonic=(char*)table[byte];
		const char* sPtr=mnemonic;
		char* dPtr=temporaryBuffer;
		int counting = 0;
		int doingDecode=0;

		if (sPtr==NULL)
		{
			sprintf(temporaryBuffer,"UNKNOWN OPCODE");
			return temporaryBuffer;
		}
	
		if (strncmp(mnemonic,"XX001__110",10)==0)				// Segment override
		{
			
			int tmpCount=0;
			decodeDisasm(DIS_,address+1,&tmpCount,256);
			*count=tmpCount+1;
			strcpy(segOveride,mnemonic+10);
			strcat(segOveride,temporaryBuffer);
			return segOveride;
		}
		if (strcmp(mnemonic,"XX00000010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00000010,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00000011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00000011,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00001011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00001011,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00111011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00111011,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10000000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10000000,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10000001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10000001,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10000011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10000011,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10000110")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10000110,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001001,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001010,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001011,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001100")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001100,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001110")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001110,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11000111")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11000111,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11010000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11010000,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11010001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11010001,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11010011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11010011,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11110110")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11110110,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11111110")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11111110,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11111111")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11111111,address+1,&tmpCount,256);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00110011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00110011,address+1,&tmpCount,256);
			*count=tmpCount+1;
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
				int offset=(*sPtr-'0')*negOffs;
				sprintf(sprintBuffer,"%02X",PeekByte(address+offset));
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
		*count=counting;
	}
	return temporaryBuffer;
}

int Disassemble(unsigned int address,int registers)
{
	int a;
	int numBytes=0;
	const char* retVal = decodeDisasm(DIS_,address,&numBytes,256);

	if (strcmp(retVal,"UNKNOWN OPCODE")==0)
	{
		printf("UNKNOWN AT : %05X\n",address);		// TODO this will fail to wrap which may show up bugs that the CPU won't see
		for (a=0;a<numBytes+1;a++)
		{
			printf("%02X ",PeekByte(address+a));
		}
		printf("\n");
		DUMP_REGISTERS();
		exit(-1);
	}

	if (registers)
	{
		DUMP_REGISTERS();
	}
	printf("%05X :",address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see

	for (a=0;a<numBytes+1;a++)
	{
		printf("%02X ",PeekByte(address+a));
	}
	for (a=0;a<8-(numBytes+1);a++)
	{
		printf("   ");
	}
	printf("%s\n",retVal);

	return numBytes+1;
}

void DisassembleRange(unsigned int start,unsigned int end)
{
	while (start<end)
	{
		start+=Disassemble(start,0);
	}
}	
#endif

void DSP_RESET(void);
void STEP(void);
void RESET(void);

extern uint16_t	PC;
extern uint8_t CYCLES;

void CPU_RESET()
{
	RESET();
}

int CPU_STEP(int doDebug)
{
#if ENABLE_DEBUG
	if (doDebug)
	{
		Disassemble(SEGTOPHYS(CS,IP),1);
	}
#endif
	STEP();

	return CYCLES;
}
	
int main(int argc,char**argv)
{
	int numClocks;

	if (argc!=2)
	{
		printf("slipstream  program.msu\n");
		return 1;
	}
	
	CPU_RESET();
	DSP_RESET();

	DSP_RAM_INIT();

	if (LoadMSU(argv[1]))
		return 1;

	VideoInitialise(WIDTH,HEIGHT,"Slipstream - V0.001");
	KeysIntialise();
	AudioInitialise(WIDTH*HEIGHT);

	//////////////////
	
//	DisassembleRange(0x0000,0x4000);

//	doDebugTrapWriteAt=0x0827B;
//	debugWatchWrites=1;

	while (1==1)
	{
#if ENABLE_DEBUG
//		if (SEGTOPHYS(CS,IP)==0x08107)
//		{
//			doDebug=1;
//			debugWatchWrites=1;
//			debugWatchReads=1;
//			numClocks=1;
//		}
#endif
		numClocks=CPU_STEP(doDebug);
		TickAsic(numClocks);
		masterClock+=numClocks;

		AudioUpdate(numClocks);

		if (masterClock>=WIDTH*HEIGHT)
		{	
			masterClock-=WIDTH*HEIGHT;

			TickKeyboard();
			JoystickPoll();
			VideoUpdate();

			if (CheckKey(GLFW_KEY_ESC))
			{
				break;
			}
			if (CheckKey(GLFW_KEY_END))
			{
//				doDebug=1;
				ClearKey(GLFW_KEY_END);
			}

			VideoWait();
		}
	}

	KeysKill();
	AudioKill();
	VideoKill();

	return 0;
}
