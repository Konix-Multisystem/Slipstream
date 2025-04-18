#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <string.h>
#include <time.h>

#include "GLFW/glfw3.h"

#include "../disasm.h"
#include "../system.h"
#include "../memory.h"
#include "../debugger.h"
#include "../host/video.h"
#include "pds.h"

#if OS_WINDOWS
#include "../host/dirent_windows.h"
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define PDS_INTERCEPT_DIRECT 0		//experiment to see if i can "pretend" the comms 

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
			//printf("KeyCode Logged : %02X\n", scanCodeNum);
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
				//printf("Extended KeyCode Logged : 0xE0 %02X\n", scanCodeNum);
				PDS_keyBuffer[PDS_keyBufferWrite++] = scanCodeNum;
			}
		}
	}
}

extern GLFWwindow *windows[MAX_WINDOWS];
int PDS_WINDOW = -1;
void PDS_Keys()
{
	glfwSetKeyCallback(windows[PDS_WINDOW], PDS_kbHandler);
}


/*--------------------------*/

/*

 Not a full dos emulator, more a crude layer to allow running the pds software and using it to assemble things...

*/

int PDSpause = 0;

const uint32_t exeLoadSegment = 0x1000;
const uint32_t intRedirectAddress = 0x500;


char* RootDisk = NULL;

uint8_t* PDS_EXE = NULL;
size_t PDS_EXE_Size = 0;

uint32_t DiskTransferAddress = 0xFF80;

uint8_t PDS_Ram[1 * 1024 * 1024 + 65536];

uint8_t PDS_HOST_CTRL;
uint8_t PDS_HOST_PORTA;
uint8_t PDS_HOST_PORTB;
uint8_t PDS_HOST_PORTC;
extern uint8_t PDS_CLIENT_DATA;
extern uint8_t PDS_CLIENT_COMMS;
extern uint8_t PDS_CLIENT_CTRLA;
extern uint8_t PDS_CLIENT_CTRLB;

#if OS_WINDOWS
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
		case 0x17:	// KBD flag byte 0
		case 0x18:	// KBD flag byte 1
			break;
		case 0x85:	// point height of char matrix LSB
			return 8;
		case 0x86:	// point height of char matrix MSB
			return 0;
		default:
			printf("BDA Read Access unknown @%04X", addr);
			DBG_BREAK;
			break;
		}
	}
	return PDS_Ram[addr&0xFFFFF];
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
		case 0x17:	// Kbd Flag
		case 0x18:
			break;
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
	case 0x0300:	
		if (PDS_HOST_CTRL & 0x10)
		{
			ret = PDS_CLIENT_DATA;
		}
		else
		{
			ret = PDS_HOST_PORTA;
		}
		break;
	case 0x0302:	// dunno at present
		if (PDS_HOST_CTRL & 0x02)
		{
			ret = PDS_CLIENT_DATA;
		}
		else
		{
			ret = PDS_HOST_PORTB;
		}
		break;
	case 0x0304:	// PortC
		{
			ret = PDS_HOST_PORTC;

			ret &= 0xEF;
			ret |= (PDS_CLIENT_COMMS & 0x80)>>3;
		}
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

int state = 0;

uint16_t segment;
uint16_t offset;
uint16_t dontcare;
uint16_t size;

uint8_t fl1_cur_bank0=0x10;
uint8_t fl1_cur_bank1=0x11;
uint8_t fl1_cur_bank2=0x12;
uint8_t fl1_cur_bank3=0x13;
uint16_t fl1_start;
uint16_t fl1_length;

#define SAVE_AS_ROM	1

#if SAVE_AS_ROM
FILE* outputRom = NULL;
const char outFileName[65536];
#endif

