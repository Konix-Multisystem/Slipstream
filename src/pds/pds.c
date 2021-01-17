#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <time.h>

#include "GLFW/glfw3.h"

#include "../disasm.h"
#include "../host/video.h"
#include "pds.h"

/*
 KB STUFF
*/
#define KEYBUF_LEN	(256)
uint8_t PDS_keyBuffer[KEYBUF_LEN] = { 0 };

uint8_t PDS_keyBufferRead = 0;
uint8_t PDS_keyBufferWrite = 0;

int ibm_scancodes[]={
	GLFW_KEY_ESCAPE,'1','2','3','4','5','6','7','8','9','0','-','=',GLFW_KEY_BACKSPACE,
	GLFW_KEY_TAB,'Q','W','E','R','T','Y','U','I','O','P','[',']',GLFW_KEY_ENTER,
	GLFW_KEY_LEFT_CONTROL,'A','S','D','F','G','H','J','K','L',';',0x27,0x60,
	GLFW_KEY_LEFT_SHIFT,'\\','Z','X','C','V','B','N','M',',','.','/',GLFW_KEY_RIGHT_SHIFT,GLFW_KEY_KP_MULTIPLY,			//NOTE KP MULTIPLY FILLS IN FOR MISSING KEY AFTER RSHIFT
	GLFW_KEY_LEFT_ALT,' ',GLFW_KEY_CAPS_LOCK,
	GLFW_KEY_F1,GLFW_KEY_F2,GLFW_KEY_F3,GLFW_KEY_F4,GLFW_KEY_F5,GLFW_KEY_F6,GLFW_KEY_F7,GLFW_KEY_F8,GLFW_KEY_F9,GLFW_KEY_F10,
	GLFW_KEY_NUM_LOCK,GLFW_KEY_KP_ENTER,
	GLFW_KEY_KP_7,GLFW_KEY_KP_8,GLFW_KEY_KP_9,GLFW_KEY_KP_SUBTRACT,
	GLFW_KEY_KP_4,GLFW_KEY_KP_5,GLFW_KEY_KP_6,GLFW_KEY_KP_ADD,
	GLFW_KEY_KP_1,GLFW_KEY_KP_2,GLFW_KEY_KP_3,
	GLFW_KEY_KP_0,GLFW_KEY_KP_DECIMAL,9999};

int ibm_extended_scancodes[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,			// 00
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,			// 10
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,			// 20
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,			// 30
	0,0,0,0,0,0,0,0,GLFW_KEY_UP,0,0,GLFW_KEY_LEFT,0,GLFW_KEY_RIGHT,0,0,			// 40
	GLFW_KEY_DOWN,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,			// 50
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,			// 60
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,			// 70
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,			// 80
	9999 };

unsigned char PDS_FindCode(int key)
{
	int* ptr=ibm_scancodes;
	int counter=1;
	while (*ptr!=9999)
	{
		if (*ptr==key)
			return counter;
		ptr++;
		counter++;
	}
	return 0;
}

unsigned char PDS_FindExtendedCode(int key)
{
	int* ptr=ibm_extended_scancodes;
	int counter=0;
	while (*ptr!=9999)
	{
		if (*ptr==key)
			return counter;
		ptr++;
		counter++;
	}
	return 0;
}

void PDS_kbHandler( GLFWwindow* window, int key, int scan, int action, int mod )		/* At present ignores which window, will add per window keys later */
{
	unsigned char scanCodeNum=PDS_FindCode(key);
	if (scanCodeNum!=0)
	{
		if (action==GLFW_RELEASE)
		{
			scanCodeNum |= 0x80;
		}
		if (((uint8_t)(PDS_keyBufferWrite + 1)) != PDS_keyBufferRead)
		{
			printf("KeyCode Logged : %02X\n", scanCodeNum);
			PDS_keyBuffer[PDS_keyBufferWrite++] = scanCodeNum;
		}
	}
	else
	{
		scanCodeNum = PDS_FindExtendedCode(key);
		if (scanCodeNum != 0)
		{
			if (action == GLFW_RELEASE)
			{
				scanCodeNum |= 0x80;
			}
			if (((uint8_t)(PDS_keyBufferWrite + 1)) != PDS_keyBufferRead)
			{
				PDS_keyBuffer[PDS_keyBufferWrite++] = 0xE0;
			}
			if (((uint8_t)(PDS_keyBufferWrite + 1)) != PDS_keyBufferRead)
			{
				printf("Extended KeyCode Logged : 0xE0 %02X\n", scanCodeNum);
				PDS_keyBuffer[PDS_keyBufferWrite++] = scanCodeNum;
			}
		}
	}
}

extern GLFWwindow *windows[MAX_WINDOWS];

void PDS_Keys()
{
	glfwSetKeyCallback(windows[MAIN_WINDOW], PDS_kbHandler);
}


/*--------------------------*/

/*

 Not a full dos emulator, more a crude layer to allow running the pds software and using it to assemble things...

*/

