#if MEMORY_MAPPED_DEBUGGER

#include <stdint.h>
#include "system.h"
#include "disasm.h"
#include "debugger.h"

#include <windows.h>

volatile unsigned char* pMapRegisters = NULL;
volatile unsigned char* pMapDisassm = NULL;
volatile unsigned char* pMapASIC = NULL;
volatile unsigned char* pMapControl = NULL;

volatile unsigned char* pMapIO = NULL;

void InitMemoryMappedDebugger()
{
	HANDLE hMapRead = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 32768, "Slip_Registers");

	pMapRegisters = (volatile unsigned char*)MapViewOfFile(hMapRead, FILE_MAP_ALL_ACCESS, 0, 0, 32768);
	
	hMapRead = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 32768, "Slip_Disassm");

	pMapDisassm = (volatile unsigned char*)MapViewOfFile(hMapRead, FILE_MAP_ALL_ACCESS, 0, 0, 32768);
	
	hMapRead = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 32768, "Slip_ASIC");

	pMapASIC = (volatile unsigned char*)MapViewOfFile(hMapRead, FILE_MAP_ALL_ACCESS, 0, 0, 32768);
	
	hMapRead = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, "Slip_Control");

	pMapControl = (volatile unsigned char*)MapViewOfFile(hMapRead, FILE_MAP_ALL_ACCESS, 0, 0, 4096);

	pMapControl[0] = 0xFF;
	pMapControl[1] = 0xFF;
	
#if ENABLE_MEMORY_MAPPED_IO
	hMapRead = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "Slip_PC_IO");

	pMapIO = (volatile unsigned char*)MapViewOfFile(hMapRead, FILE_MAP_ALL_ACCESS, 0, 0, 32768);
#else
	pMapIO = (volatile unsigned char*)malloc(32768);
#endif
}

uint16_t ASIC_DEB_PORTS[128];

#if 0
 CP1 Maps