// TO MOVE TO CLIENT SIDE lib
void PDS_Recieve(uint8_t byte)
{
#if SAVE_AS_ROM
	uint8_t outByte;
#endif
	if (curSystem == ESS_P88)
	{
		switch (state)
		{
		case 0:	// Waiting for command
			if (byte == 0xC8)
				state = 100;
			else if (byte == 0xCA)
				state = 200;
			else if (byte == 0xB3)
				printf("PDS_dummyByte? B3\n");
			else
				printf("PDS_Unknown command : %02X\n", byte);
			break;

			// DOWNLOAD DATA
		case 100:	// SegmentLo
			segment = byte;
			state++;
			break;
		case 101:	// SegmentHi
			segment = (segment & 0x00FF) | (byte << 8);
			state++;
			break;
		case 102:	// OffsetLo
			offset = byte;
			state++;
			break;
		case 103:	// OffsetHi
			offset = (offset & 0x00FF) | (byte << 8);
			state++;
			break;
		case 104:	// unknown
		case 105:
			if (byte != 0)
			{
				printf("Unknown PDS Data Byte : %d - %02X", state, byte);
			}
			state++;
			break;
		case 106:	// SizeLo
			size = byte;
			state++;
			break;
		case 107:	// SizeHi
			size = (size & 0x00FF) | (byte << 8);
			state++;
			printf("Load Section : %04X:%04X %04X\n", segment, offset, size);
			break;
		case 108:	// data
			size--;
			SetByte(segment * 16 + offset, byte);
			if (offset == 0xFFFF)
			{
				printf("Offset wrap ... ");
			}
			offset++;
			if (size == 0)
				state = 0;
			break;
			// EXECUTE SECTION
		case 200:	// SegmentLo
			segment = byte;
			state++;
			break;
		case 201:	// SegmentHi
			segment = (segment & 0x00FF) | (byte << 8);
			state++;
			break;
		case 202:	// OffsetLo
			offset = byte;
			state++;
			break;
		case 203:	// OffsetHi
			offset = (offset & 0x00FF) | (byte << 8);
			state = 0;
			printf("Execute Section : %04X:%04X\n", segment, offset);
			CS = segment;
			IP = offset;
			break;
		}
	}
	else if (curSystem == ESS_FL1)
	{
		// Initial bank layouts for PDS
		// 3 = 13H - Only bank that gets switched
		// 2 = 12H
		// 1 = 11H
		// 0 = 10H

		switch (state)
		{
		case 0:	// Waiting for command

			switch (byte)
			{
			case 179:	//PAD
				break;
			case 180:		// Download block
				state = 200;
				break;
			case 181:		// Jump to address
				state = 300;
				break;
			case 183:		// Select Bank
				state = 100;
				break;
			case 0xFF:		// Dummy At Startup
#if SAVE_AS_ROM
				sprintf(outFileName, "%s%s", RootDisk, "OUT.FL1");
				outputRom = fopen(outFileName, "wb");
				outByte = 0xF1;	// F1 ROM 
				fwrite(&outByte, 1, 1, outputRom);
#endif
				break;
			case 0x8B:		// Dummy At Startup
				break;
			default:
				printf("UNIMPLEMENTED");
				break;
			}
			/*if (byte == 0xC8)
				state = 100;
			else if (byte == 0xCA)
				state = 200;
			else if (byte == 0xB3)
				printf("PDS_dummyByte? B3\n");
			else
				printf("PDS_Unknown command : %02X\n", byte);*/
			break;

		case 100:	// select bank
			fl1_cur_bank3 = byte;
			state = 0;
			break;

		case 200:	// download 
			fl1_start = byte << 8;
			state++;
			break;
		case 201:
			fl1_start |= byte;
			state++;
			break;
		case 202:
			fl1_length = byte << 8;
			state++;
			break;
		case 203:
			fl1_length |= byte;
#if SAVE_AS_ROM
			outByte = 0xC8;
			fwrite(&outByte, 1, 1, outputRom);

			// Compute Linear Address From Bank and fl1_start
			{
				uint32_t address;
				uint32_t bnk = fl1_start / 16384;
				switch (bnk)
				{
				case 0:
					address = fl1_cur_bank0 * 16384;
					break;
				case 1:
					address = fl1_cur_bank1 * 16384;
					break;
				case 2:
					address = fl1_cur_bank2 * 16384;
					break;
				case 3:
					address = fl1_cur_bank3 * 16384;
					break;
				}
				address += fl1_start & 16383;
				uint16_t seg = (address >> 4)&0xF000;
				uint16_t off = address & 0xFFFF;
				fwrite(&seg, 1, 2, outputRom);
				fwrite(&off, 1, 2, outputRom);
				seg = 0;
				off = fl1_length;
				fwrite(&seg, 1, 2, outputRom);
				fwrite(&off, 1, 2, outputRom);
			}
#endif
			state++;
			break;
		case 204:
#if SAVE_AS_ROM
			fwrite(&byte, 1, 1, outputRom);
#endif
			fl1_length--;
			fl1_start++;
			if (fl1_length == 0)
				state = 0;
			break;

		case 300:	// download 
			fl1_start = byte << 8;
			state++;
			break;
		case 301:
			fl1_start |= byte;
#if SAVE_AS_ROM
			outByte = 0xCA;
			fwrite(&outByte, 1, 1, outputRom);

			// Compute Linear Address From Bank and fl1_start
			{
				uint32_t address;
				uint32_t bnk = fl1_start / 16384;
				switch (bnk)
				{
				case 0:
					address = fl1_cur_bank0 * 16384;
					break;
				case 1:
					address = fl1_cur_bank1 * 16384;
					break;
				case 2:
					address = fl1_cur_bank2 * 16384;
					break;
				case 3:
					address = fl1_cur_bank3 * 16384;
					break;
				}
				address += fl1_start & (16384-1);
				uint16_t seg = (address >> 4)&0xF000;
				uint16_t off = address & 0xFFFF;
				fwrite(&seg, 1, 2, outputRom);
				fwrite(&off, 1, 2, outputRom);
				fclose(outputRom);
			}
#endif
			state=0;
			break;
		}
	}
	else
	{
		printf("WARNING: PDS CLIENT LIB MISSING IMPLEMENTATION FOR SYSTEM : %d\n", curSystem);
	}
}

