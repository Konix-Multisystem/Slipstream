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

#include "system.h"
#include "video.h"
#include "audio.h"
#include "keys.h"
#include "asic.h"
#include "dsp.h"

#define SEGTOPHYS(seg,off)	( ((seg)<<4) + (off) )				// Convert Segment,offset pair to physical address

#define RAM_SIZE	(768*1024)			// Right lets get a bit more serious with available ram. (Ill make the assumption for now it extends from segment 0x0000 -> 0xC000
                                                        // which is 768k - hardware chips reside above this point (with the bios assumed to reside are E000)
unsigned char RAM[RAM_SIZE];							
unsigned char PALETTE[256*2];			

ESlipstreamSystem curSystem=ESS_MSU;

uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);
uint8_t GetPortB(uint16_t port);
void SetPortB(uint16_t port,uint8_t byte);
uint16_t GetPortW(uint16_t port);
void SetPortW(uint16_t port,uint16_t word);

int doDebug=0;
int doShowPortStuff=0;
uint32_t doDebugTrapWriteAt=0xFFFFF;
int debugWatchWrites=0;
int debugWatchReads=0;

void PALETTE_INIT()
{
	int a;
	for (a=0;a<256;a++)			// Setup a dummy palette (helps in debugging)
	{
		PALETTE[a*2+0]=a;
		PALETTE[a*2+1]=a;
	}

	// Also pre-fill vector table at $0000 to point to an IRET instruction at $400 (suspect the bios is supposed to safely setup this area before booting a program)
	for (a=0;a<256;a++)
	{
		SetByte(a*4+0,0x00);
		SetByte(a*4+1,0x04);
		SetByte(a*4+2,0x00);
		SetByte(a*4+3,0x00);
	}
	SetByte(0x400,0xCF);
}



int masterClock=0;

extern uint8_t *DIS_[256];			// FROM EDL
extern uint8_t *DIS_XX00000000[256];			// FROM EDL
extern uint8_t *DIS_XX00000001[256];			// FROM EDL
extern uint8_t *DIS_XX00000010[256];			// FROM EDL
extern uint8_t *DIS_XX00000011[256];			// FROM EDL
extern uint8_t *DIS_XX00001000[256];			// FROM EDL
extern uint8_t *DIS_XX00001001[256];			// FROM EDL
extern uint8_t *DIS_XX00001010[256];			// FROM EDL
extern uint8_t *DIS_XX00001011[256];			// FROM EDL
extern uint8_t *DIS_XX00010001[256];			// FROM EDL
extern uint8_t *DIS_XX00010011[256];			// FROM EDL
extern uint8_t *DIS_XX00100000[256];			// FROM EDL
extern uint8_t *DIS_XX00100001[256];			// FROM EDL
extern uint8_t *DIS_XX00100010[256];			// FROM EDL
extern uint8_t *DIS_XX00100011[256];			// FROM EDL
extern uint8_t *DIS_XX00101000[256];			// FROM EDL
extern uint8_t *DIS_XX00101001[256];			// FROM EDL
extern uint8_t *DIS_XX00101010[256];			// FROM EDL
extern uint8_t *DIS_XX00101011[256];			// FROM EDL
extern uint8_t *DIS_XX00110000[256];			// FROM EDL
extern uint8_t *DIS_XX00110001[256];			// FROM EDL
extern uint8_t *DIS_XX00110010[256];			// FROM EDL
extern uint8_t *DIS_XX00110011[256];			// FROM EDL
extern uint8_t *DIS_XX00111000[256];			// FROM EDL
extern uint8_t *DIS_XX00111001[256];			// FROM EDL
extern uint8_t *DIS_XX00111010[256];			// FROM EDL
extern uint8_t *DIS_XX00111011[256];			// FROM EDL
extern uint8_t *DIS_XX10000000[256];			// FROM EDL
extern uint8_t *DIS_XX10000001[256];			// FROM EDL
extern uint8_t *DIS_XX10000011[256];			// FROM EDL
extern uint8_t *DIS_XX10000110[256];			// FROM EDL
extern uint8_t *DIS_XX10000111[256];			// FROM EDL
extern uint8_t *DIS_XX10001000[256];			// FROM EDL
extern uint8_t *DIS_XX10001001[256];			// FROM EDL
extern uint8_t *DIS_XX10001010[256];			// FROM EDL
extern uint8_t *DIS_XX10001011[256];			// FROM EDL
extern uint8_t *DIS_XX10001100[256];			// FROM EDL
extern uint8_t *DIS_XX10001101[256];			// FROM EDL
extern uint8_t *DIS_XX10001110[256];			// FROM EDL
extern uint8_t *DIS_XX10001111[256];			// FROM EDL
extern uint8_t *DIS_XX11000110[256];			// FROM EDL
extern uint8_t *DIS_XX11000111[256];			// FROM EDL
extern uint8_t *DIS_XX11010000[256];			// FROM EDL
extern uint8_t *DIS_XX11010001[256];			// FROM EDL
extern uint8_t *DIS_XX11010010[256];			// FROM EDL
extern uint8_t *DIS_XX11010011[256];			// FROM EDL
extern uint8_t *DIS_XX11110110[256];			// FROM EDL
extern uint8_t *DIS_XX11110111[256];			// FROM EDL
extern uint8_t *DIS_XX11111110[256];			// FROM EDL
extern uint8_t *DIS_XX11111111[256];			// FROM EDL