const char* WritePortInfo[128] =
{
	"VID_INT", "V_PERIOD", "V_DIS_BEG", "V_BLK_END", "H_COUNT", "V_DIS_END", "V_COUNT", "V_BLK_BEG", "SCROLL0", "SCROLL1", "V_SYNC", "ACKW  ",
	"MODE  ", "BORDER", "H_PERIOD", "MASK  ", "INDEX ", "H_BLK_END", "H_FCH_BEG", "MEM   ", "GPR   ", "MODE2 ", "INT_DIS", "H_DIS_BEG", "H_FCH_END", "H_DIS_END",
	"H_BLK_BEG", "H_SYNC", "H_VSYNC", "BANK  ", "CHRW  ",0, "BLITPC0", "BLITPC1", "BLITCMD", "BLITCON", 0, "BLITROT0", "BLITROT1", "BLITROT2",
	0,0,0,0,0,0,0,0,
	"CDCMD ", "CDADDR0", "CDADDR1", "CDCNT0", "CDCNT1", "CDHDR0", "CDHDR1", 0, 0,0,0,0,0,0,0,0,
	"JOYOUT", "TO_MIC", 0, 0, "P_DATA", "P_STAT", "BIOS", /*"JOYOUT",
	"JOYOUT", "JOYOUT", "JOYOUT", "JOYOUT", "JOYOUT", "JOYOUT", "JOYOUT", "JOYOUT",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO1 ", /*"GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ",
	"GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO2 ", /*"GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ",
	"GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO3 ", /*"GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ",
	"GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

const char* ReadPortInfo[128] =
{
	"LP0   ", "LP1   ", "TEST  ", "ACKR  ", 0, 0, "STAT  ", 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,0, "BLITDST0", "BLITDST1", "BLITSRC0", "BLITSRC1", "BLITIN ", "BLITOUT", "BLITSTAT", 0,
	0,0,0,0,0,0,0,0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,
	"JOYIN", 0, "F_MIC", "COMMS", 0/*"P_DATA"*/, "P_STAT", /*"JOYIN", "JOYIN",
	"JOYIN", "JOYIN", "JOYIN", "JOYIN", "JOYIN", "JOYIN", "JOYIN", "JOYIN",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO1 ", /*"GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ",
	"GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO2 ", /*"GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ",
	"GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO3 ", /*"GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ",
	"GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
#else

const char* WritePortInfo[128] =
{
	"VID_INT", 0, "STARTL", 0, "HCNT  ", 0, "VCNT  ", 0, "SCROLL1", "SCROLL2", "SCROLL3", "ACK",
	"MODE  ", "BORDER", 0, "PMASK ", "INDEX ", "ENDL", 0, "MEM   ", "GPR   ", "DIAG ", "DIS ", "JOY", 0, 0,
	0, 0, 0, 0, 0,0, "BLITPC", "BLITCMD", "BLITCON", 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	"CDCMD ", "CDADDR0", "CDADDR1", "CDCNT0", "CDCNT1", "CDHDR0", "CDHDR1", 0, 
    0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,	
    0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO1 ", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO2 ", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO3 ", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

const char* ReadPortInfo[128] =
{
	"HLP   ", "VLP   ", 0, 0, "JOYIN", 0, "STAT  ", 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,0, "BLITDST0", "BLITDST1", "BLITSRC0", "BLITSRC1", "BLITIN ", "BLITOUT", "BLITSTAT", 0,
	0,0,0,0,0,0,0,0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,
	0, 0, 0, 0, 0/*"P_DATA"*/, 0, /*"JOYIN", "JOYIN",
	"JOYIN", "JOYIN", "JOYIN", "JOYIN", "JOYIN", "JOYIN", "JOYIN", "JOYIN",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO1 ", /*"GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ",
	"GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ", "GPIO1 ",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO2 ", /*"GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ",
	"GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ", "GPIO2 ",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	"GPIO3 ", /*"GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ",
	"GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ", "GPIO3 ",*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
#endif

int UpdateMemoryMappedDebuggerViews()
{
	int cmd = 0xff;
	char* tmp2 = (char*)pMapRegisters;
	switch (curSystem)
	{
        case ESS_CP1:
		case ESS_MSU:
			FETCH_REGISTERS80386(tmp2);
			break;
		case ESS_P88:
		//	address=SEGTOPHYS(CS,IP)&0xFFFFF;
			//sprintf(tmp,"%s:P88%05X\n",pause?"Paused":"Runnin",address);
			FETCH_REGISTERS8086(tmp2);
			break;
		case ESS_P89:
			//address=SEGTOPHYS(CS,IP)&0xFFFFF;
			//sprintf(tmp,"%s:P88%05X\n",pause?"Paused":"Runnin",address);
			FETCH_REGISTERS8086(tmp2);
			break;
		case ESS_FL1:
			//address=Z80_PC;//GetZ80LinearAddress()&0xFFFFF;		// disassembly expects address in chip space not linear space
			//sprintf(tmp,"%s:FL1%05X\n",pause?"Paused":"Runnin",address);
			FETCH_REGISTERSZ80(tmp2);
			break;
	}

	char* tmp = (char*)pMapDisassm;
	char tBuffer[2048];

	unsigned int address = MSU_GETPHYSICAL_EIP();
	for (int l = 0;l < 20;l++)
	{
		InStream disMe;
		disMe.bytesRead = 0;
		disMe.curAddress = address;
		disMe.useAddress = 1;
		Disassemble(&disMe, MSU_cSize);
		int a;

		if (disMe.bytesRead != 0)
		{
			sprintf(tmp, "%06X : ", address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see

			for (a = 0;a < disMe.bytesRead;a++)
			{
				sprintf(tBuffer, "%02X ", PeekByte(address + a));
				strcat(tmp, tBuffer);
			}
			for (a = 0;a < 15 - disMe.bytesRead;a++)
			{
				strcat(tmp, "   ");
			}
			sprintf(tBuffer, "%s\n", (char*)GetOutputBuffer());
			strcat(tmp, tBuffer);

			address += disMe.bytesRead;
			tmp += strlen(tmp);
		}
		else
			break;
	}

	tmp = (char*)pMapASIC;
	for (int a = 0;a < 128;a++)
	{
		char* ptr = tBuffer;
		*ptr = 0;
		if (WritePortInfo[a] != 0 || ReadPortInfo[a]!=0)
		{
			sprintf(ptr, "%02X : ", a * 2);
			ptr += strlen(ptr);
			if (WritePortInfo[a])
			{
				sprintf(ptr, "%04X %s", ASIC_DEB_PORTS[a], WritePortInfo[a]);
				ptr += strlen(ptr);
			}
			else
			{
				sprintf(ptr, "\t\t");
				ptr += strlen(ptr);
			}
			if (ReadPortInfo[a])
			{
				sprintf(ptr, "\t(%04X %s)\n", GetPortW(a * 2),ReadPortInfo[a]);
				ptr += strlen(ptr);
			}
			else
			{
				sprintf(ptr, "\n");
				ptr += strlen(ptr);
			}
			strcpy(tmp, tBuffer);
			tmp += strlen(tmp);
		}
	}
	if (pMapControl[0] != pMapControl[1])
	{
		cmd = pMapControl[0];
		pMapControl[1] = pMapControl[0];
	}
	return cmd;
}


#endif