static int initialSync = 0;
static uint8_t byteFromPDS = 0;
int PDS_GetAByte()
{
	uint8_t comms = PDS_HOST_PORTC & 0x01;	// clk bit from host
	comms = comms^ initialSync;
	if (comms & 1)
		return 0;
	byteFromPDS = PDS_HOST_PORTB;
	PDS_CLIENT_COMMS = initialSync;
	initialSync ^= 129;
	return 1;
}


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
		//printf("PIT Counter 0 : %02X\n", byte);
		break;
	case 0x0042:	// PIT - counter 2, casette/speaker (BEEP)
		//printf("PIT Counter 2 : %02X\n", byte);
		break;
	case 0x0043:	// PIT - Mode Register
		{
			const char* counters[4] = { "0","1","2","ERR" };
			const char* latch[4] = { "counter latch command", "r/w counter bits 0-7 only","r/w counter bits 8-15 only","r/w counter bits 0-7 then 8-15" };
			const char* mode[8] = { "mode 0 select","one shot","rate generator","square wave","soft strobe","hard strobe","rate generator","square wave" };
			const char* type[2] = { "binary counter 16 bits","BCD counter" };

			//printf("PIT Mode - %02X  (counter %s | %s | mode %s | %s)\n", byte, counters[byte >> 6], latch[(byte >> 4) & 0x3], mode[(byte >> 1) & 0x7], type[byte & 1]);
		}
		break;
	case 0x0061:	// Kb control
		if (PDS_keyBufferRead!=PDS_keyBufferWrite && (KB_Control & 0x80) && ((byte & 0x80) == 0))
		{
			PDS_keyBufferRead++;
		}
		KB_Control = byte;
		return;
	case 0x0302:	// PDS Port B
		//printf("PrtB : %02X\n", byte);
		PDS_HOST_PORTB = byte;
		break;
	case 0x0304:	// PDS Port C
		//printf("PrtC : %02X\n", byte);
		PDS_HOST_PORTC = byte;
#if PDS_INTERCEPT_DIRECT
		if (PDS_GetAByte())
		{
			//printf("PDS_DATA_VALUE : %02X\n", byteFromPDS);
			PDS_Recieve(byteFromPDS);
		}
#endif
		break;
	case 0x0306:	// PDS CTRL
		//printf("Ctrl : %02X\n", byte);
		PDS_HOST_CTRL = byte;
		break;
	case 0x03D4:	// CGA Index Register
	{
		if (byte > 0x11)
			DBG_BREAK;	// BAD Index
		CGA_Index = byte;
		//printf("CGA Video Register Index Set : %s\n", CGA_IndexNames[byte]);
		break;
	}
	case 0x03D5:	// CGA Data Register
		//printf("CGA Video Register Data Set : %s (%04X)<-%02X\n", CGA_IndexNames[CGA_Index],CGA_Index, byte);
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

MSVC_PACK_BEGIN;
struct GCC_PACK DTA
{
	uint8_t		attributeOfSearch;					// 00
	uint8_t		driveUsedInSearch;					// 01
	uint8_t		searchName[11];						// 02
	uint16_t	directoryEntryNumber;				// 0D
	uint16_t	startingDirectoryClusterNumberV3;	// 0F
	uint16_t	reserved;							// 11
	uint16_t	startingDirectoryClusterNumberV2;	// 13
	uint8_t		attributeOfMatchingFile;			// 15
	uint16_t	fileTime;							// 16
	uint16_t	fileDate;							// 18
	uint32_t	fileSize;							// 1A
	uint8_t		filenameFound[13];					// 1E
};
MSVC_PACK_END;

