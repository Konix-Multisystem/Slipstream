/*
 * Slipstream emulator
 *
 * Assumes PAL (was european after all) at present
 */

#define SLIPSTREAM_VERSION	"0.2 Preview"

#include <GL/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "system.h"
#include "logfile.h"
#include "video.h"
#include "audio.h"
#include "keys.h"
#include "asic.h"
#include "dsp.h"
#include "memory.h"
#include "debugger.h"

ESlipstreamSystem curSystem=ESS_MSU;
int masterClock=0;

int HandleLoadSection(FILE* inFile)
{
	uint16_t	segment,offset;
	uint16_t	size;
	int		a=0;
	uint8_t		byte;

	if (2!=fread(&segment,1,2,inFile))
	{
		CONSOLE_OUTPUT("Failed to read segment for LoadSection\n");
		exit(1);
	}
	if (2!=fread(&offset,1,2,inFile))
	{
		CONSOLE_OUTPUT("Failed to read offset for LoadSection\n");
		exit(1);
	}
	fseek(inFile,2,SEEK_CUR);		// skip unknown
	if (2!=fread(&size,1,2,inFile))
	{
		CONSOLE_OUTPUT("Failed to read size for LoadSection\n");
		exit(1);
	}

	CONSOLE_OUTPUT("Found Section Load Memory : %04X:%04X   (%08X bytes)\n",segment,offset,size);

	for (a=0;a<size;a++)
	{
		if (1!=fread(&byte,1,1,inFile))
		{
			CONSOLE_OUTPUT("Failed to read data from LoadSection\n");
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
		CONSOLE_OUTPUT("Failed to read segment for ExecuteSection\n");
		exit(1);
	}
	if (2!=fread(&offset,1,2,inFile))
	{
		CONSOLE_OUTPUT("Failed to read offset for ExecuteSection\n");
		exit(1);
	}

	CS=segment;
	IP=offset;

	CONSOLE_OUTPUT("Found Section Execute : %04X:%04X\n",segment,offset);

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
			CONSOLE_OUTPUT("Failed to read section header\n");
			return 1;
		}
		expectedSize--;

		switch (sectionType)
		{
			case 0xFF:							// Not original specification - added to indicate system type is P88
				CONSOLE_OUTPUT("Found Section Konix 8088\n");
				curSystem=ESS_P88;
				break;
			case 0xC8:
				expectedSize-=HandleLoadSection(inFile);
				break;
			case 0xCA:
				expectedSize-=HandleExecuteSection(inFile);
				break;
			default:
				CONSOLE_OUTPUT("Unknown section type @%ld : %02X\n",ftell(inFile)-1,sectionType);
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
			CONSOLE_OUTPUT("Failed to read from %s\n",fname);
			return 1;
		}
		SetByte(address,data);
		address++;
		expectedSize--;
	}

	fclose(inFile);

	return 0;
}

void DSP_RESET(void);
void STEP(void);
void RESET(void);
void Z80_RESET(void);
void Z80_STEP(void);

extern uint8_t DSP_CPU_HOLD;		// For now, DSP will hold CPU during relevant DMAs like this

int use6MhzP88Cpu=1;
int emulateDSP=1;

void CPU_RESET()
{
	RESET();
	Z80_RESET();
}

void DoCPU8086()
{
#if ENABLE_DEBUG
		if (SEGTOPHYS(CS,IP)==0)//0x80120)//(0x80ECF))
		{
			doDebug=1;
			debugWatchWrites=1;
			debugWatchReads=1;
			doShowBlits=1;
//			numClocks=1;
		}
#endif
#if ENABLE_DEBUG
	if (doDebug)
	{
		Disassemble8086(SEGTOPHYS(CS,IP),1);
	}
#endif
	STEP();
}

void DoCPUZ80()
{
#if ENABLE_DEBUG
	if (Z80_PC==0)//0x400)
	{
		doDebug=1;
		debugWatchWrites=1;
		debugWatchReads=1;
		doShowBlits=1;
		//			numClocks=1;
	}
#endif
#if ENABLE_DEBUG
	if (doDebug)
	{
		DisassembleZ80(Z80_PC,1);
	}
#endif

	Z80_STEP();
}

