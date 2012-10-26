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

#include "audio.h"
#include "keys.h"

#define SEGTOPHYS(seg,off)	( ((seg&0xF000)<<4) + ( (((seg&0x0FFF)<<4) + off)&0xFFFF) )				// Convert Segment,offset pair to physical address

unsigned char RAM[256*1024];

unsigned char PALETTE[256*2];

uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);
uint8_t GetPortB(uint16_t port);
void SetPortB(uint16_t port,uint8_t byte);
uint16_t GetPortW(uint16_t port);
void SetPortW(uint16_t port,uint16_t word);

// According to docs for PAL 17.734475 Mhz crystal - divided by 1.5	-- 11.822983 Mhz clock
//
//  11822983 ticks / 50  = 236459.66  (236459 ticks per frame)
//  236459 / 312 = 757 clocks per line
//
// Clocks per line is approximate but probably close enough - 757 clocks - matches documentation
//
//  active display is 120 to 631 horizontal		-- From documentation
//  active display is 33 to 288 vertical		-- From documentation
//

#define WIDTH	(757)			// Should probably remove hsync period and overscan
#define	HEIGHT	(312)			// Should probably remove vsync period and overscan

int intClocks=0;
int masterClock=0;
int hClock=0;
int vClock=0;

extern uint8_t *DIS_[256];			// FROM EDL
extern uint8_t *DIS_XX10001011[256];			// FROM EDL
extern uint8_t *DIS_XX10001110[256];			// FROM EDL
extern uint8_t *DIS_XX00110011[256];			// FROM EDL

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

int debugWatchWrites=0;

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
				printf("Unknown section type @%d : %02X\n",ftell(inFile)-1,sectionType);
				return 1;
		}
	}

	fclose(inFile);

	return 0;
}

uint8_t GetByte(uint32_t addr)
{
	addr&=0xFFFFF;
	if (addr<128*1024)
	{
		return RAM[addr];
	}
	printf("GetByte : %05X - TODO\n",addr);
	return 0xAA;
}

void SetByte(uint32_t addr,uint8_t byte)
{
	addr&=0xFFFFF;
	if (debugWatchWrites)
	{
		printf("Writing to address : %05X<-%02X\n",addr,byte);
	}
	if (addr<128*1024)
	{
		RAM[addr]=byte;
		return;
	}
	if (addr>=0xC0000 && addr<=0xC01FF)
	{
		PALETTE[addr]=byte;
		return;
	}
	printf("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
}

void DebugWPort(uint16_t port)
{
	switch (port)
	{
		case 0x0000:
			printf("HLP ??? Horizontal.... (Byte address)\n");
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
		default:
			printf("PORT WRITE UNKNOWN - TODO\n");
			exit(-1);
			break;
	}
}

void DebugRPort(uint16_t port)
{
	switch (port)
	{
		default:
			printf("PORT READ UNKNOWN - TODO\n");
			exit(-1);
			break;
	}
}

uint8_t GetPortB(uint16_t port)
{
	printf("GetPortB : %04X - TODO\n",port);
	DebugRPort(port);
	return 0xAA;
}

void SetPortB(uint16_t port,uint8_t byte)
{
	printf("SetPortB : %04X, %02X - TODO\n",port,byte);
	DebugWPort(port);
}

uint16_t GetPortW(uint16_t port)
{
	printf("GetPortW : %04X - TODO\n",port);
	DebugRPort(port);
	return 0xAAAA;
}

void SetPortW(uint16_t port,uint16_t word)
{
	printf("SetPortW : %04X, %04X - TODO\n",port,word);
	DebugWPort(port);
}


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
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

	uint8_t byte = GetByte(address);
	if (byte>realLength)
	{
		sprintf(temporaryBuffer,"UNKNOWN OPCODE");
		return temporaryBuffer;
	}
	else
	{
		const char* mnemonic=table[byte];
		const char* sPtr=mnemonic;
		char* dPtr=temporaryBuffer;
		int counting = 0;
		int doingDecode=0;

		if (sPtr==NULL)
		{
			sprintf(temporaryBuffer,"UNKNOWN OPCODE");
			return temporaryBuffer;
		}
	
		if (strcmp(mnemonic,"XX10001011")==0)
		{
			int tmpCount=0;
			decodeDisasm(DIS_XX10001011,address+1,&tmpCount,256);
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
				sprintf(sprintBuffer,"%02X",GetByte(address+offset));
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
			printf("%02X ",GetByte(address+a));
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
		printf("%02X ",GetByte(address+a));
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

void STEP(void);
void RESET(void);
void INTERRUPT(uint8_t);

extern uint16_t	PC;
extern uint8_t CYCLES;

void CPU_RESET()
{
	RESET();
}

int CPU_STEP(int intClocks,int doDebug)
{
	if (doDebug)
	{
		Disassemble(SEGTOPHYS(CS,IP),1);
	}
	if (intClocks)
	{
		printf("Interrupt Pending... TODO\n");
//		INTERRUPT(0xFF);
	
		if (CYCLES==0)
		{
			STEP();
		}
	}
	else
	{
		STEP();
	}

	return CYCLES;
}
	
int main(int argc,char**argv)
{
	int a;
	int numClocks;

	if (argc!=2)
	{
		printf("slipstream  program.msu\n");
		return 1;
	}
	
	CPU_RESET();

	if (LoadMSU(argv[1]))
		return 1;

	VideoInitialise(WIDTH,HEIGHT,"Slipstream - V0.001");
	KeysIntialise();
	AudioInitialise(WIDTH*HEIGHT);

	//////////////////
	
	debugWatchWrites=1;
//	DisassembleRange(0x0000,0x4000);

	while (1==1)
	{
		static int doDebug=1;
		
		numClocks=CPU_STEP(intClocks,doDebug);

		masterClock+=numClocks;
		if (intClocks)
		{
			intClocks-=numClocks;
			if (intClocks<0)
			{
				intClocks=0;
			}
		}

		AudioUpdate(numClocks);

		if (masterClock>=WIDTH*HEIGHT)
		{	
			masterClock-=WIDTH*HEIGHT;

			VideoUpdate();

			if (CheckKey(GLFW_KEY_ESC))
			{
				break;
			}
			if (CheckKey(GLFW_KEY_END))
			{
				doDebug=1;
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