void FetchRegistersPDS(char* tmp)
{
	if (1)
	{
		sprintf(tmp, "--------\nFLAGS = O  D  I  T  S  Z  -  A  -  P  -  C\n        %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s\nEAX= %08X\nEBX= %08X\nECX= %08X\nEDX= %08X\nESP= %08X\nEBP= %08X\nESI= %08X\nEDI= %08X\nCS= %04X\nDS= %04X\nES= %04X\nSS= %04X\nFS= %04X\nGS= %04X\n\nEIP= %08X\n\nCR0= %08X\nDR7= %08X\n--------\n",
			PDS_EFLAGS & 0x800 ? "1" : "0",
			PDS_EFLAGS & 0x400 ? "1" : "0",
			PDS_EFLAGS & 0x200 ? "1" : "0",
			PDS_EFLAGS & 0x100 ? "1" : "0",
			PDS_EFLAGS & 0x080 ? "1" : "0",
			PDS_EFLAGS & 0x040 ? "1" : "0",
			PDS_EFLAGS & 0x020 ? "1" : "0",
			PDS_EFLAGS & 0x010 ? "1" : "0",
			PDS_EFLAGS & 0x008 ? "1" : "0",
			PDS_EFLAGS & 0x004 ? "1" : "0",
			PDS_EFLAGS & 0x002 ? "1" : "0",
			PDS_EFLAGS & 0x001 ? "1" : "0",
			PDS_EAX, PDS_EBX, PDS_ECX, PDS_EDX, PDS_ESP, PDS_EBP, PDS_ESI, PDS_EDI, PDS_CS, PDS_DS, PDS_ES, PDS_SS, PDS_FS, PDS_GS, PDS_EIP, PDS_CR0, PDS_DR7);
	}
	else
	{
		sprintf(tmp, "--------\nFLAGS = O  D  I  T  S  Z  -  A  -  P  -  C\n        %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s\nAX= %04X\nBX= %04X\nCX= %04X\nDX= %04X\nSP= %04X\nBP= %04X\nSI= %04X\nDI= %04X\nCS= %04X\nDS= %04X\nES= %04X\nSS= %04X\n--------\n",
			PDS_EFLAGS & 0x800 ? "1" : "0",
			PDS_EFLAGS & 0x400 ? "1" : "0",
			PDS_EFLAGS & 0x200 ? "1" : "0",
			PDS_EFLAGS & 0x100 ? "1" : "0",
			PDS_EFLAGS & 0x080 ? "1" : "0",
			PDS_EFLAGS & 0x040 ? "1" : "0",
			PDS_EFLAGS & 0x020 ? "1" : "0",
			PDS_EFLAGS & 0x010 ? "1" : "0",
			PDS_EFLAGS & 0x008 ? "1" : "0",
			PDS_EFLAGS & 0x004 ? "1" : "0",
			PDS_EFLAGS & 0x002 ? "1" : "0",
			PDS_EFLAGS & 0x001 ? "1" : "0",
			PDS_EAX & 0xFFFF, PDS_EBX & 0xFFFF, PDS_ECX & 0xFFFF, PDS_EDX & 0xFFFF, PDS_ESP & 0xFFFF, PDS_EBP & 0xFFFF, PDS_ESI & 0xFFFF, PDS_EDI & 0xFFFF, PDS_CS, PDS_DS, PDS_ES, PDS_SS);
	}
}

unsigned int FetchOneDisassemblePDS(char* tmp, uint32_t address)
{
	int a;
	char tBuffer[1024];

	*tmp = 0;
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

	return disMe.bytesRead;
}

void FetchDisassemblePDS(char* tmp)
{
	uint32_t address = PDS_GETPHYSICAL_EIP();
	for (int a = 0; a < 20; a++)
	{
		address += FetchOneDisassemblePDS(tmp, address);
		tmp += strlen(tmp);
	}
}


void PDS_DebugIt()
{
	char blah[65536] = { 0 };

	FetchRegistersPDS(blah);
	printf("%s",blah);
	blah[0] = 0;

	FetchOneDisassemblePDS(blah, PDS_GETPHYSICAL_EIP());
	printf("%s",blah);
}


void PDS_Setup(const char* commandLine)
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
	psp->numCharactersAfterProgram = strlen(commandLine);
	strcpy(psp->commandLine, commandLine);
//	strcat(psp->commandLine, "\n");

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
	BDA = (uint16_t*) (PDS_Ram + 0x417);
	*BDA = 0x0000;				// kbd flag bytees
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
	if (offset < 0)
		return 0;	// only applies to attribute basically
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


/*
	|7|6|5|4|3|2|1|0| Directory Attribute Flags
	 | | | | | | | `--- 1 = read only
	 | | | | | | `---- 1 = hidden
	 | | | | | `----- 1 = system
	 | | | | `------ 1 = volume label  (exclusive)
	 | | | `------- 1 = subdirectory
	 | | `-------- 1 = archive
	 `----------- unused
	 */

#define MAX_EMU_HANDLES	(16)
FILE* handles[MAX_EMU_HANDLES] = { 0 };

#if OS_WINDOWS
#define NCASE_CMP	strnicmp
#else
#define NCASE_CMP	strncasecmp
#endif

extern int pause;

void PDS_FileName(char* dstPath, const char* filename)
{
	if (filename[0] == 'A' && filename[1] == ':' && filename[2] == '\\')
		filename += 3;
	else if (filename[0] == 'A' && filename[1] == ':')
		filename += 2;

	if (NCASE_CMP(filename, "D:\\PDS\\WORK\\", 12) == 0)
		filename += 12;
	if (NCASE_CMP(filename, "\\PDS\\WORK\\", 10) == 0)
		filename += 10;
	if (NCASE_CMP(filename, "..\\", 3) == 0)
		filename += 3;

	sprintf(dstPath, "%s%s", RootDisk, filename);

	char* killSpace = dstPath + strlen(dstPath)-1;
	while (killSpace > dstPath)
	{
		if (*killSpace == ' ')
			*killSpace-- = 0;
		else
			break;
	}
}