int PDSpause = 0;

const uint32_t exeLoadSegment = 0x1000;
const uint32_t intRedirectAddress = 0x500;

uint8_t* PDS_EXE = NULL;
size_t PDS_EXE_Size = 0;

uint8_t PDS_Ram[1 * 1024 * 1024 + 65536];

uint8_t PDS_COMMS[16];

#if OS_WINDOWS
void DebugBreak();
#define DBG_BREAK DebugBreak()
#else
#define DBG_BREAK
#endif

uint8_t PDS_GetByte(uint32_t addr)
{
	if (addr >= 0x400 && addr <= 0x4FF)
	{
		switch (addr & 0xFF)
		{
		case 0x08:		// LPT1
		case 0x09:
		case 0x10:		// Equipment List
		case 0x11:
		case 0x13:		// Memory size in Kb
		case 0x14:
		case 0x63:		// Port for CRT
		case 0x64:
			break;
		case 0x17:	// KBD flag byte 0
		case 0x18:	// KBD flag byte 1
			return 0;
		default:
			printf("BDA Read Access unknown @%04X", addr);
			DBG_BREAK;
			break;
		}
	}
	return PDS_Ram[addr];
}

void PDS_SetByte(uint32_t addr,uint8_t byte)
{
	if (addr >= 0x000 && addr <= 0x3FF)
	{
		int vector = addr / 4;
		uint16_t offset = PDS_Ram[vector * 4 + 1];
		offset <<= 8;
		offset |= PDS_Ram[vector * 4 + 0];
		uint16_t segment = PDS_Ram[vector * 4 + 3];
		segment <<= 8;
		segment |= PDS_Ram[vector * 4 + 2];
		printf("Interrupt Vector Rewritten : %02X  (%04X:%04X)\n", vector, segment, offset);
	}
	if (addr >= 0x400 && addr <= 0x4FF)
	{
		switch (addr & 0xFF)
		{
		default:
			printf("BDA Write Access unknown @%04X", addr);
			DBG_BREAK;
			break;
		}
	}
	PDS_Ram[addr] = byte;
}

uint8_t PDS_PeekByte(uint32_t addr)
{
	return PDS_GetByte(addr);
}

uint8_t PDS_GetPortB(uint16_t port);

uint16_t PDS_GetPortW(uint16_t port)
{
	uint16_t ret = PDS_GetPortB(port+1);
	ret <<= 8;
	ret |= PDS_GetPortB(port);
	return ret;
}


uint8_t PDS_GetPortB(uint16_t port)
{
	uint8_t ret = 0xFF;
	switch (port)
	{
	case 0x0060:	// KBD code
		if (PDS_keyBufferRead != PDS_keyBufferWrite)
			ret = PDS_keyBuffer[PDS_keyBufferRead];
		else
			ret = 0xFF;
		break;
	case 0x0061:	// KBD status
		ret = 0;
		break;
	/*case 0x0040:	// PIT - counter 0, counter divisor
		printf("PIT Counter 0 : %02X\n", byte);
		break;
	case 0x0043:	// PIT - Mode Register
		{
			const char* counters[4] = { "0","1","2","ERR" };
			const char* latch[4] = { "counter latch command", "r/w counter bits 0-7 only","r/w counter bits 8-15 only","r/w counter bits 0-7 then 8-15" };
			const char* mode[8] = { "mode 0 select","one shot","rate generator","square wave","soft strobe","hard strobe","rate generator","square wave" };
			const char* type[2] = { "binary counter 16 bits","BCD counter" };

			printf("PIT Mode - %02X  (counter %s | %s | mode %s | %s)\n", byte, counters[byte >> 6], latch[(byte >> 4) & 0x3], mode[(byte >> 1) & 0x7], type[byte & 1]);
		}
		break;*/
	case 0x0302:	// dunno at present
	case 0x0304:	// dunno at present
	case 0x0306:	// dunno at present
		ret = PDS_COMMS[port - 0x300];
		printf("PDS_COMMS? %04X->0xFF\n", port);
		break;
	/*case 0x03D9:	// CGA - Palette Register
		break;*/
	default:
		printf("Read from unhandled Port : %04X->\n", port);
		DBG_BREAK;
		break;
	}
	return ret;
}

void PDS_SetPortW(uint16_t port,uint16_t word)
{
	DBG_BREAK;
}

uint8_t CGA_Index = 0;
uint8_t KB_Control = 0;