int CPU_STEP(int doDebug)
{
	if (!DSP_CPU_HOLD)
	{
		switch (curSystem)
		{
			case ESS_MSU:
				DoCPU8086();
				return CYCLES;			// Assuming clock speed same as hardware chips
			case ESS_P88:
				DoCPU8086();
				if (use6MhzP88Cpu)
					return CYCLES*2;		// 6Mhz
				else
					return CYCLES;
			case ESS_FL1:
				DoCPUZ80();
				return Z80_CYCLES;
		}
	}
		
	return 1;		// CPU HELD, MASTER CLOCKS continue
}
	

void Usage()
{
	CONSOLE_OUTPUT("slipstream [opts] program.msu/program.p88\n");
	CONSOLE_OUTPUT("-f [disable P88 frequency divider]\n");
	CONSOLE_OUTPUT("-b address file.bin [Load binary to ram]\n");
	CONSOLE_OUTPUT("-n [disable DSP emulation]\n");
	CONSOLE_OUTPUT("-z filename [load a file as FL1 binary]\n");
	CONSOLE_OUTPUT("\nFor example to load the PROPLAY.MSU :\n");
	CONSOLE_OUTPUT("slipstream -b 90000 RCBONUS.MOD PROPLAY.MSU\n");
	exit(1);
}

void ParseCommandLine(int argc,char** argv)
{
	int a;

	if (argc<2)
	{
		return Usage();
	}

	for (a=1;a<argc;a++)
	{
		if (argv[a][0]=='-')
		{
			if (strcmp(argv[a],"-f")==0)
			{
				use6MhzP88Cpu=0;
				continue;
			}
			if (strcmp(argv[a],"-n")==0)
			{
				emulateDSP=0;
				continue;
			}
			if (strcmp(argv[a],"-b")==0)
			{
				if ((a+2)<argc)
				{
					// Grab address (hex)
					uint32_t address;
					sscanf(argv[a+1],"%x",&address);
					CONSOLE_OUTPUT("Loading Binary %s @ %05X\n",argv[a+2],address);
					LoadBinary(argv[a+2],address);
				}
				else
				{
					return Usage();
				}
				a+=2;
				continue;
			}
			if (strcmp(argv[a],"-z")==0)
			{
				if ((a+1)<argc)
				{
					LoadBinary(argv[a+1],1024);
					Z80_PC=1024;
					curSystem=ESS_FL1;

					return;
				}
				else
				{
					return Usage();
				}
				a+=1;
				continue;
			}
		}
		else
		{
			LoadMSU(argv[a]);
		}
	}
}

int main(int argc,char**argv)
{
	int numClocks;

	CPU_RESET();
	DSP_RESET();

	PALETTE_INIT();
	DSP_RAM_INIT();

	ParseCommandLine(argc,argv);

	VECTORS_INIT();

	VideoInitialise(WIDTH,HEIGHT,"Slipstream - V" SLIPSTREAM_VERSION);
	KeysIntialise();
	AudioInitialise(WIDTH*HEIGHT);

	//////////////////
	
//	doDebugTrapWriteAt=0x088DAA;
//	debugWatchWrites=1;
//	doDebug=1;

	while (1==1)
	{
		numClocks=CPU_STEP(doDebug);
		switch (curSystem)
		{
			case ESS_MSU:
				TickAsicMSU(numClocks);
				break;
			case ESS_P88:
				TickAsicP88(numClocks);
				break;
			case ESS_FL1:
				TickAsicFL1(numClocks);
				break;
		}
		masterClock+=numClocks;

		AudioUpdate(numClocks);

		if (masterClock>=WIDTH*HEIGHT)
		{	
			masterClock-=WIDTH*HEIGHT;

			TickKeyboard();
			if (JoystickPresent())
			{
				JoystickPoll();
			}
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