int PDS_FileDelete(const char* filename, uint8_t mask)
{
	char path[2048];
	struct DTA* dta = &PDS_Ram[DiskTransferAddress];
	struct stat status;
	int statusOk;
		
	printf("Delete File : '%s'  (%02X)\n", filename, mask);

	PDS_FileName(path, filename);

	// Should we delete though ?
	struct stat buffer;
	int exist = stat(path, &buffer);
	if (exist)
	{
		return 0;
	}

	return -FileNotFound;
}

int PDS_FileOpen(const char* filename, uint8_t kind)
{
	char path[2048];
	struct DTA* dta = &PDS_Ram[DiskTransferAddress];
	struct stat status;
	int statusOk;
		
	printf("Open File : '%s'  %s (%02X)\n", filename, (kind&3) == 0 ? "for read" : ((kind&3) == 1 ? "for write" : "read/write"), kind);
	
	PDS_FileName(path, filename);

	int a = 2;
	for (; a < MAX_EMU_HANDLES; a++)
	{
		if (handles[a] == NULL)
			break;
	}

	if (a >= MAX_EMU_HANDLES)
		return -TooManyOpenFiles;

	handles[a] = fopen(path, "rb");
	if (handles[a])
	{
		printf("Opened %04X\n", a);
		return a;
	}

	return -FileNotFound;
}

int PDS_CloseHandle(uint16_t handle)
{
	printf("Close File : %04X\n", handle);

	if (handle > MAX_EMU_HANDLES || handles[handle] == NULL)
	{
		return -InvalidHandle;
	}
	fclose(handles[handle]);
	handles[handle] = NULL;
	return 0;
}

int PDS_FileRead(uint32_t destination, uint16_t size, uint16_t handle)
{
	printf("Read File : %04X  (%04X) to (%08X)\n", handle, size, destination);
	if (handle > MAX_EMU_HANDLES || handles[handle] == NULL)
	{
		return -InvalidHandle;
	}

	int read = 0;
	while (size > 0)
	{
		uint8_t b;
		if (1 != fread(&b, 1, 1, handles[handle]))
		{
			return read;
		}
		PDS_SetByte(destination++, b);
		size--;
		read++;
	}
	return read;
}

int PDS_GetFileDateTime(uint16_t* time, uint16_t* date, uint16_t handle)
{
	printf("Get File Data Time: %04X\n", handle);
	struct stat status;
	struct tm* t;

	if (handle > MAX_EMU_HANDLES || handles[handle] == NULL)
	{
		return -InvalidHandle;
	}

	int fd = fileno(handles[handle]);
	if (fstat(fd, &status) != 0)
		return -FileNotFound;

	t = localtime(&status.st_mtime);
	*date = ((1993 - 1980) << 9) | ((t->tm_mon + 1) << 5) | ((t->tm_mday) << 0);
	*time = ((t->tm_hour) << 11) | ((t->tm_min) << 5) | (t->tm_sec & 0x1E);

	return 0;
}

int PDS_FileSeek(uint8_t origin, uint16_t numHi, uint16_t numLo, uint16_t handle)
{
	printf("Seek File : %04X  (%04X%04X) (%02X)\n", handle, numHi,numLo, origin);
	struct stat status;
	struct tm* t;

	if (handle > MAX_EMU_HANDLES || handles[handle] == NULL)
	{
		return -InvalidHandle;
	}

	int seekOrigin = SEEK_SET;
	if (origin == 1)
		seekOrigin = SEEK_CUR;
	if (origin == 2)
		seekOrigin = SEEK_END;

	int move = (numHi << 16) + numLo;
	if (fseek(handles[handle], move, seekOrigin) != 0)
		return -FileNotFound;
	long result = ftell(handles[handle]);
	if (result < 0)
		return -FileNotFound;

	return result & 0xFFFFFFFF;
}

void CreateFXCBFromEntry(uint8_t searchAttr, const char* searchName, struct dirent* entry)
{
	char path[2048];
	struct DTA* dta = &PDS_Ram[DiskTransferAddress];
	struct stat status;
	int statusOk;

	sprintf(path, "%s%s", RootDisk, entry->d_name);
	statusOk = stat(path, &status);

	dta->attributeOfMatchingFile = 0;
	if (entry->d_type == DT_DIR)
		dta->attributeOfMatchingFile = 0x10;
	dta->attributeOfSearch = searchAttr;
	dta->directoryEntryNumber = 0;
	dta->driveUsedInSearch = 0;
	strncpy(dta->searchName, searchName, 11);
	strncpy(dta->filenameFound, entry->d_name, 13);
	if (statusOk == 0)
	{
		struct tm* t;
		t = localtime(&status.st_mtime);
		dta->fileDate = ((1993 - 1980) << 9) | ((t->tm_mon+1) << 5) | ((t->tm_mday) << 0);
		dta->fileTime = ((t->tm_hour) << 11) | ((t->tm_min) << 5) | (t->tm_sec & 0x1E);
		dta->fileSize = status.st_size;
	}
	else
	{
		dta->fileDate = ((1993 - 1980) << 9) | ((1) << 5) | ((1) << 0);
		dta->fileTime = ((10) << 11) | ((30) << 5) | (30);
		dta->fileSize = 0;
	}
	dta->startingDirectoryClusterNumberV2 = 0;
	dta->startingDirectoryClusterNumberV3 = 0;

}

