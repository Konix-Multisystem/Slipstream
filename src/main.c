/*
 * Slipstream emulator
 *
 * Assumes PAL (was european after all) at present
 */

#define SLIPSTREAM_VERSION	"0.1 RC1"

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

#define SEGTOPHYS(seg,off)	( ((seg)<<4) + (off) )				// Convert Segment,offset pair to physical address

#define RAM_SIZE	(768*1024)			// Right lets get a bit more serious with available ram. (Ill make the assumption for now it extends from segment 0x0000 -> 0xC000
                                                        // which is 768k - hardware chips reside above this point (with the bios assumed to reside are E000) - NB: Memory map differs for earlier models!
unsigned char RAM[RAM_SIZE];							
unsigned char PALETTE[256*2];			

ESlipstreamSystem curSystem=ESS_MSU;

uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);
uint8_t GetPortB(uint16_t port);
void SetPortB(uint16_t port,uint8_t byte);
uint16_t GetPortW(uint16_t port);
void SetPortW(uint16_t port,uint16_t word);

int doDebug=1;
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

extern uint8_t *DIS_[256];				// FROM EDL
extern uint32_t DIS_max_;				// FROM EDL
extern uint8_t *DIS_TABLE_DECINC_MOD[256];		// FROM EDL
extern uint32_t DIS_max_TABLE_DECINC_MOD;		// FROM EDL
extern uint8_t *DIS_TABLE_OP_MOD_IMM[256];		// FROM EDL
extern uint32_t DIS_max_TABLE_OP_MOD_IMM;		// FROM EDL
extern uint8_t *DIS_TABLE_SH1_MOD[256];			// FROM EDL
extern uint32_t DIS_max_TABLE_SH1_MOD;			// FROM EDL
extern uint8_t *DIS_TABLE_SHV_MOD[256];			// FROM EDL
extern uint32_t DIS_max_TABLE_SHV_MOD;			// FROM EDL
extern uint8_t *DIS_TABLE_SOP_MOD[256];			// FROM EDL
extern uint32_t DIS_max_TABLE_SOP_MOD;			// FROM EDL
extern uint8_t *DIS_TABLE_OP_MOD[256];			// FROM EDL
extern uint32_t DIS_max_TABLE_OP_MOD;			// FROM EDL


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
	CONSOLE_OUTPUT("GetByte : %05X - TODO\n",addr);
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
	CONSOLE_OUTPUT("GetByte : %05X - TODO\n",addr);
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
				CONSOLE_OUTPUT("Reading from address : %05X->%02X\n",addr,retVal);
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
		CONSOLE_OUTPUT("STOMP STOMP STOMP\n");
	}
	if (debugWatchWrites)
	{
		if (addr<0xC1000 || addr>0xC1FFF)		// DSP handled seperately
		{
			CONSOLE_OUTPUT("Writing to address : %05X<-%02X\n",addr,byte);
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
	CONSOLE_OUTPUT("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
#endif
}

void SetByteP88(uint32_t addr,uint8_t byte)
{
	addr&=0xFFFFF;
#if ENABLE_DEBUG
	if (addr==doDebugTrapWriteAt)
	{
		CONSOLE_OUTPUT("STOMP STOMP STOMP\n");
	}
	if (debugWatchWrites)
	{
		if (addr<0x41000 || addr>0x41FFF)		// DSP handled seperately
		{
			CONSOLE_OUTPUT("Writing to address : %05X<-%02X\n",addr,byte);
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
	CONSOLE_OUTPUT("SetByte : %05X,%02X - TODO\n",addr&0xFFFFF,byte);
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
					CONSOLE_OUTPUT("INTL: Vertical line interrupt location\n");
					break;
				case 0x0001:
					CONSOLE_OUTPUT("INTH: Vertical line interrupt location\n");
					break;
				case 0x0002:
					CONSOLE_OUTPUT("STARTL - screen line start\n");
					break;
				case 0x0003:
					CONSOLE_OUTPUT("STARTH - screen line start\n");
					break;
				case 0x0008:
					CONSOLE_OUTPUT("SCROLL1 - TL pixel address LSB\n");
					break;
				case 0x0009:
					CONSOLE_OUTPUT("SCROLL2 - TL pixel address middle byte\n");
					break;
				case 0x000A:
					CONSOLE_OUTPUT("SCROLL3 - TL pixel address MSB\n");
					break;
				case 0x000B:
					CONSOLE_OUTPUT("ACK - interrupt acknowledge\n");
					break;
				case 0x000C:
					CONSOLE_OUTPUT("MODE - screen mode\n");
					break;
				case 0x000D:
					CONSOLE_OUTPUT("BORDL - border colour\n");
					break;
				case 0x000E:
					CONSOLE_OUTPUT("BORDH - border colour\n");
					break;
				case 0x000F:
					CONSOLE_OUTPUT("PMASK - palette mask\n");
					break;
				case 0x0010:
					CONSOLE_OUTPUT("INDEX - palette index\n");
					break;
				case 0x0011:
					CONSOLE_OUTPUT("ENDL - screen line end\n");
					break;
				case 0x0012:
					CONSOLE_OUTPUT("ENDH - screen line end\n");
					break;
				case 0x0013:
					CONSOLE_OUTPUT("MEM - memory configuration\n");
					break;
				case 0x0015:
					CONSOLE_OUTPUT("DIAG - diagnostics\n");
					break;
				case 0x0016:
					CONSOLE_OUTPUT("DIS - disable interupts\n");
					break;
				case 0x0030:
					CONSOLE_OUTPUT("BLPROG0\n");
					break;
				case 0x0031:
					CONSOLE_OUTPUT("BLPROG1\n");
					break;
				case 0x0032:
					CONSOLE_OUTPUT("BLPROG2\n");
					break;
				case 0x0033:
					CONSOLE_OUTPUT("BLTCMD\n");
					break;
				case 0x0034:
					CONSOLE_OUTPUT("BLTCON - blitter control\n");
					break;
					// The below 4 ports are from the development kit <-> PC interface  | Chip Z8536 - Zilog CIO counter/timer parallel IO Unit
				case 0x0060:
					CONSOLE_OUTPUT("DRC - Data Register C\n");
					break;
				case 0x0061:
					CONSOLE_OUTPUT("DRB - Data Register B\n");
					break;
				case 0x0062:
					CONSOLE_OUTPUT("DRA - Data Register A\n");
					break;
				case 0x0063:
					CONSOLE_OUTPUT("CTRL - Control Register (not sure which one yet though)\n");
					break;
				default:
					CONSOLE_OUTPUT("PORT WRITE UNKNOWN - TODO\n");
					exit(-1);
					break;
			}

			break;
		case ESS_MSU:
			switch (port)
			{
				case 0x0000:
					CONSOLE_OUTPUT("KINT ??? Vertical line interrupt location (Word address)\n");
					break;
				case 0x0004:
					CONSOLE_OUTPUT("STARTL - screen line start (Byte address)\n");
					break;
				case 0x0010:
					CONSOLE_OUTPUT("SCROLL1 - TL pixel address LSB (Word address) - byte width\n");
					break;
				case 0x0012:
					CONSOLE_OUTPUT("SCROLL2 - TL pixel address middle byte (Word address) - byte width\n");
					break;
				case 0x0014:
					CONSOLE_OUTPUT("SCROLL3 - TL pixel address MSB (Word address) - byte width\n");
					break;
				case 0x0016:
					CONSOLE_OUTPUT("ACK - interrupt acknowledge (Byte address)\n");
					break;
				case 0x0018:
					CONSOLE_OUTPUT("MODE - screen mode (Byte address)\n");
					break;
				case 0x001A:
					CONSOLE_OUTPUT("BORD - border colour (Word address)  - Little Endian if matching V1\n");
					break;
				case 0x001E:
					CONSOLE_OUTPUT("PMASK - palette mask? (Word address) - only a byte documented\n");
					break;
				case 0x0020:
					CONSOLE_OUTPUT("INDEX - palette index (Word address) - only a byte documented\n");
					break;
				case 0x0022:
					CONSOLE_OUTPUT("ENDL - screen line end (Byte address)\n");
					break;
				case 0x0026:
					CONSOLE_OUTPUT("MEM - memory configuration (Byte address)\n");
					break;
				case 0x002A:
					CONSOLE_OUTPUT("DIAG - diagnostics (Byte address)\n");
					break;
				case 0x002C:
					CONSOLE_OUTPUT("DIS - disable interupts (Byte address)\n");
					break;
				case 0x0040:
					CONSOLE_OUTPUT("BLTPC (low 16 bits) (Word address)\n");
					break;
				case 0x0042:
					CONSOLE_OUTPUT("BLTCMD (Word address)\n");
					break;
				case 0x0044:
					CONSOLE_OUTPUT("BLTCON - blitter control (Word address) - only a byte documented, but perhaps step follows?\n");
					break;
				case 0x00C0:
					CONSOLE_OUTPUT("ADP - (Word address) - Anologue/digital port reset?\n");
					break;
				case 0x00E0:
					CONSOLE_OUTPUT("???? - (Byte address) - number pad reset?\n");
					break;
				default:
					CONSOLE_OUTPUT("PORT WRITE UNKNOWN - TODO\n");
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
					CONSOLE_OUTPUT("HLPL - Horizontal scanline position low byte\n");
					break;
				case 0x0001:
					CONSOLE_OUTPUT("HLPH - Horizontal scanline position hi byte\n");
					break;
				case 0x0002:
					CONSOLE_OUTPUT("VLPL - Vertical scanline position low byte\n");
					break;
				case 0x0003:
					CONSOLE_OUTPUT("VLPH - Vertical scanline position hi byte\n");
					break;
				case 0x0040:
					CONSOLE_OUTPUT("PORT1 - Joystick 1\n");
					break;
				case 0x0050:
					CONSOLE_OUTPUT("PORT2 - Joystick 2\n");
					break;
				case 0x0060:
					CONSOLE_OUTPUT("DRC - Data Register C\n");
					break;
				case 0x0061:
					CONSOLE_OUTPUT("DRB - Data Register B\n");
					break;
				case 0x0062:
					CONSOLE_OUTPUT("DRA - Data Register A\n");
					break;
				case 0x0063:
					CONSOLE_OUTPUT("CTRL - Control Register (not sure which one yet though)\n");
					break;
				default:
					CONSOLE_OUTPUT("PORT READ UNKNOWN - TODO\n");
					exit(-1);
					break;
			}

			break;
		case ESS_MSU:

			switch (port)
			{
				case 0x0C:
					CONSOLE_OUTPUT("???? - (Byte Address) - controller buttons...\n");
					break;
				case 0x80:
					CONSOLE_OUTPUT("???? - (Word Address) - Possibly controller digital button status\n");
					break;
				case 0xC0:
					CONSOLE_OUTPUT("ADP - (Word Address) - Analogue/digital port status ? \n");
					break;
				case 0xE0:
					CONSOLE_OUTPUT("???? - (Byte Address) - Numberic pad read ? \n");
					break;
				default:
					CONSOLE_OUTPUT("PORT READ UNKNOWN - TODO\n");
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
						CONSOLE_OUTPUT("Warning unknown numPadRowSelectValue : %02X\n",numPadRowSelect);
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
		CONSOLE_OUTPUT("GetPortB : %04X - TODO\n",port);
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
		CONSOLE_OUTPUT("GetPortW : %04X - TODO\n",port);
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
	CONSOLE_OUTPUT("--------\n");
	CONSOLE_OUTPUT("FLAGS = O  D  I  T  S  Z  -  A  -  P  -  C\n");
	CONSOLE_OUTPUT("        %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s\n",
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

	CONSOLE_OUTPUT("AX= %04X\n",AX);
	CONSOLE_OUTPUT("BX= %04X\n",BX);
	CONSOLE_OUTPUT("CX= %04X\n",CX);
	CONSOLE_OUTPUT("DX= %04X\n",DX);
	CONSOLE_OUTPUT("SP= %04X\n",SP);
	CONSOLE_OUTPUT("BP= %04X\n",BP);
	CONSOLE_OUTPUT("SI= %04X\n",SI);
	CONSOLE_OUTPUT("DI= %04X\n",DI);
	CONSOLE_OUTPUT("CS= %04X\n",CS);
	CONSOLE_OUTPUT("DS= %04X\n",DS);
	CONSOLE_OUTPUT("ES= %04X\n",ES);
	CONSOLE_OUTPUT("SS= %04X\n",SS);
	CONSOLE_OUTPUT("--------\n");
}

const char* GetSReg(int reg)
{
	char* regs[4]={"ES", "CS", "SS", "DS"};

	return regs[reg&3];
}
	
const char* GetReg(int word,int reg)
{
	char* regw[8]={"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"};
	char* regb[8]={"AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH"};

	if (word)
	{
		return regw[reg&7];
	}
		
	return regb[reg&7];
}
	
const char* GetModRM(int word,uint8_t modrm,uint32_t address,int *cnt)
{
	const char* modregs[8]={"BX+SI", "BX+DI", "BP+SI", "BP+DI", "SI", "DI", "BP", "BX"};
	static char tmpBuffer[256];

	*cnt=0;
	switch (modrm&0xC0)
	{
		case 0xC0:
			sprintf(tmpBuffer,"%s",GetReg(word,modrm&0x7));
			break;
		case 0x00:
			if ((modrm&7)==6)
			{
				*cnt=2;
				sprintf(tmpBuffer,"[%02X%02X]",PeekByte(address+1),PeekByte(address));
			}
			else
			{
				sprintf(tmpBuffer,"[%s]",modregs[modrm&7]);
			}
			break;
		case 0x40:
			{
				uint16_t tmp=PeekByte(address);
				if (tmp&0x80)
				{
					tmp|=0xFF00;
				}
				*cnt=1;
				sprintf(tmpBuffer,"%04X[%s]",tmp,modregs[modrm&7]);
			}
			break;
		case 0x80:
//			*cnt=2;
//			sprintf(tmpBuffer,"%02X%02X[%s]",PeekByte(address+1),PeekByte(address),modregs[modrm&7]);
			sprintf(tmpBuffer,"TODO");
			break;
	}

	return tmpBuffer;
}

int DoRegWB(uint8_t op,char** tPtr)
{
	const char* reg;
	
	reg=GetReg(op&8,op&7);

	while (*reg)
	{
		*(*tPtr)++=*reg++;
	}
	return 0;
}

int DoModSRegRM(uint8_t op,uint32_t address,char** tPtr)
{
	char tmpBuffer[256];
	// Extract register,EA
	int nextByte=PeekByte(address);
	int word=1;
	int direc=op&2;
	char* reg=tmpBuffer;
	int cnt;

	if (direc)
	{
		sprintf(tmpBuffer,"%s,%s",GetSReg((nextByte&0x18)>>3),GetModRM(word,nextByte,address+1,&cnt));
	}
	else
	{
		sprintf(tmpBuffer,"%s,%s",GetModRM(word,nextByte,address+1,&cnt),GetSReg((nextByte&0x18)>>3));
	}
	while (*reg)
	{
		*(*tPtr)++=*reg++;
	}
	return cnt;
}

int DoModRegRM(uint8_t op,uint32_t address,char** tPtr)
{
	char tmpBuffer[256];
	// Extract register,EA
	int nextByte=PeekByte(address);
	int word=op&1;
	int direc=op&2;
	char* reg=tmpBuffer;
	int cnt;

	if (direc)
	{
		sprintf(tmpBuffer,"%s,%s",GetReg(word,(nextByte&0x38)>>3),GetModRM(word,nextByte,address+1,&cnt));
	}
	else
	{
		sprintf(tmpBuffer,"%s,%s",GetModRM(word,nextByte,address+1,&cnt),GetReg(word,(nextByte&0x38)>>3));
	}
	while (*reg)
	{
		*(*tPtr)++=*reg++;
	}
	return cnt;
}

int DoModnnnRM(uint8_t op,uint32_t address,char** tPtr)
{
	char tmpBuffer[256];
	// Extract register,EA
	int nextByte=PeekByte(address);
	int word=op&1;
	char* reg=tmpBuffer;
	int cnt;

	sprintf(tmpBuffer,"%s",GetModRM(word,nextByte,address+1,&cnt));
	while (*reg)
	{
		*(*tPtr)++=*reg++;
	}
	return cnt;
}


int DoDisp(int cnt,uint32_t address,char** tPtr)
{
	char tmpBuffer[256];
	char* reg=tmpBuffer;
	uint16_t disp;
	
	if (cnt==8)
	{
		disp=PeekByte(address);
		if (disp&0x80)
		{
			disp|=0xFF00;
		}
		sprintf(tmpBuffer,"%04X (%02X)",(address+disp)&0xFFFF,disp&0xFF);
	}
	if (cnt==16)
	{
		disp=PeekByte(address);
		disp|=PeekByte(address+1)<<8;
		sprintf(tmpBuffer,"%04X (%04X)",(address+2+disp)&0xFFFF,disp&0xFFFF);
	}
	while (*reg)
	{
		*(*tPtr)++=*reg++;
	}
	return cnt/8;
}

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength)
{
	static char segOveride[2048];
	static char temporaryBuffer[2048];
	char sprintBuffer[256];
	char tmpCommand[256];
	char* tPtr;

	uint8_t byte = PeekByte(address);
	if (byte>=realLength)
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
	
		if (strncmp(mnemonic,"REP",3)==0)
		{
			int tmpcount=0;
			sPtr=decodeDisasm(DIS_,address+1,&tmpcount,DIS_max_);
			if (sPtr==NULL)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
			*count=tmpcount+1;
			strcpy(segOveride,mnemonic);
			strcat(segOveride," ");
			strcat(segOveride,sPtr);
			return segOveride;
		}
		if (strncmp(mnemonic,"XX001__110",10)==0)				// Segment override
		{
			int tmpcount=0;
			sPtr=decodeDisasm(DIS_,address+1,&tmpcount,DIS_max_);
			if (sPtr==NULL)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
			*count=tmpcount+1;
			strcpy(segOveride,mnemonic+10);
			strcat(segOveride,temporaryBuffer);
			return segOveride;
		}

		tmpCommand[0]=0;
// New version. disassembler does more work it needs to scan for # tags, grab the contents, perform an action and write the data to the destination string
		if (strcmp(mnemonic,"#TABLE2#")==0)				// alternate table
		{
			// assumed to represent entire mnemonic, so just reparse using the new table information
			byte=PeekByte(address+1);
			if (byte>=DIS_max_TABLE_DECINC_MOD)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
			sPtr=(char*)DIS_TABLE_DECINC_MOD[byte];
			if (sPtr==NULL)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
		}
		if (strcmp(mnemonic,"#TABLE4#")==0)				// alternate table
		{
			// assumed to represent entire mnemonic, so just reparse using the new table information
			byte=PeekByte(address+1);
			if (byte>=DIS_max_TABLE_OP_MOD_IMM)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
			sPtr=(char*)DIS_TABLE_OP_MOD_IMM[byte];
			if (sPtr==NULL)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
		}
		if (strcmp(mnemonic,"#TABLE5#")==0)				// alternate table
		{
			// assumed to represent entire mnemonic, so just reparse using the new table information
			byte=PeekByte(address+1);
			if (byte>=DIS_max_TABLE_SH1_MOD)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
			sPtr=(char*)DIS_TABLE_SH1_MOD[byte];
			if (sPtr==NULL)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
		}
		if (strcmp(mnemonic,"#TABLE6#")==0)				// alternate table
		{
			// assumed to represent entire mnemonic, so just reparse using the new table information
			byte=PeekByte(address+1);
			if (byte>=DIS_max_TABLE_SHV_MOD)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
			sPtr=(char*)DIS_TABLE_SHV_MOD[byte];
			if (sPtr==NULL)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
		}
		if (strcmp(mnemonic,"#TABLE7#")==0)				// alternate table
		{
			// assumed to represent entire mnemonic, so just reparse using the new table information
			byte=PeekByte(address+1);
			if (byte>=DIS_max_TABLE_SOP_MOD)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
			sPtr=(char*)DIS_TABLE_SOP_MOD[byte];
			if (sPtr==NULL)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
		}
		if (strcmp(mnemonic,"#TABLE8#")==0)				// alternate table
		{
			// assumed to represent entire mnemonic, so just reparse using the new table information
			byte=PeekByte(address+1);
			if (byte>=DIS_max_TABLE_OP_MOD)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
			sPtr=(char*)DIS_TABLE_OP_MOD[byte];
			if (sPtr==NULL)
			{
				sprintf(temporaryBuffer,"UNKNOWN OPCODE");
				return temporaryBuffer;
			}
		}
		
		// First, scan until we hit #, then write to tmpcommand until we hit # again, then do action, then resume scan

		tPtr=tmpCommand;
		while (*sPtr)
		{
			switch (doingDecode)
			{
				case 0:
					if (*sPtr=='#')
					{
						doingDecode=1;
					}
					else
					{
						*dPtr++=*sPtr;
					}
					sPtr++;
					break;
				case 1:
					if (*sPtr=='#')
					{
						*tPtr=0;
						doingDecode=2;
					}
					else
					{
						*tPtr++=*sPtr++;
					}
					break;
				case 2:
					// We have a command.. do the action, reset command buffer

					{
						uint8_t op;
						op=PeekByte(address);

						if (strcmp(tmpCommand,"REGWB")==0)
						{
							counting+=DoRegWB(op,&dPtr);
						}
						else
						if (strcmp(tmpCommand,"MODnnnRM")==0)
						{
							counting+=1+DoModnnnRM(op,address+counting+1,&dPtr);
						}
						else
						if (strcmp(tmpCommand,"MODSREGRM")==0)
						{
							counting+=1+DoModSRegRM(op,address+counting+1,&dPtr);
						}
						else
						if (strcmp(tmpCommand,"MODregRM")==0)
						{
							counting+=1+DoModRegRM(op,address+counting+1,&dPtr);
						}
						else
						if (strcmp(tmpCommand,"8DISP")==0)
						{
							counting+=DoDisp(8,address+counting+1,&dPtr);
						}
						else
						if (strcmp(tmpCommand,"16DISP")==0)
						{
							counting+=DoDisp(16,address+counting+1,&dPtr);
						}
						else
						if (strcmp(tmpCommand,"PORT")==0)
						{
							if (op&0x01)
							{
								sprintf(sprintBuffer,"#%02X",PeekByte(address+counting+1));
							}
							else
							{
								sprintf(sprintBuffer,"#%02X",PeekByte(address+counting+1));
							}
							counting+=1;
							tPtr=sprintBuffer;
							while (*tPtr)
							{
								*dPtr++=*tPtr++;
							}
						}
						else
						if (strcmp(tmpCommand,"IMMSW")==0)
						{
							if (op&0x01)
							{
								if (op&0x02)
								{
									uint16_t t=PeekByte(address+counting+1);
									if (t&0x80)
										t|=0xFF00;
									sprintf(sprintBuffer,"#%04X",t);
									counting+=1;
								}
								else
								{
									sprintf(sprintBuffer,"#%02X%02X",PeekByte(address+counting+2),PeekByte(address+counting+1));
									counting+=2;
								}
							}
							else
							{
								sprintf(sprintBuffer,"#%02X",PeekByte(address+counting+1));
								counting+=1;
							}
							tPtr=sprintBuffer;
							while (*tPtr)
							{
								*dPtr++=*tPtr++;
							}
						}
						else
						if (strcmp(tmpCommand,"ADDR")==0)
						{
							sprintf(sprintBuffer,"[%02X%02X]",PeekByte(address+counting+2),PeekByte(address+counting+1));
							counting+=2;
							tPtr=sprintBuffer;
							while (*tPtr)
							{
								*dPtr++=*tPtr++;
							}
						}
						else
						if (strcmp(tmpCommand,"IMM3")==0)
						{
							if (op&0x08)
							{
								sprintf(sprintBuffer,"#%02X%02X",PeekByte(address+counting+2),PeekByte(address+counting+1));
								counting+=2;
							}
							else
							{
								sprintf(sprintBuffer,"#%02X",PeekByte(address+counting+1));
								counting+=1;
							}
							tPtr=sprintBuffer;
							while (*tPtr)
							{
								*dPtr++=*tPtr++;
							}
						}
						else
						if (strcmp(tmpCommand,"IMM0")==0)
						{
							if (op&0x01)
							{
								sprintf(sprintBuffer,"#%02X%02X",PeekByte(address+counting+2),PeekByte(address+counting+1));
								counting+=2;
							}
							else
							{
								sprintf(sprintBuffer,"#%02X",PeekByte(address+counting+1));
								counting+=1;
							}
							tPtr=sprintBuffer;
							while (*tPtr)
							{
								*dPtr++=*tPtr++;
							}
						}
						else
							CONSOLE_OUTPUT("Unknown Command : %s\n",tmpCommand);

					}

					sPtr++;
					tPtr=tmpCommand;
					doingDecode=0;
					break;
			}
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
		CONSOLE_OUTPUT("UNKNOWN AT : %05X\n",address);		// TODO this will fail to wrap which may show up bugs that the CPU won't see
		for (a=0;a<numBytes+1;a++)
		{
			CONSOLE_OUTPUT("%02X ",PeekByte(address+a));
		}
		CONSOLE_OUTPUT("\nNext 7 Bytes : ");
		for (a=0;a<7;a++)
		{
			CONSOLE_OUTPUT("%02X ",PeekByte(address+numBytes+1+a));
		}
		CONSOLE_OUTPUT("\n");
		DUMP_REGISTERS();
		exit(-1);
	}

	if (registers)
	{
		DUMP_REGISTERS();
	}
	CONSOLE_OUTPUT("%05X :",address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see

	for (a=0;a<numBytes+1;a++)
	{
		CONSOLE_OUTPUT("%02X ",PeekByte(address+a));
	}
	for (a=0;a<8-(numBytes+1);a++)
	{
		CONSOLE_OUTPUT("   ");
	}
	CONSOLE_OUTPUT("%s\n",retVal);

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

int use6MhzP88Cpu=1;
int emulateDSP=1;

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

	switch (curSystem)
	{
		case ESS_MSU:
			return CYCLES;			// Assuming clock speed same as hardware chips
		case ESS_P88:
			if (use6MhzP88Cpu)
				return CYCLES*2;		// 6Mhz
			else
				return CYCLES;
	}

	return 0;
}
	

void Usage()
{
	CONSOLE_OUTPUT("slipstream [opts] program.msu/program.p88\n");
	CONSOLE_OUTPUT("-f [disable P88 frequency divider]\n");
	CONSOLE_OUTPUT("-b address file.bin [Load binary to ram]\n");
	CONSOLE_OUTPUT("-n [disable DSP emulation]\n");
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

	VideoInitialise(WIDTH,HEIGHT,"Slipstream - V" SLIPSTREAM_VERSION);
	KeysIntialise();
	AudioInitialise(WIDTH*HEIGHT);

	//////////////////
	
//	doDebugTrapWriteAt=0x088DAA;
//	debugWatchWrites=1;
//	doDebug=1;

	while (1==1)
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

uint32_t missing(uint32_t opcode)
{
	int a;
	CONSOLE_OUTPUT("IP : %04X:%04X\n",CS,IP);
	CONSOLE_OUTPUT("Next 7 Bytes : ");
	for (a=0;a<7;a++)
	{
		CONSOLE_OUTPUT("%02X ",PeekByte(SEGTOPHYS(CS,IP)+a));
	}
	CONSOLE_OUTPUT("\nNext 7-1 Bytes : ");
	for (a=0;a<7;a++)
	{
		CONSOLE_OUTPUT("%02X ",PeekByte(SEGTOPHYS(CS,IP)+a-1));
	}
	CONSOLE_OUTPUT("\nNext 7-2 Bytes : ");
	for (a=0;a<7;a++)
	{
		CONSOLE_OUTPUT("%02X ",PeekByte(SEGTOPHYS(CS,IP)+a-2));
	}
	CONSOLE_OUTPUT("\n");
	exit(-1);
}