extern uint32_t DIS_max_;			// FROM EDL
extern uint32_t DIS_max_XX00000000;			// FROM EDL
extern uint32_t DIS_max_XX00000001;			// FROM EDL
extern uint32_t DIS_max_XX00000010;			// FROM EDL
extern uint32_t DIS_max_XX00000011;			// FROM EDL
extern uint32_t DIS_max_XX00001000;			// FROM EDL
extern uint32_t DIS_max_XX00001001;			// FROM EDL
extern uint32_t DIS_max_XX00001010;			// FROM EDL
extern uint32_t DIS_max_XX00001011;			// FROM EDL
extern uint32_t DIS_max_XX00010001;			// FROM EDL
extern uint32_t DIS_max_XX00010011;			// FROM EDL
extern uint32_t DIS_max_XX00100000;			// FROM EDL
extern uint32_t DIS_max_XX00100001;			// FROM EDL
extern uint32_t DIS_max_XX00100010;			// FROM EDL
extern uint32_t DIS_max_XX00100011;			// FROM EDL
extern uint32_t DIS_max_XX00101000;			// FROM EDL
extern uint32_t DIS_max_XX00101001;			// FROM EDL
extern uint32_t DIS_max_XX00101010;			// FROM EDL
extern uint32_t DIS_max_XX00101011;			// FROM EDL
extern uint32_t DIS_max_XX00110000;			// FROM EDL
extern uint32_t DIS_max_XX00110001;			// FROM EDL
extern uint32_t DIS_max_XX00110010;			// FROM EDL
extern uint32_t DIS_max_XX00110011;			// FROM EDL
extern uint32_t DIS_max_XX00111000;			// FROM EDL
extern uint32_t DIS_max_XX00111001;			// FROM EDL
extern uint32_t DIS_max_XX00111010;			// FROM EDL
extern uint32_t DIS_max_XX00111011;			// FROM EDL
extern uint32_t DIS_max_XX10000000;			// FROM EDL
extern uint32_t DIS_max_XX10000001;			// FROM EDL
extern uint32_t DIS_max_XX10000011;			// FROM EDL
extern uint32_t DIS_max_XX10000110;			// FROM EDL
extern uint32_t DIS_max_XX10000111;			// FROM EDL
extern uint32_t DIS_max_XX10001000;			// FROM EDL
extern uint32_t DIS_max_XX10001001;			// FROM EDL
extern uint32_t DIS_max_XX10001010;			// FROM EDL
extern uint32_t DIS_max_XX10001011;			// FROM EDL
extern uint32_t DIS_max_XX10001100;			// FROM EDL
extern uint32_t DIS_max_XX10001101;			// FROM EDL
extern uint32_t DIS_max_XX10001110;			// FROM EDL
extern uint32_t DIS_max_XX10001111;			// FROM EDL
extern uint32_t DIS_max_XX11000110;			// FROM EDL
extern uint32_t DIS_max_XX11000111;			// FROM EDL
extern uint32_t DIS_max_XX11010000;			// FROM EDL
extern uint32_t DIS_max_XX11010001;			// FROM EDL
extern uint32_t DIS_max_XX11010010;			// FROM EDL
extern uint32_t DIS_max_XX11010011;			// FROM EDL
extern uint32_t DIS_max_XX11110110;			// FROM EDL
extern uint32_t DIS_max_XX11110111;			// FROM EDL
extern uint32_t DIS_max_XX11111110;			// FROM EDL
extern uint32_t DIS_max_XX11111111;			// FROM EDL

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