DIR* search = NULL;

int HasWildcard(const char* name)
{
	while (*name!=0)
	{
		if (*name == '?' || *name == '*')
			return 1;
		name++;
	}
	return 0;
}

int IsMatch(const char* searchName, const char* filename)
{
	if (HasWildcard(searchName))
	{
		if (NCASE_CMP(searchName, "???????????", 11) == 0)
			return 1;

		if (NCASE_CMP(searchName, "*.PRJ", 5) == 0)
		{
			if (NCASE_CMP(filename + strlen(filename) - 4, ".PRJ", 4) == 0)
				return 1;
			return 0;
		}
		DBG_BREAK;
		return 0;
	}

	if (NCASE_CMP(searchName, filename, strlen(searchName)) == 0)
		return 1;
	return 0;
}

uint8_t ContinueSearch(uint8_t searchAttribute, const char* filename)
{
	if (search)
	{
		// Get first entry
		struct dirent* entry;
		while (entry = readdir(search))
		{
			if (entry)
			{
				if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
					continue;

				if ((entry->d_type == DT_DIR) && (searchAttribute & 0x10) == 0)
					continue;

				if (IsMatch(filename, entry->d_name))
				{
					CreateFXCBFromEntry(searchAttribute, filename, entry);
					return 0;
				}
			}
		}
	}
	search = NULL;
	return 0xFF;
}

uint8_t StartSearch(uint8_t searchAttribute, const char* filename)
{
	search = opendir(RootDisk);

	return ContinueSearch(searchAttribute, filename);
}