void PDS_SetPortB(uint16_t port,uint8_t byte)
{
	const char* CGA_IndexNames[18] = { "Horizontal Total","Horizontal Displayed","Horizontal Sync Position","Horizontal Sync Width",
		"Vertical Total", "Vertical Displayed", "Vertical Sync Pos", "Vertical Sync Width", "Interlace Mode",
		"Max Scan Lines", "Cursor Start", "Cursor End", "Start Address High", "Start Address Low", "Cursor Location High",
		"Cursor Location Low", "Light Pen High", "Light Pen Low" };

	switch (port)
	{
	case 0x0020:	//Int status
		if (byte & 0x20)
		{
			//KB_InterruptLatch = 0;
		}
		break;
	case 0x0040:	// PIT - counter 0, counter divisor
		printf("PIT Counter 0 : %02X\n", byte);
		break;
	case 0x0042:	// PIT - counter 2, casette/speaker (BEEP)
		printf("PIT Counter 2 : %02X\n", byte);
		break;
	case 0x0043:	// PIT - Mode Register
		{
			const char* counters[4] = { "0","1","2","ERR" };
			const char* latch[4] = { "counter latch command", "r/w counter bits 0-7 only","r/w counter bits 8-15 only","r/w counter bits 0-7 then 8-15" };
			const char* mode[8] = { "mode 0 select","one shot","rate generator","square wave","soft strobe","hard strobe","rate generator","square wave" };
			const char* type[2] = { "binary counter 16 bits","BCD counter" };

			printf("PIT Mode - %02X  (counter %s | %s | mode %s | %s)\n", byte, counters[byte >> 6], latch[(byte >> 4) & 0x3], mode[(byte >> 1) & 0x7], type[byte & 1]);
		}
		break;
	case 0x0061:	// Kb control
		if (PDS_keyBufferRead!=PDS_keyBufferWrite && (KB_Control & 0x80) && ((byte & 0x80) == 0))
		{
			PDS_keyBufferRead++;
		}
		KB_Control = byte;
		return;
	case 0x0302:	// dunno at present
	case 0x0304:	// dunno at present
	case 0x0306:	// dunno at present
		PDS_COMMS[port - 0x300] = byte;
		printf("PDS_COMMS? %04X<-%02X\n", port, byte);
		break;
	case 0x03D4:	// CGA Index Register
	{
		if (byte > 0x11)
			DBG_BREAK;	// BAD Index
		CGA_Index = byte;
		printf("CGA Video Register Index Set : %s\n", CGA_IndexNames[byte]);
		break;
	}
	case 0x03D5:	// CGA Data Register
		printf("CGA Video Register Data Set : %s (%04X)<-%02X\n", CGA_IndexNames[CGA_Index],CGA_Index, byte);
		break;
	case 0x03D9:	// CGA - Palette Register
		break;
	default:
		printf("Write to unknown Port : %04X<-%02X\n", port, byte);
		DBG_BREAK;
		break;
	}
}

void DOS_VECTOR_TRAP(uint8_t vector);

uint32_t PDS_missing(uint32_t opcode)
{
	if (opcode == 0xD8)
	{
		uint32_t addr = PDS_GETPHYSICAL_EIP();
		if (addr >= intRedirectAddress && addr < intRedirectAddress+0x300)
		{
			uint8_t vector = PDS_GetByte(addr);

			DOS_VECTOR_TRAP(vector);

			PDS_EIP++;	// Skip vector (will hit iret on next step)
			return 0;
		}
	}

	int a;
	printf("IP : %08X\n",PDS_GETPHYSICAL_EIP());
	printf("Next 7 Bytes : ");
	for (a=0;a<7;a++)
	{
		printf("%02X ",PDS_PeekByte(PDS_GETPHYSICAL_EIP()+a));
	}
	printf("\nNext 7-1 Bytes : ");
	for (a=0;a<7;a++)
	{
		printf("%02X ",PDS_PeekByte(PDS_GETPHYSICAL_EIP()+a-1));
	}
	printf("\nNext 7-2 Bytes : ");
	for (a=0;a<7;a++)
	{
		printf("%02X ",PDS_PeekByte(PDS_GETPHYSICAL_EIP()+a-2));
	}
	printf("\n");
	DBG_BREAK;
	return 0;
}

uint32_t PDS_unimplemented(uint32_t opcode)
{
	return PDS_missing(opcode);
}



int PDS_LoadEXE(const char* filename)
{
	size_t expectedSize=0;
	FILE* inFile = fopen(filename,"rb");
	if (inFile == NULL)
	{
		printf("Failed to read from %s\n", filename);
		return 1;
	}
	fseek(inFile,0,SEEK_END);
	expectedSize=ftell(inFile);
	fseek(inFile,0,SEEK_SET);
	
	PDS_EXE_Size = expectedSize;
	if ((PDS_EXE_Size & 1) == 1)
		PDS_EXE_Size++;				// multiple of 2 so checksum is simple

	PDS_EXE = malloc(PDS_EXE_Size);

	memset(PDS_EXE, 0, PDS_EXE_Size);

	expectedSize -= fread(PDS_EXE, 1, expectedSize, inFile);
	if (expectedSize != 0)
	{
		printf("Failed to read from %s\n", filename);
		free(PDS_EXE);
		PDS_EXE = NULL;
		return 1;
	}

	fclose(inFile);

	return 0;
}