uint32_t missing(uint32_t opcode)
{
	printf("IP : %04X:%04X\n",CS,IP);
	exit(-1);
}

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
			case 0xFF:							// Not original specification - added to indicate system type is P88
				printf("Found Section Konix 8088\n");
				curSystem=ESS_P88;
				break;
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

int LoadBinary(const char* fname,uint32_t address)					// Load an MSU file which will fill some memory regions and give us our booting point
{
	unsigned int expectedSize=0;
	FILE* inFile = fopen(fname,"rb");
	fseek(inFile,0,SEEK_END);
	expectedSize=ftell(inFile);
	fseek(inFile,0,SEEK_SET);

	while (expectedSize)
	{
		uint8_t data;

		// Read a byte
		if (1!=fread(&data,1,1,inFile))
		{
			printf("Failed to read from %s\n",fname);
			return 1;
		}
		SetByte(address,data);
		address++;
		expectedSize--;
	}

	fclose(inFile);

	return 0;
}


uint8_t GetByteMSU(uint32_t addr)
{
	addr&=0xFFFFF;
	if (addr<RAM_SIZE)
	{
		return RAM[addr];
	}
	if (addr>=0xC1000 && addr<=0xC1FFF)
	{
		return ASIC_HostDSPMemReadMSU(addr-0xC1000);
	}
	if (addr>=0xE0000)
	{
		return 0xCB;			// STUB BIOS, Anything that FAR calls into it, will be returned from whence it came
	}
#if ENABLE_DEBUG
	printf("GetByte : %05X - TODO\n",addr);
#endif
	return 0xAA;
}

uint8_t GetByteP88(uint32_t addr)
{
	addr&=0xFFFFF;
	if (addr<0x40000)
	{
		return RAM[addr];
	}
	if (addr>=0x80000 && addr<0xC0000)		// Expansion RAM 0
	{
		return RAM[addr];
	}
	if (addr>=0x41000 && addr<=0x41FFF)
	{
		return ASIC_HostDSPMemReadP88(addr-0x41000);
	}
#if ENABLE_DEBUG
	printf("GetByte : %05X - TODO\n",addr);
#endif
	return 0xAA;
}