void DOS_Function(uint8_t functionNumber)
{
	uint8_t AL = PDS_EAX & 0xFF;
	uint16_t DS = PDS_DS;
	uint16_t ES = PDS_ES;
	uint16_t BX = PDS_EBX & 0xFFFF;
	uint16_t CX = PDS_ECX & 0xFFFF;
	uint16_t DX = PDS_EDX & 0xFFFF;
	uint8_t CL = PDS_ECX & 0xFF;
	uint8_t DL = PDS_EDX & 0xFF;
	uint16_t SI = PDS_ESI & 0xFFFF;
	uint16_t DI = PDS_EDI & 0xFFFF;
	uint32_t addrDSDX = DS * 16 + DX;
	uint32_t addrDSSI = DS * 16 + SI;
	uint32_t addrESDI = ES * 16 + DI;
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
		printf("Start Search : Drive %d Filename '%s'\n", GetFCBByte(addrDSDX, DriveNumber), GetFCBFilename(addrDSDX));
		//for now return 0
		PDS_EAX &= 0xFF00;
		PDS_EAX = StartSearch(GetFCBByte(addrDSDX, FileAttributeExt), GetFCBFilename(addrDSDX));
		break;
	case 0x12:		// Find next?
		printf("Continue Search : Drive %d Filename '%s'\n", GetFCBByte(addrDSDX, DriveNumber), GetFCBFilename(addrDSDX));
		//for now return 0
		PDS_EAX &= 0xFF00;
		PDS_EAX = ContinueSearch(GetFCBByte(addrDSDX, FileAttributeExt), GetFCBFilename(addrDSDX));
		break;
	case 0x19:		// Get Current Default Drive Number
		PDS_EAX &= 0xFF00;	// drive = 0 (A)
		break;
	case 0x1A:		// Set Disk Transfer Address
		DiskTransferAddress = addrDSDX;
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
		printf("CHDIR : '%s'\n", &PDS_Ram[addrDSDX]);
		if (strncmp(&PDS_Ram[addrDSDX], "A:\\", 3) == 0 || PDS_Ram[addrDSDX]==0)
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
		if (AL == 0)
		{
			int result = PDS_FileOpen(&PDS_Ram[addrDSDX], 0);
			if (result < 0)
				DOSError(-result);
			else
			{
				ClearCarry();
				PDS_EAX = result;
			}
		}
		else
		{
			DBG_BREAK;	// TODO

			ClearCarry();
			PDS_EAX = 5;		// temporary handle.. we should implement file stuffs
			PDSpause = 1;
		}
		break;
	case 0x3E:		// Close handle
	{
		int result = PDS_CloseHandle(BX);
		if (result < 0)
			DOSError(-result);
		else
		{
			ClearCarry();
		}
		break;
	}
	case 0x3F:		// Read From Handle
	{
		int result = PDS_FileRead(addrDSDX, CX, BX);
		if (result < 0)
		{
			SetCarry();
			PDS_EAX = 0;
		}
		else
		{
			ClearCarry();
			PDS_EAX = result;
		}
		break;
	}
	case 0x41:
	{
		int result = PDS_FileDelete(&PDS_Ram[addrDSDX], CL);
		if (result < 0)
		{
			SetCarry();
			PDS_EAX = 0;
		}
		else
		{
			ClearCarry();
			PDS_EAX = result;
		}
		break;
	}
	case 0x42:
	{
		int result = PDS_FileSeek(AL, CX, DX, BX);
		if (result < 0)
			DOSError(-result);
		else
		{
			ClearCarry();
			PDS_EDX = result >> 16;
			PDS_EAX = result & 0xFFFF;
		}
		break;
	}
	case 0x44:		// IOCTRL
		IOCTL(AL);
		break;
	case 0x47:		// Get Current Directory
		ClearCarry();
		PDS_EAX = 0x100;
		PDS_Ram[addrDSSI] = 0;	// A:/
		break;
	case 0x4A:			// Modify Allocated Memory Block
		if (PDS_ES == 0xFF0)
		{
			ClearCarry();
			PDS_EBX = 0x9000;	// allow PDS to realise there is enough ram
		}
		else
		{
			SetCarry();
			PDSpause = 1;
		}
		break;
	case 0x4E:		// Find first matching file
		printf("Start Search : Filename '%s'\n", &PDS_Ram[addrDSDX]);
		//for now return 0
		if (0xFF == StartSearch(CX, &PDS_Ram[addrDSDX]))
		{
			DOSError(FileNotFound);
		}
		else
		{
			/*if (AL == 1)
			{
				// Additionally copy the filename back?
				struct DTA* dta = &PDS_Ram[DiskTransferAddress];
				strcpy(&PDS_Ram[addrDSDX], dta->filenameFound);
			}*/
			ClearCarry();
		}
		break;
	case 0x54:		// Get Verify Flag
		PDS_EAX &= 0xFF00;	// verify off
		break;
	case 0x57:
		if (AL == 0)
		{
			uint16_t time, date;
			int result = PDS_GetFileDateTime(&time,&date,BX);
			if (result < 0)
				DOSError(-result);
			else
			{
				ClearCarry();
				PDS_ECX = time;
				PDS_EDX = date;
			}
			break;
		}
		else
		{
			DBG_BREAK;
		}
		break;
	case 0x60:		// Get Fully Qualified file name
	{
		const char* filename = &PDS_Ram[addrDSSI];
		char* dstBuffer = &PDS_Ram[addrESDI];
		snprintf(dstBuffer, 127, "A:\\%s", filename);
		ClearCarry();
	}
		break;
	case 0x62:		// Get PSP address
		PDS_EBX = 0xFF0;
		break;
	default:
		printf("Unimplemented DOS Function %02X\n", functionNumber);
		DBG_BREAK;//unhandled vector
		break;
	}
}

void VIDEO_CONFIG(uint8_t functionNumber)
{
	switch (functionNumber)
	{
	case 0x10: // Get Video Information

		PDS_EBX = 0x0010;		// colour 64k
		PDS_ECX = 0xffff;		// features/switch status
		break;
	default:
		printf("Unimplemented VIDEO_CONFIG %02X\n", functionNumber);
		DBG_BREAK;//unhandled vector
		break;
	}
}

void CHAR_CURRENT_INFO(uint8_t functionNumber)
{
	switch (functionNumber)
	{
	case 0x00:
		PDS_ECX = 8;
		PDS_EDX &= 0xFF00;
		PDS_EDX |= 7;
		PDS_ES = 0xB000;
		PDS_SegBase[2] = PDS_ES * 16;
		PDS_EBP = 0x0000;
		break;
	default:
		printf("Unimplemented CHAR_CURRENT_INFO %02X\n", functionNumber);
		DBG_BREAK;//unhandled vector
		break;
	}
}

void CHARACTER_GENERATOR_ROUTINE(uint8_t functionNumber)
{
	uint8_t BH = (PDS_EBX >> 8) & 0xFF;
	switch (functionNumber)
	{
	case 0x30:	// Get current character generator information
		CHAR_CURRENT_INFO(BH);
		break;
	default:
		printf("Unimplemented CHARACTER_GENERATOR_ROUTINE %02X\n", functionNumber);
		DBG_BREAK;//unhandled vector
		break;
	}
}