struct DOSMZ
{
	uint16_t sig;
	uint16_t extraBytes;
	uint16_t pages;
	uint16_t relocationItems;
	uint16_t headerSize;
	uint16_t minimumAllocation;
	uint16_t maximumAllocation;
	uint16_t initialSS;
	uint16_t initialSP;
	uint16_t checksum;
	uint16_t initialIP;
	uint16_t initialCS;
	uint16_t relocationTable;
	uint16_t overlay;
};

struct DOSRELOC
{
	uint16_t offset;
	uint16_t segment;
};

#if OS_WINDOWS
#define MSVC_PACK_BEGIN __pragma(pack(push,1))
#define MSVC_PACK_END   __pragma(pack(pop))
#define GCC_PACK
#else
#define MSVC_PACK_BEGIN
#define MSVC_PACK_END
#define GCC_PACK __attribute__((packed))
#endif

MSVC_PACK_BEGIN;
struct GCC_PACK DOSPSP
{
	uint16_t exit;							// 00
	uint16_t top;							// 02
	uint8_t  zero;							// 04
	uint8_t	 fCall[5];						// 05
	uint32_t int22TerminateAddress;			// 0A
	uint32_t int23CtrlBreakAddress;			// 0E
	uint32_t int24CriticalErrorAddress;		// 12
	uint16_t parentProcessAddr;				// 16
	uint8_t  fileHandleArray[20];			// 18
	uint16_t segmentEnvironment;			// 2C
	uint32_t stackAndSegmentLastInt21;		// 2E
	uint16_t handleArraySize;				// 32
	uint32_t handleArrayPointer;			// 34
	uint32_t previousPSP;					// 38
	uint32_t reserved1;						// 3C
	uint16_t dosVersion;					// 40
	uint8_t  reserved2[14];					// 42
	uint8_t  dosFuncDispatch[3];			// 50
	uint8_t  unused[9];						// 53
	uint8_t  defaultUnopenendFCB1[16];		// 5C
	uint8_t  defaultUnopenendFCB2[20];		// 6C
	uint8_t  numCharactersAfterProgram;		// 80
	uint8_t  commandLine[127];				// 81
};
MSVC_PACK_END;

void FetchRegistersPDS(char* tmp)
{
	sprintf(tmp,"--------\nFLAGS = O  D  I  T  S  Z  -  A  -  P  -  C\n        %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s\nAX= %04X\nBX= %04X\nCX= %04X\nDX= %04X\nSP= %04X\nBP= %04X\nSI= %04X\nDI= %04X\nCS= %04X\nDS= %04X\nES= %04X\nSS= %04X\n--------\n",
			PDS_EFLAGS&0x800 ? "1" : "0",
			PDS_EFLAGS&0x400 ? "1" : "0",
			PDS_EFLAGS&0x200 ? "1" : "0",
			PDS_EFLAGS&0x100 ? "1" : "0",
			PDS_EFLAGS&0x080 ? "1" : "0",
			PDS_EFLAGS&0x040 ? "1" : "0",
			PDS_EFLAGS&0x020 ? "1" : "0",
			PDS_EFLAGS&0x010 ? "1" : "0",
			PDS_EFLAGS&0x008 ? "1" : "0",
			PDS_EFLAGS&0x004 ? "1" : "0",
			PDS_EFLAGS&0x002 ? "1" : "0",
			PDS_EFLAGS&0x001 ? "1" : "0",
			PDS_EAX&0xFFFF,PDS_EBX&0xFFFF,PDS_ECX&0xFFFF,PDS_EDX&0xFFFF,PDS_ESP&0xFFFF,PDS_EBP&0xFFFF,PDS_ESI&0xFFFF,PDS_EDI&0xFFFF,PDS_CS,PDS_DS,PDS_ES,PDS_SS);
}