uint8_t GetByte(uint32_t addr)
{
	uint8_t retVal;
	switch (curSystem)
	{
		case ESS_MSU:
			return GetByteMSU(addr);
		case ESS_P88:
			retVal=GetByteP88(addr);
#if ENABLE_DEBUG
			if (debugWatchReads)
			{
				printf("Reading from address : %05X->%02X\n",addr,retVal);
			}
#endif
			return retVal;
	}
	return 0xBB;
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

void SetByteMSU(uint32_t addr,uint8_t byte)
{
	addr&=0xFFFFF;
#if ENABLE_DEBUG
	if (addr==doDebugTrapWriteAt)
	{
		printf("STOMP STOMP STOMP\n");
	}
	if (debugWatchWrites)
	{
		if (addr<0xC1000 || addr>0xC1FFF)		// DSP handled seperately
		{
			printf("Writing to address : %05X<-%02X\n",addr,byte);
		}
	}
#endif
	if (addr<RAM_SIZE)
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
	printf("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
#endif
}

void SetByteP88(uint32_t addr,uint8_t byte)
{
	addr&=0xFFFFF;
#if ENABLE_DEBUG
	if (addr==doDebugTrapWriteAt)
	{
		printf("STOMP STOMP STOMP\n");
	}
	if (debugWatchWrites)
	{
		if (addr<0x41000 || addr>0x41FFF)		// DSP handled seperately
		{
			printf("Writing to address : %05X<-%02X\n",addr,byte);
		}
	}
#endif
	if (addr<0x40000)
	{
		RAM[addr]=byte;
		return;
	}
	if (addr>=0x80000 && addr<0xC0000)		// Expansion RAM 0
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
	printf("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
#endif
}

void SetByte(uint32_t addr,uint8_t byte)
{
	switch (curSystem)
	{
		case ESS_MSU:
			SetByteMSU(addr,byte);
			break;
		case ESS_P88:
			SetByteP88(addr,byte);
			break;
	}
}

void DebugWPort(uint16_t port)
{
#if ENABLE_DEBUG
	switch (curSystem)
	{
		case ESS_P88:

			switch (port)
			{
				case 0x0000:
					printf("INTL: Vertical line interrupt location\n");
					break;
				case 0x0001:
					printf("INTH: Vertical line interrupt location\n");
					break;
				case 0x0002:
					printf("STARTL - screen line start\n");
					break;
				case 0x0003:
					printf("STARTH - screen line start\n");
					break;
				case 0x0008:
					printf("SCROLL1 - TL pixel address LSB\n");
					break;
				case 0x0009:
					printf("SCROLL2 - TL pixel address middle byte\n");
					break;
				case 0x000A:
					printf("SCROLL3 - TL pixel address MSB\n");
					break;
				case 0x000B:
					printf("ACK - interrupt acknowledge\n");
					break;
				case 0x000C:
					printf("MODE - screen mode\n");
					break;
				case 0x000D:
					printf("BORDL - border colour\n");
					break;
				case 0x000E:
					printf("BORDH - border colour\n");
					break;
				case 0x000F:
					printf("PMASK - palette mask\n");
					break;
				case 0x0010:
					printf("INDEX - palette index\n");
					break;
				case 0x0011:
					printf("ENDL - screen line end\n");
					break;
				case 0x0012:
					printf("ENDH - screen line end\n");
					break;
				case 0x0013:
					printf("MEM - memory configuration\n");
					break;
				case 0x0015:
					printf("DIAG - diagnostics\n");
					break;
				case 0x0016:
					printf("DIS - disable interupts\n");
					break;
				case 0x0030:
					printf("BLPROG0\n");
					break;
				case 0x0031:
					printf("BLPROG1\n");
					break;
				case 0x0032:
					printf("BLPROG2\n");
					break;
				case 0x0033:
					printf("BLTCMD\n");
					break;
				case 0x0034:
					printf("BLTCON - blitter control\n");
					break;
					// The below 4 ports are from the development kit <-> PC interface  | Chip Z8536 - Zilog CIO counter/timer parallel IO Unit
				case 0x0060:
					printf("DRC - Data Register C\n");
					break;
				case 0x0061:
					printf("DRB - Data Register B\n");
					break;
				case 0x0062:
					printf("DRA - Data Register A\n");
					break;
				case 0x0063:
					printf("CTRL - Control Register (not sure which one yet though)\n");
					break;
				default:
					printf("PORT WRITE UNKNOWN - TODO\n");
					exit(-1);
					break;
			}

			break;
		case ESS_MSU:
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
				case 0x0040:
					printf("BLTPC (low 16 bits) (Word address)\n");
					break;
				case 0x0042:
					printf("BLTCMD (Word address)\n");
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
			break;
	}
#endif
}

void DebugRPort(uint16_t port)
{
#if ENABLE_DEBUG
	switch (curSystem)
	{
		case ESS_P88:

			switch (port)
			{
				case 0x0000:
					printf("HLPL - Horizontal scanline position low byte\n");
					break;
				case 0x0001:
					printf("HLPH - Horizontal scanline position hi byte\n");
					break;
				case 0x0002:
					printf("VLPL - Vertical scanline position low byte\n");
					break;
				case 0x0003:
					printf("VLPH - Vertical scanline position hi byte\n");
					break;
				case 0x0040:
					printf("PORT1 - Joystick 1\n");
					break;
				case 0x0050:
					printf("PORT2 - Joystick 2\n");
					break;
				case 0x0060:
					printf("DRC - Data Register C\n");
					break;
				case 0x0061:
					printf("DRB - Data Register B\n");
					break;
				case 0x0062:
					printf("DRA - Data Register A\n");
					break;
				case 0x0063:
					printf("CTRL - Control Register (not sure which one yet though)\n");
					break;
				default:
					printf("PORT READ UNKNOWN - TODO\n");
					exit(-1);
					break;
			}

			break;
		case ESS_MSU:

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
	switch (curSystem)
	{
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
						printf("Warning unknown numPadRowSelectValue : %02X\n",numPadRowSelect);
#endif
						return 0xFF;
				}
			}
			break;
		case ESS_P88:
			if (port<=3)
			{
				return ASIC_ReadP88(port,doShowPortStuff);
			}
			if (port==0x40)
			{
				return (0xFFFF ^ joyPadState)>>8;
			}
			if (port==0x50)
			{
				return (0xFFFF ^ joyPadState)&0xFF;
			}
			break;
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
			switch (curSystem)
			{
				case ESS_MSU:
					ASIC_WriteMSU(port,byte,doShowPortStuff);
					break;
				case ESS_P88:
					ASIC_WriteP88(port,byte,doShowPortStuff);
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

uint16_t GetPortW(uint16_t port)
{
#if ENABLE_DEBUG
	if (doShowPortStuff)
	{
		printf("GetPortW : %04X - TODO\n",port);
		DebugRPort(port);
	}
#endif
	switch (curSystem)
	{
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
		case ESS_P88:
			if (port<=3)
			{
				return (ASIC_ReadP88(port+1,doShowPortStuff)<<8)|ASIC_ReadP88(port,doShowPortStuff);
			}
			break;
	}
	return 0x0000;
}

void SetPortW(uint16_t port,uint16_t word)
{
	switch (curSystem)
	{
		case ESS_MSU:
			ASIC_WriteMSU(port,word&0xFF,doShowPortStuff);
			ASIC_WriteMSU(port+1,word>>8,doShowPortStuff);
			if (port==0xC0)
			{
				ADPSelect=word&0xFF;
			}
			break;
		case ESS_P88:
			ASIC_WriteP88(port,word&0xFF,doShowPortStuff);
			ASIC_WriteP88(port+1,word>>8,doShowPortStuff);
			break;
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
			decodeDisasm(DIS_,address+1,&tmpCount,DIS_max_);
			*count=tmpCount+1;
			strcpy(segOveride,mnemonic+10);
			strcat(segOveride,temporaryBuffer);
			return segOveride;
		}
		if (strcmp(mnemonic,"XX00000000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00000000,address+1,&tmpCount,DIS_max_XX00000000);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00000001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00000001,address+1,&tmpCount,DIS_max_XX00000001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00000010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00000010,address+1,&tmpCount,DIS_max_XX00000010);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00000011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00000011,address+1,&tmpCount,DIS_max_XX00000011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00001000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00001000,address+1,&tmpCount,DIS_max_XX00001000);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00001001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00001001,address+1,&tmpCount,DIS_max_XX00001001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00001010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00001010,address+1,&tmpCount,DIS_max_XX00001010);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00001011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00001011,address+1,&tmpCount,DIS_max_XX00001011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00010001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00010001,address+1,&tmpCount,DIS_max_XX00010001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00010011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00010011,address+1,&tmpCount,DIS_max_XX00010011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00100000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00100000,address+1,&tmpCount,DIS_max_XX00100000);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00100001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00100001,address+1,&tmpCount,DIS_max_XX00100001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00100010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00100010,address+1,&tmpCount,DIS_max_XX00100010);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00100011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00100011,address+1,&tmpCount,DIS_max_XX00100011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00101000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00101000,address+1,&tmpCount,DIS_max_XX00101000);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00101001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00101001,address+1,&tmpCount,DIS_max_XX00101001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00101010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00101010,address+1,&tmpCount,DIS_max_XX00101010);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00101011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00101011,address+1,&tmpCount,DIS_max_XX00101011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00110000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00110000,address+1,&tmpCount,DIS_max_XX00110000);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00110001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00110001,address+1,&tmpCount,DIS_max_XX00110001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00110010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00110010,address+1,&tmpCount,DIS_max_XX00110010);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00110011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00110011,address+1,&tmpCount,DIS_max_XX00110011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00111000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00111000,address+1,&tmpCount,DIS_max_XX00111000);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00111001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00111001,address+1,&tmpCount,DIS_max_XX00111001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00111010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00111010,address+1,&tmpCount,DIS_max_XX00111010);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX00111011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX00111011,address+1,&tmpCount,DIS_max_XX00111011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10000000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10000000,address+1,&tmpCount,DIS_max_XX10000000);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10000001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10000001,address+1,&tmpCount,DIS_max_XX10000001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10000011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10000011,address+1,&tmpCount,DIS_max_XX10000011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10000110")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10000110,address+1,&tmpCount,DIS_max_XX10000110);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10000111")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10000111,address+1,&tmpCount,DIS_max_XX10000111);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001000,address+1,&tmpCount,DIS_max_XX10001000);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001001,address+1,&tmpCount,DIS_max_XX10001001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001010,address+1,&tmpCount,DIS_max_XX10001010);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001011,address+1,&tmpCount,DIS_max_XX10001011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001100")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001100,address+1,&tmpCount,DIS_max_XX10001100);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001101")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001101,address+1,&tmpCount,DIS_max_XX10001101);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001110")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001110,address+1,&tmpCount,DIS_max_XX10001110);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX10001111")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001111,address+1,&tmpCount,DIS_max_XX10001111);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11000110")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11000110,address+1,&tmpCount,DIS_max_XX11000110);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11000111")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11000111,address+1,&tmpCount,DIS_max_XX11000111);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11010000")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11010000,address+1,&tmpCount,DIS_max_XX11010000);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11010001")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11010001,address+1,&tmpCount,DIS_max_XX11010001);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11010010")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11010010,address+1,&tmpCount,DIS_max_XX11010010);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11010011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11010011,address+1,&tmpCount,DIS_max_XX11010011);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11110110")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11110110,address+1,&tmpCount,DIS_max_XX11110110);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11110111")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11110111,address+1,&tmpCount,DIS_max_XX11110111);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11111110")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11111110,address+1,&tmpCount,DIS_max_XX11111110);
			*count=tmpCount+1;
			return temporaryBuffer;
		}
		if (strcmp(mnemonic,"XX11111111")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX11111111,address+1,&tmpCount,DIS_max_XX11111111);
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
	const char* retVal = decodeDisasm(DIS_,address,&numBytes,DIS_max_);

	if (strcmp(retVal+(strlen(retVal)-14),"UNKNOWN OPCODE")==0)
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