void VIDEO_Function(uint8_t functionNumber)
{
	uint8_t AL = PDS_EAX & 0xFF;
	uint8_t BL = PDS_EBX & 0xFF;
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
	case 0x02:		// Set cursor Pos
		printf("Set Cursor : %02X,%02X  [page %02X]\n", PDS_EDX & 0xFF, (PDS_EDX >> 8) & 0xFF, (PDS_EBX >> 8) & 0xFF);
		break;
	case 0x0F:		// Get current video state
		PDS_EAX = 0x5003;	// 80 col, mode 3
		PDS_EBX &= 0xFF;	// page 0
		break;
	case 0x11:		// Character Generator Routines
		CHARACTER_GENERATOR_ROUTINE(AL);
		break;
	case 0x12:		// Video System Configuration
		VIDEO_CONFIG(BL);
		break;
	default:
		printf("Unimplemented VIDEO Function %02X\n", functionNumber);
		DBG_BREAK;//unhandled vector
		break;
	}
}

void MOUSE_Function(uint8_t functionNumber)
{
	switch (functionNumber)
	{
	case 0x0000:
		break;
	default:
		printf("Unimplemented MOUSE Function %04X\n", functionNumber);
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
	case 0x09:
		// Keyboard - gets redirected by pds eventually
		break;
	case 0x10:
		VIDEO_Function((PDS_EAX >> 8) & 0xFF);
		break;
	case 0x21:
		DOS_Function((PDS_EAX >> 8) & 0xFF);
		break;
	case 0x33:
		MOUSE_Function(PDS_EAX & 0xFFFF);
		break;

	default:
		printf("Unimplemented Interupt Vector Trap %02X\n", vector);
		DBG_BREAK;//unhandled vector
		break;
	}
}


void RenderVideo();

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
	uint32_t* gfx = (uint32_t*) (videoMemory[PDS_WINDOW]);
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

#define CLOCK_WAIT (10000)
#define TIMER_DELAY (10000*3)
int clksDelay = CLOCK_WAIT;
int timerDelay = TIMER_DELAY;

int PDS_Tick()
{
	if (PDSpause)
	{
		PDS_DebugIt();
		getchar();
	}
	PDS_STEP();
	clksDelay--;
	timerDelay--;
	if (timerDelay == 0)
	{
		timerDelay = TIMER_DELAY;
		PDS_INTERRUPT(0x08);
	}
	if (clksDelay == 0)
	{
		if (PDS_keyBufferRead != PDS_keyBufferWrite)
		{
			PDS_INTERRUPT(0x09);
		}
		clksDelay = CLOCK_WAIT;

		RenderVideo();
		return 1;
	}
	return 0;
}

int LoadBinary(const char* fname, uint32_t address);

void PDS_Start()
{
	PDS_WINDOW = VideoCreate(640, 200, 2, 4, 1, 1, "PDS", 0);
	videoMemory[PDS_WINDOW] = (unsigned char*)malloc(640*200*sizeof(unsigned int));
	PDS_Keys();

	PSF_Load("C:\\Users\\savou\\Downloads\\PDS\\PDS_executables\\BM.PSF");		//Extracted from IBM-EGA8x8.FON from old school font pack

	PDS_LoadEXE("C:\\Users\\savou\\Downloads\\PDS\\PDS_executables\\atd\\pdsz80.exe");
	//PDS_LoadEXE("C:\\Users\\savou\\Downloads\\PDS\\PDS_executables\\version121_pdsz80.exe");
	//PDS_LoadEXE("C:\\Users\\savou\\Downloads\\PDS\\PDS_executables\\P89.exe");
	PDS_Setup("");// "A.PRJ");// A:\\A.PRJ");

//	RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/DISK_ROOT_Trans/";

	RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/JOYSTICK_DEMOS/TRANSFOR/";	// CHAIR VERSION
	//RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/JOYSTICK_DEMOS/HITCH/";
	///RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/JOYSTICK_DEMOS/NINJA2/";
	//RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/JOYSTICK_DEMOS/PALETTE/";
	//RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/JOYSTICK_DEMOS/SLIDE/";
	//RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/JOYSTICK_DEMOS/CUBE/";
	//RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/LATER/CUBE/";	// DONE.FL1
	//RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/LATER/HITCH/";
	//RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/LATER/INVADERS/"; /// INVESTIGATE
	//RootDisk = "C:/Users/savou/Downloads/PDS/PDS_executables/LATER/TRANSFOR/";

	// Download Alternate Flare 1 rom... (4 bytes at head of image for some reason)
	LoadBinary("C:\\Users\\savou\\Downloads\\External Contributions\\ST_DISK_Z80_PROGS_FLARE_1\\PDS\\PDS_RO0.P", -4);
}

// Standalone
void PDS_SetControl(int number, uint8_t value);

void PDS_Main()
{
#if PDS_INTERCEPT_DIRECT
	PDS_SetControl(1, 255);
	PDS_CLIENT_COMMS = 255;
	PDS_SetControl(1, 63);
	PDS_SetControl(0, 255);
	PDS_SetControl(0, 255);
#endif

	return;
	VideoInitialise();
	PDS_Start();

	while (1)
	{
		if (PDS_Tick())
		{
			VideoUpdate(0);
			VideoWait(0.02f);
		}
	}
}