void DebugIt()
{
	uint32_t address = PDS_GETPHYSICAL_EIP();
	char blah[65536] = { 0 };
	char* tmp = blah;
	char tBuffer[1024];
	int a;

	FetchRegistersPDS(blah);
	printf("%s",blah);
	blah[0] = 0;

	InStream disMe;
	disMe.cpu = CPU_X86;
	disMe.bytesRead = 0;
	disMe.curAddress = address;
	disMe.useAddress = 1;
	disMe.findSymbol = NULL;
	disMe.PeekByte = PDS_PeekByte;
	Disassemble(&disMe, PDS_cSize);

	if (disMe.bytesRead != 0)
	{
		sprintf(tBuffer, "%06X : ", address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see
		strcat(tmp, tBuffer);

		for (a = 0; a < disMe.bytesRead; a++)
		{
			sprintf(tBuffer, "%02X ", PDS_PeekByte(address + a));
			strcat(tmp, tBuffer);
		}
		for (a = 0; a < 15 - disMe.bytesRead; a++)
		{
			strcat(tmp, "   ");
		}
		sprintf(tBuffer, "%s\n", (char*)GetOutputBuffer());
		strcat(tmp, tBuffer);

		address += disMe.bytesRead;
		tmp += strlen(tmp);
		*tmp = 0;
	}

	printf("%s",blah);


}


void PDS_Setup()
{
	PDS_RESET();
	// Process the exe 
	struct DOSMZ* header = (struct DOSMZ*)PDS_EXE;

	if (header->sig != 0x5A4D)
		return;

	// We can avoid bothering with relocation
	uint8_t* loadImage = PDS_EXE + header->headerSize * 16;
	
	// Pretend the exe has been loaded at 0x10000

	size_t totalLoadImageSize = PDS_EXE_Size - header->headerSize * 16;
	for (size_t a = 0; a < totalLoadImageSize; a++)
	{
		PDS_SetByte(0x10000 + a, *loadImage++);
	}

	// Relocate loadimage
	struct DOSRELOC* relocations = (struct DOSRELOC*) (PDS_EXE + header->relocationTable);
	for (int reloc = 0; reloc < header->relocationItems; reloc++)
	{
		uint32_t addr = exeLoadSegment * 16;
		addr += relocations->segment * 16;
		addr += relocations->offset;
		uint16_t* ptr = (uint16_t*) (PDS_Ram + addr);
		*ptr += exeLoadSegment;
		relocations++;
	}

	// Setup an environment block (256 bytes) (0x10000 - 512) = 0xFE00 = FE0:0000
	uint8_t* env = PDS_Ram + 0xFE00;
	env[0] = 0;	// empty environment for now

	// Setup a PSP (0x10000 - 256 = FF00 = FF0:0000
	struct DOSPSP* psp = (struct DOSPSP*) (PDS_Ram + 0xFF00);
	psp->exit = 0x20CD;
	psp->top = 0xFFFF;
	psp->zero = 0;
	psp->fCall[0] = 0x9A;
	psp->fCall[1] = 0x00;
	psp->fCall[2] = 0x20;		// DOS Function Dispatcher = 0000:2000
	psp->fCall[3] = 0x00;
	psp->fCall[4] = 0x00;		// BIOS/DOS emulation redirection address range = 0x0000:0000 - 0x0000:FFFF   == 0x0-0xFFFF
	psp->int22TerminateAddress = 0x00002100;
	psp->int23CtrlBreakAddress = 0x00002200;
	psp->int24CriticalErrorAddress = 0x00002300;
	psp->parentProcessAddr = 0;
	for (int a=0;a<20;a++)
		psp->fileHandleArray[a] = 0xFF;
	psp->segmentEnvironment = 0xFE0;
	psp->stackAndSegmentLastInt21 = 0;
	psp->handleArraySize = 0;
	psp->handleArrayPointer = 0;
	psp->previousPSP = 0;
	psp->dosVersion = 0;
	psp->dosFuncDispatch[0] = 0xCD;		// Int 21h
	psp->dosFuncDispatch[1] = 0x21;
	psp->dosFuncDispatch[2] = 0xCB;		// retf
	psp->numCharactersAfterProgram = 1;
	psp->commandLine[0] = 0x0D;

	// Setup initial registers
	PDS_CS = exeLoadSegment + header->initialCS;
	PDS_EIP = header->initialIP;
    PDS_SegBase[0] = PDS_CS * 16;
	PDS_SS = exeLoadSegment + header->initialSS;
	PDS_ESP = header->initialSP;
    PDS_SegBase[5] = PDS_SS * 16;

	PDS_EAX = 0x0000;
	PDS_DS = 0xFF0;
    PDS_SegBase[1] = PDS_DS * 16;
	PDS_ES = 0xFF0;
    PDS_SegBase[2] = PDS_ES * 16;

	// Interrupt vector redirects
	for (int a = 0; a < 256; a++)
	{
		PDS_Ram[a * 4 + 0] = a*3;
		PDS_Ram[a * 4 + 1] = (a*3 + intRedirectAddress)>>8;
		PDS_Ram[a * 4 + 2] = 0x00;
		PDS_Ram[a * 4 + 3] = 0x00;

		PDS_Ram[intRedirectAddress + a * 3 + 0] = 0xD8;		// coprocessor escape (which i don't implement, so will vector to missing instruction)
		PDS_Ram[intRedirectAddress + a * 3 + 1] = a;		// original vector
		PDS_Ram[intRedirectAddress + a * 3 + 2] = 0xCF;		// IRET
	}

	uint16_t* BDA = (uint16_t*) (PDS_Ram + 0x413);
	*BDA = 0xA0000 / 1024;		// Memory size 
	BDA = (uint16_t*) (PDS_Ram + 0x463);
	*BDA = 0x3D4;				// Base Port for 6845 CRT (colour)
	BDA = (uint16_t*) (PDS_Ram + 0x408);
	*BDA = 0x3BF;				// Base Port for LPT1
	BDA = (uint16_t*) (PDS_Ram + 0x410);
	*BDA = 0x422C;				// 1 drive 80x25 color initial mode 11 (64k normal or mouse and unused), 1 parallel, 1 serial
}

enum FCBOffset
{
	Extended=-7,					// byte
	FileAttributeExt=-1,			// byte
	DriveNumber=0,					// byte
	Filename=1,						// byte[8]
	Extension=9,					// byte[3]
	CurrentBlockNum=0x0C,			// word
	LogicalRecordSizeBytes=0x0E,	// word
	FileSizeBytes=0x10,				// dword
	LastUpdatedDate=0x14,			// word
	LastWriteTime=0x16,				// word
	VersionSpecific=0x18,			// 8bytes
};

uint8_t GetFCBByte(uint32_t fcbAddress, enum FCBOffset offset)
{
	if (PDS_Ram[fcbAddress] == 0xFF)
		return PDS_Ram[fcbAddress + 7 + offset];
	return PDS_Ram[fcbAddress + offset];
}

const char* GetFCBFilename(uint32_t fcbAddress)
{
	static char fileBuffer[8+3+1];
	int c = 0;
	for (int a = 0; a < 8 + 3; a++)
	{
		uint8_t b = GetFCBByte(fcbAddress, Filename+a);
		if (b == 32)
		{
			if (a < 8)
				a = 8;
			else
				a = 8 + 3;
		}
		else
			fileBuffer[c++] = b;
	}
	fileBuffer[c] = 0;
	return fileBuffer;
}

//DECLARE	EFLAGS[32]	ALIAS	%00000000000000:VM[1]:RF[1]:%0:NT[1]:IOPL[2]:OF[1]:DF[1]:I[1]:TF[1]:SF[1]:ZF[1]:%0:AF[1]:%0:PF[1]:%1:CF[1];

uint16_t* GetStackWordAddress(uint32_t offset)
{
	offset += PDS_SS * 16 + (PDS_ESP & 0xFFFF);
	return (uint16_t*) (PDS_Ram + offset);
}

void ClearCarry()
{
	uint16_t* ptr = GetStackWordAddress(4);	// FLAGS
	*ptr &= 0xFFFE;
}

void SetCarry()
{
	uint16_t* ptr = GetStackWordAddress(4);	// FLAGS
	*ptr |= 0x0001;
}

enum DOSError
{
	InvalidFunctionNumber=1,
	FileNotFound=2,
	PathNotFound=3,
	TooManyOpenFiles=4,
	AccessDenied=5,
	InvalidHandle=6,
	MemoryControlBlocksDestroyed=7,
	InsufficientMemory=8,
	InvalidMemoryBlockAddress=9,
	InvalidEnvironment=10,
	InvalidFormat=11,
	InvalidAccessMode=12,
	InvalidDrive=15
};

void DOSError(enum DOSError errorCode)
{
	SetCarry();
	PDS_EAX = errorCode;
}

void IOCTL(uint8_t functionNumber)
{
	uint8_t BL = PDS_EBX & 0xFF;
	switch (functionNumber)
	{
	case 0x08:		// Device Removable Query
		if (BL == 0 || BL == 1)
		{
			ClearCarry();
			PDS_EAX = 0;	//removable
			PDSpause = 1;
		}
		else
		{
			DOSError(InvalidDrive);
		}
		break;
	default:
		printf("Unimplemented IOCTL Function %02X\n", functionNumber);
		DBG_BREAK;//unhandled vector
		break;
	}
}

void SYSTEM_Values(uint8_t functionNumber)
{
	uint8_t DL = PDS_EDX & 0xFF;
	switch (functionNumber)
	{
	case 0x00:	// Get CTRL-Break checking flag
		PDS_EDX &= 0xFF00;	// break check off
		break;
	case 0x01:	// Set CTRL-Btreak checking flag
		break;
	default:
		printf("Unimplemented SYSTEM_Values Function %02X\n", functionNumber);
		DBG_BREAK;//unhandled vector
		break;
	}
}


void DOS_Function(uint8_t functionNumber)
{
	uint8_t AL = PDS_EAX & 0xFF;
	uint16_t DS = PDS_DS;
	uint16_t DX = PDS_EDX & 0xFFFF;
	uint8_t DL = PDS_EDX & 0xFF;
	uint16_t SI = PDS_ESI & 0xFFFF;
	uint32_t addrDSDX = DS * 16 + DX;
	uint32_t addrDSSI = DS * 16 + SI;
	switch (functionNumber)
	{
	case 0x09:		// Print String
		while (PDS_Ram[addrDSDX] != '$')
		{
			putchar(PDS_Ram[addrDSDX++]);
		}
		break;
	case 0x0D:		// Reset Disk
		// Flush all buffers to disks, does not update directory entry
		break;
	case 0x0E:		// Select Disk
		printf("Selected disk %c:\n", DL + 'A');
		PDS_EAX &= 0xFF00;
		PDS_EAX |= 0x01;		// one drive installed
		break;
	case 0x11:		// Search for first entry using FCB
		printf("Search : Drive %d Filename %s\n", GetFCBByte(addrDSDX, DriveNumber), GetFCBFilename(addrDSDX));
		//for now return 0
		PDS_EAX |= 0x00FF;		// nothing found
		break;
	case 0x19:		// Get Current Default Drive Number
		PDS_EAX &= 0xFF00;	// drive = 0 (A)
		break;
	case 0x1A:		// Set Disk Transfer Address
		printf("DTA : %08X\n", addrDSDX);
		break;
	case 0x25:		// Set Interrupt Vector
		printf("Interrupt Vector : %02X <- %04X:%04X\n", AL, DS, DX);
		PDS_Ram[AL * 4 + 0] = DX & 0xFF;
		PDS_Ram[AL * 4 + 1] = DX >> 8;
		PDS_Ram[AL * 4 + 2] = DS & 0xFF;
		PDS_Ram[AL * 4 + 3] = DS >> 8;
		break;
	case 0x2A:		// Get Date
	{
		time_t tme;
		struct tm* t;
		tme = time(NULL);
		t = localtime(&tme);
		PDS_EAX &= 0xFF00;
		PDS_EAX |= t->tm_wday;
		PDS_ECX = 1993; // t->tm_year + 1900;
		PDS_EDX = ((t->tm_mon+1) << 8) | (t->tm_mday);
		//TODO update BDA
		break;
	}
	case 0x2C:		// Get Time
	{
		time_t tme;
		struct tm* t;
		tme = time(NULL);
		t = localtime(&tme);
		PDS_ECX = ((t->tm_hour)<<8)|t->tm_min;
		PDS_EDX = ((t->tm_sec) << 8) | 0x00;
		break;
	}
	case 0x2E:		// Set/Reset Verify Flag
		printf("Verify set to %s\n", AL == 0 ? "off" : "on");
		break;
	case 0x30:		// Get DOS Version
		PDS_EAX = 0x0003;		// Version 4
		PDS_EBX = 0xFF00;		// MSDOS
		PDS_ECX = 0x0000;
		break;
	case 0x33:		// Get/Set System Values
		SYSTEM_Values(AL);
		break;
	case 0x36:		// Get Disk Free Space
		if (DL == 0 || DL == 1)
		{
			PDS_EAX = 2;		// sectors per cluster
			PDS_EBX = 1024;		// available clusters
			PDS_ECX = 512;		// bytes per sector
			PDS_EDX = 1024;		// clusters per drive
		}
		else
		{
			PDS_EAX = 0xFFFF;
			PDSpause = 1;
		}
		break;
	case 0x3B:		// Change current directory
		printf("CHDIR : %s\n", &PDS_Ram[addrDSDX]);
		if (strncmp(&PDS_Ram[addrDSDX], "A:\\", 3) == 0)
		{
			ClearCarry();
		}
		else
		{
			DOSError(PathNotFound);
			PDSpause = 1;
		}
		break;
	case 0x3D:		// Open File return handle
		printf("Open File : %s  %s (%02X)", &PDS_Ram[addrDSDX], AL == 0 ? "for read" : (AL == 1 ? "for write" : "read/write"), AL);
		if (AL == 0)
		{
			DOSError(FileNotFound);
		}
		else
		{
			ClearCarry();
			PDS_EAX = 5;		// temporary handle.. we should implement file stuffs
			PDSpause = 1;
		}
		break;
	case 0x44:		// IOCTRL
		IOCTL(AL);
		break;
	case 0x47:		// Get Current Directory
		ClearCarry();
		PDS_EAX = 0;	// Error code if carry
		PDS_Ram[addrDSSI] = 0;	// A:/
		break;
	case 0x54:		// Get Verify Flag
		PDS_EAX &= 0xFF00;	// verify off
		break;
	default:
		printf("Unimplemented DOS Function %02X\n", functionNumber);
		DBG_BREAK;//unhandled vector
		break;
	}
}

void VIDEO_Function(uint8_t functionNumber)
{
	uint8_t AL = PDS_EAX & 0xFF;
	uint16_t DS = PDS_DS;
	uint16_t DX = PDS_EDX & 0xFFFF;

	switch (functionNumber)
	{
	case 0x00:		// Set Video Mode (AL)
		if (AL != 3)
		{
			printf("Request for unimplemented video mode\n");
			DBG_BREAK;
		}
		//MODE 3  = 80x25 16 colour text
		break;
	case 0x01:		// Cursor Kind
		break;
	case 0x0F:		// Get current video state
		PDS_EAX = 0x5003;	// 80 col, mode 3
		PDS_EBX &= 0xFF;	// page 0
		break;
	default:
		printf("Unimplemented VIDEO Function %02X\n", functionNumber);
		DBG_BREAK;//unhandled vector
		break;
	}
}


void DOS_VECTOR_TRAP(uint8_t vector)
{
	switch (vector)
	{
	case 0x08:
		// Timer intterrupt - gets redirected by pds eventually
		break;
	case 0x10:
		VIDEO_Function((PDS_EAX >> 8) & 0xFF);
		break;
	case 0x21:
		DOS_Function((PDS_EAX >> 8) & 0xFF);
		break;

	default:
		printf("Unimplemented Interupt Vector Trap %02X\n", vector);
		DBG_BREAK;//unhandled vector
		break;
	}
}


void RenderVideo();

const int CLOCK_WAIT = 10000;

uint8_t* PSF_FONT = NULL;
size_t PSF_FONT_Size = 0;

int PSF_Load(const char* filename)
{
	size_t expectedSize=0;
	FILE* inFile = fopen(filename,"rb");
	if (inFile == NULL)
	{
		printf("Failed to read from %s\n", filename);
		return 1;
	}
	fseek(inFile,0,SEEK_END);
	expectedSize=ftell(inFile);
	fseek(inFile,0,SEEK_SET);
	
	PSF_FONT_Size = expectedSize;
	if ((PSF_FONT_Size & 1) == 1)
		PSF_FONT_Size++;				// multiple of 2 so checksum is simple

	PSF_FONT = malloc(PSF_FONT_Size);

	memset(PSF_FONT, 0, PSF_FONT_Size);

	expectedSize -= fread(PSF_FONT, 1, expectedSize, inFile);
	if (expectedSize != 0)
	{
		printf("Failed to read from %s\n", filename);
		free(PSF_FONT);
		PSF_FONT = NULL;
		return 1;
	}

	fclose(inFile);

	return 0;
}

void RenderGlyph(int x, int y, uint8_t glyph, uint32_t ink, uint32_t paper)
{
	uint32_t* gfx = (uint32_t*) (videoMemory[MAIN_WINDOW]);
	gfx += y * 640 + x;
	uint8_t* ptr = PSF_FONT + 4;	// skip header, font is hardwired here
	ptr += glyph * 8;				// get to bitmap location
	for (int yy = 0; yy < 8; yy++)
	{
		uint8_t bitmap = *ptr++;
		for (int xx = 0; xx < 8; xx++)
		{
			if (bitmap & 0x80)
				*gfx = ink;
			else
				*gfx = paper;
			bitmap <<= 1;
			gfx++;
		}
		gfx += 640 - 8;
	}
}

uint32_t EGA_Palette[16] = { 0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
							 0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
						     0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
							 0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF};

uint8_t flashTime = 0;
void RenderVideo()
{
	flashTime--;
	uint8_t* videoMemory = &PDS_Ram[0xB8000];
	for (int row = 0; row < 25; row++)
	{
		for (int col = 0; col < 80; col++)
		{
			uint8_t glyph = *videoMemory++;
			uint8_t attr = *videoMemory++;
			int ink = attr & 0xF;
			int paper = (attr >> 4) & 0x7;
			int flash = attr & 0x80;
			if (flash)
			{
				if (flashTime < 128)
				{
					int t = paper;
					paper = ink;
					ink = t;
				}
			}

			RenderGlyph(col * 8, row * 8, glyph, EGA_Palette[ink], EGA_Palette[paper]);
		}
	}
}

void PDS_Tick()
{
	int clksDelay = CLOCK_WAIT;
	int timerDelay = CLOCK_WAIT*3;
	while (1)
	{
/*		if (PDS_GETPHYSICAL_EIP() == 0x1FC8A)
			PDSpause = 1;*/
		if (PDSpause)
		{
			DebugIt();
			getchar();
		}
		PDS_STEP();
		clksDelay--;
		timerDelay--;
		if (timerDelay == 0)
		{
			timerDelay = CLOCK_WAIT*3;
			PDS_INTERRUPT(0x08);
		}
		if (clksDelay == 0)
		{
			clksDelay = CLOCK_WAIT;

			RenderVideo();

			VideoUpdate(0);
			VideoWait(0.02f);



			if (PDS_keyBufferRead != PDS_keyBufferWrite)
			{
				PDS_INTERRUPT(0x09);
			}
		}
	}
}


void PDS_Main()
{
	videoMemory[MAIN_WINDOW] = (unsigned char*)malloc(640*200*sizeof(unsigned int));
	VideoInitialise();
	VideoCreate(640, 200, 2, 4, "PDS", 0);
	PDS_Keys();

	PSF_Load("C:\\Users\\savou\\Downloads\\PDS\\PDS_executables\\BM.PSF");		//Extracted from IBM-EGA8x8.FON from old school font pack

	PDS_LoadEXE("C:\\Users\\savou\\Downloads\\PDS\\PDS_executables\\version121_pdsz80.exe");
	//PDS_LoadEXE("C:\\Users\\savou\\Downloads\\PDS\\PDS_executables\\P89.exe");
	PDS_Setup();
	PDS_Tick();
}