extern uint8_t DSP_CPU_HOLD;		// For now, DSP will hold CPU during relevant DMAs like this

void CPU_RESET()
{
	RESET();
}

int CPU_STEP(int doDebug)
{
	if (!DSP_CPU_HOLD)
	{
#if ENABLE_DEBUG
		if (doDebug)
		{
			Disassemble(SEGTOPHYS(CS,IP),1);
		}
#endif
		STEP();
	}
	else
	{
		return 1;		// CPU HELD, MASTER CLOCKS continue
	}

	return CYCLES;
}
	

void Usage()
{
	printf("slipstream [opts] program.msu\n");
	printf("-b address file.bin\n");
	printf("\nFor example to load the proplay.MSU :\n");
	printf("slipstream -b 90000 RCBONUS.MOD PROPLAY.MSU\n");
}

void ParseCommandLine(int argc,char** argv)
{
	int a;
	for (a=1;a<argc;a++)
	{
		if (argv[a][0]=='-')
		{
			if (strcmp(argv[a],"-b")==0)
			{
				if ((a+2)<argc)
				{
					// Grab address (hex)
					uint32_t address;
					sscanf(argv[a+1],"%x",&address);
					printf("Loading Binary %s @ %05X\n",argv[a+2],address);
					LoadBinary(argv[a+2],address);
				}
				else
				{
					return Usage();
				}
				a+=2;
				continue;
			}
		}
		else
		{
			LoadMSU(argv[a]);
		}
	}
}

extern int doShowBlits;

int main(int argc,char**argv)
{
	int numClocks;

	CPU_RESET();
	DSP_RESET();

	PALETTE_INIT();
	DSP_RAM_INIT();

	ParseCommandLine(argc,argv);

	VideoInitialise(WIDTH,HEIGHT,"Slipstream - V0.001");
	KeysIntialise();
	AudioInitialise(WIDTH*HEIGHT);

	//////////////////
	
//	DisassembleRange(0x0000,0x4000);

//	doDebugTrapWriteAt=0x088DAA;
//	debugWatchWrites=1;
//	doDebug=1;

	while (1==1)
	{
#if ENABLE_DEBUG
		if (SEGTOPHYS(CS,IP)==0x80438)//(0x80ECF))
		{
			doDebug=1;
			debugWatchWrites=1;
			debugWatchReads=1;
			doShowBlits=1;
//			numClocks=1;
		}
#endif
		numClocks=CPU_STEP(doDebug);
		switch (curSystem)
		{
			case ESS_MSU:
				TickAsicMSU(numClocks);
				break;
			case ESS_P88:
				TickAsicP88(numClocks);
				break;
		}
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
#if !ENABLE_DEBUG
			VideoWait();
#endif
		}
	}

	KeysKill();
	AudioKill();
	VideoKill();

	return 0;
}
