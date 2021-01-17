#if MEMORY_MAPPED_DEBUGGER 

#include <stdint.h>
#include "system.h"
#include "disasm.h"
#include "debugger.h"

#include <windows.h>

#define SEGTOPHYS(seg,off)	( ((seg)<<4) + (off) )				// Convert Segment,offset pair to physical address

volatile unsigned char* pMapRegisters = NULL;
volatile unsigned char* pMapDisassm = NULL;
volatile unsigned char* pMapASIC = NULL;
volatile unsigned char* pMapDSP = NULL;
volatile unsigned char* pMapDSPRegisters = NULL;
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
	
	hMapRead = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 32768, "Slip_DSP");

	pMapDSP = (volatile unsigned char*)MapViewOfFile(hMapRead, FILE_MAP_ALL_ACCESS, 0, 0, 32768);

	hMapRead = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 32768, "Slip_DSPReg");

	pMapDSPRegisters = (volatile unsigned char*)MapViewOfFile(hMapRead, FILE_MAP_ALL_ACCESS, 0, 0, 32768);

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
uint8_t ASIC_DEB_BYTE_PORT_WRITE[256];

const char* CP1_WritePortInfo[128] =
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

const char* CP1_ReadPortInfo[128] =
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

const char* MSU_WritePortInfo[128] =
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

const char* MSU_ReadPortInfo[128] =
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

const char* FL1_ReadPortInfo[256] =
{
	"BANK0 ", "BANK1 ", "BANK2 ", "BANK3 ", "LPEN1 ", "LPEN2 ", "LPEN3 ", /*"INTACK"*/0,	//0x00-0x07	--skip intack so we don't ack accidently
	0, 0, 0, 0,	0, 0, 0, 0,																	//0x08-0x0F
	0, 0, 0, 0, "RUNST ", 0, 0, 0,															//0x10-0x17
	"KBDDAT", 0, 0, 0, 0, 0, 0, 0,															//0x18-0x1F
	"STOPDL", "STOPDH", "GP0   ", 0, 0, 0, 0, 0,											//0x20-0x27
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x28-0x2F
	/*"COMREG"*/0, /*"TRKREG"*/0, /*"SECREG"*/0, /*"DATREG"*/0, 0, 0, 0, 0,					//0x30-0x37  --skip to avoid clearing
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x38-0x3F
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x40-0x47
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x48-0x4F
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x50-0x57
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x58-0x5F
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x60-0x67
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x68-0x6F
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x70-0x77
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x78-0x7F
	0, 0, 0, 0,	0, 0, 0, 0,																	//0x80-0x87
	0, 0, 0, 0,	0, 0, 0, 0,																	//0x88-0x8F
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x90-0x97
	0, 0, 0, 0, 0, 0, 0, 0,																	//0x98-0x9F
	"IPPORT", 0, 0, 0, 0, 0, 0, 0,															//0xA0-0xA7
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xA8-0xAF
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xB0-0xB7
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xB8-0xBF
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xC0-0xC7
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xC8-0xCF
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xD0-0xD7
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xD8-0xDF
	"CTRL_P", 0, 0, 0, 0, 0, 0, 0,															//0xE0-0xE7
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xE8-0xEF
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xF0-0xF7
	0, 0, 0, 0, 0, 0, 0, 0,																	//0xF8-0xFF
};

const char* FL1_WritePortInfo[256] =
{
	"BANK0 ", "BANK1 ", "BANK2 ", "BANK3 ", "BAUD  ", "HCNT  ", "VCNT  ", "INTREG",					//0x00-0x07
	"CMD1  ", "CMD2  ", "BORDER", "SCRLH ", "SCRLV ", "TRANS ", "MAG   ", "YEL   ",		//0x08-0x0F
	"INTRD ", "INTRDP", "INTRA ", "MPROG ", "RUNST ", "PROGRM", 0, 0,					//0x10-0x17
	"BLTPC0", "BLTPC1", "BLTPC2", 0, 0, 0, 0, 0,										//0x18-0x1F
	"BLTCMD", 0, "GP0   ", 0, 0, 0, 0, 0,												//0x20-0x27
	0, 0, 0, 0, 0, 0, 0, 0,																//0x28-0x2F
	"COMREG", "TRKREG", "SECREG", "DATREG", 0, 0, 0, 0,									//0x30-0x37
	0, 0, 0, 0, 0, 0, 0, 0,																//0x38-0x3F
	0, 0, 0, 0, 0, 0, 0, 0,																//0x40-0x47
	0, 0, 0, 0, 0, 0, 0, 0,																//0x48-0x4F
	"PALAW ", "PALVAL", "PALMSK", 0, 0, 0, 0, 0,										//0x50-0x57
	0, 0, 0, 0, 0, 0, 0, 0,																//0x58-0x5F
	0, 0, 0, 0, 0, 0, 0, 0,																//0x60-0x67
	0, 0, 0, 0, 0, 0, 0, 0,																//0x68-0x6F
	0, 0, 0, 0, 0, 0, 0, 0,																//0x70-0x77
	0, 0, 0, 0, 0, 0, 0, 0,																//0x78-0x7F
	0, 0, 0, 0,	0, 0, 0, 0,																//0x80-0x87
	0, 0, 0, 0,	0, 0, 0, 0,																//0x88-0x8F
	0, 0, 0, 0, 0, 0, 0, 0,																//0x90-0x97
	0, 0, 0, 0, 0, 0, 0, 0,																//0x98-0x9F
	0, 0, 0, 0, 0, 0, 0, 0,																//0xA0-0xA7
	0, 0, 0, 0, 0, 0, 0, 0,																//0xA8-0xAF
	0, 0, 0, 0, 0, 0, 0, 0,																//0xB0-0xB7
	0, 0, 0, 0, 0, 0, 0, 0,																//0xB8-0xBF
	"CHAIRP", 0, 0, 0, 0, 0, 0, 0,														//0xC0-0xC7
	0, 0, 0, 0, 0, 0, 0, 0,																//0xC8-0xCF
	0, 0, 0, 0, 0, 0, 0, 0,																//0xD0-0xD7
	0, 0, 0, 0, 0, 0, 0, 0,																//0xD8-0xDF
	"CTRL_P", 0, 0, 0, 0, 0, 0, 0,														//0xE0-0xE7
	0, 0, 0, 0, 0, 0, 0, 0,																//0xE8-0xEF
	0, 0, 0, 0, 0, 0, 0, 0,																//0xF0-0xF7
	0, 0, 0, 0, 0, 0, 0, 0,																//0xF8-0xFF
};


extern uint32_t	ASIC_BANK0;				// Z80 banking registers  (stored in upper 16bits)
extern uint32_t	ASIC_BANK1;
extern uint32_t	ASIC_BANK2;
extern uint32_t	ASIC_BANK3;

uint32_t getZ80LinearAddress()
{
	uint32_t addr = Z80_PC;

	switch (addr&0xC000)
	{
		case 0x0000:
			return ASIC_BANK0+(addr&0x3FFF);

		case 0x4000:
			return ASIC_BANK1+(addr&0x3FFF);

		case 0x8000:
			return ASIC_BANK2+(addr&0x3FFF);
		
		case 0xC000:
			break;
	}
	return ASIC_BANK3+(addr&0x3FFF);

}

void FL1_DSP_REGISTERS(char* output);
void FL1_DSP_DISASSEMBLE(char* output);

#include <stdio.h>>

struct SymbolElement
{
	struct SymbolElement* Previous;
	struct SymbolElement* Next;
	const char* Symbol;
	int Address;
};

struct SymbolElement* SymbolHead = NULL;

struct SymbolElement* AllocElement()
{
	return malloc(sizeof(struct SymbolElement));
}

void InitialiseElement(struct SymbolElement* element, int address, const char* symbol)
{
	element->Address = address;
	element->Symbol = malloc(strlen(symbol) + 1);
	element->Next = NULL;
	element->Previous = NULL;
	strcpy(element->Symbol, symbol);
}

int SymbolNotPresent(struct SymbolElement* element, const char* symbol)
{
	const char* found = strstr(element->Symbol, symbol);
	while (found)
	{
		if (found == element->Symbol || found[-1] == ',')
		{
			found += strlen(symbol);
			if (*found == 0 || *found == ',')
				return 0;
		}
		else
			found++;
		found = strstr(found, symbol);
	}
	return 1;
}

void InsertSymbol(int address, const char* symbol)
{
	struct SymbolElement* element = NULL;
	if (SymbolHead == NULL)
	{
		element = AllocElement();
		InitialiseElement(element, address, symbol);
		SymbolHead = element;
		return;
	}

	// Insert in address order
	element = SymbolHead;
	struct SymbolElement* previous = NULL;
	while (1 == 1)
	{
		if (element->Address == address)
		{
			if (SymbolNotPresent(element, symbol))
			{
				// Multiply defined symbol for address (append name)
				const char* tmp = element->Symbol;
				element->Symbol = malloc(strlen(tmp) + 1 + strlen(symbol) + 1);
				strcpy(element->Symbol, tmp);
				strcat(element->Symbol, ",");
				strcat(element->Symbol, symbol);
				free(tmp);
			}
			return;
		}
		else if (element->Address < address)
		{
			previous = element;
			element = element->Next;
			if (element == NULL)
			{
				// found leaf, insert new element
				previous->Next = AllocElement();
				InitialiseElement(previous->Next, address, symbol);
				element = previous->Next;
				element->Previous = previous;
				return;
			}
			else
			{
				if (element->Address > address)
				{
					// insert between
					struct SymbolElement* tmp = AllocElement();
					InitialiseElement(tmp, address, symbol);
					tmp->Previous = previous;
					previous->Next = tmp;
					tmp->Next = element;
					element->Previous = tmp;
					return;
				}
				//otherwise keep looping
			}
		}
		else // element->Address > address
		{
			previous = element;
			element = element->Previous;
			if (element == NULL)
			{
				// found leaf, insert new element
				previous->Previous = AllocElement();
				InitialiseElement(previous->Previous, address, symbol);
				element = previous->Previous;
				element->Next = previous;
				return;
			}
			else
			{
				if (element->Address < address)
				{
					// insert between
					struct SymbolElement* tmp = AllocElement();
					InitialiseElement(tmp, address, symbol);
					tmp->Next = previous;
					previous->Previous = tmp;
					tmp->Previous = element;
					element->Next = tmp;
					return;
				}
				//otherwise keep looping
			}
		}
	}
}

const char* FetchSymbolForAddress(int address)
{
	struct SymbolElement* element = SymbolHead;
	while (element)
	{
		if (element->Address == address)
		{
			return element->Symbol;
		}
		else if (element->Address < address)
		{
			element = element->Next;
			if (element && element->Address > address)
				break;
		}
		else
		{
			element = element->Previous;
			if (element && element->Address < address)
				break;
		}
	}
	return NULL;
}

void LoadSymbolFile(char* filename)
{
	FILE* symbols = fopen(filename, "r");
	if (symbols == NULL)
		return;

	int state = 0;
	int numSourceLines = 0;
	char lineBuffer[1024];
	while (fgets(lineBuffer, 1024, symbols) != NULL)
	{
		switch (state)
		{
		case 0:	// first line path to source
			state = 1;
			break;
		case 1:
			numSourceLines = atoi(lineBuffer);
			state = 2;
			break;
		case 2:	// Source line information
			numSourceLines--;
			if (numSourceLines == 0)
				state = 3;
			break;
		case 3:	// Symbols
			{
				int address;
				char symbol[1024];
				char* sPtr=symbol;
				sscanf(lineBuffer, "%d\t%s\n", &address, symbol);
				if (strstr(symbol, "prefix") == symbol)
				{
					sPtr += strlen("prefix _");
				}
				InsertSymbol(address, sPtr);
			}
			break;
		}
	}
}

int UpdateMemoryMappedDebuggerViews(int isPaused)
{
	int cmd = 0xff;
	if (isPaused)
	{
		char* tmp2 = (char*)pMapRegisters;
		unsigned int address;
		int code_size = 1;
		switch (curSystem)
		{
		case ESS_CP1:
		case ESS_MSU:
			FETCH_REGISTERS80386(tmp2);
			address = MSU_GETPHYSICAL_EIP();
			code_size = MSU_cSize;
			break;
		case ESS_P88:
		case ESS_P89:
			//address=SEGTOPHYS(CS,IP)&0xFFFFF;
			//sprintf(tmp,"%s:P88%05X\n",pause?"Paused":"Runnin",address);
			FETCH_REGISTERS8086(tmp2);
			address = SEGTOPHYS(CS, IP);
			code_size = 1;
			break;
		case ESS_FL1:
			//			address = Z80_PC;// GetZ80LinearAddress() & 0xFFFFF;		// disassembly expects address in chip space not linear space
						//sprintf(tmp,"%s:FL1%05X\n",pause?"Paused":"Runnin",address);
			FETCH_REGISTERSZ80(tmp2);
			address = getZ80LinearAddress();
			break;
		}

		char* tmp = (char*)pMapDisassm;
		char tBuffer[2048];

		strcpy(tmp, "");
		int spacer = 15;
		for (int l = 0; l < 20; l++)
		{
			InStream disMe;
			disMe.findSymbol = FetchSymbolForAddress;
			if (curSystem == ESS_FL1)
			{
				disMe.cpu = CPU_Z80;
				spacer = 6;
			}
			else
			{
				disMe.cpu = CPU_X86;
				spacer = 15;
			}
			disMe.bytesRead = 0;
			disMe.curAddress = address;
			disMe.useAddress = 1;
			disMe.PeekByte = PeekByte;
			Disassemble(&disMe, MSU_cSize);
			int a;

			if (disMe.bytesRead != 0)
			{
				const char* symbol = FetchSymbolForAddress(address);

				if (symbol == NULL && address > 0xFFFF)
					symbol = FetchSymbolForAddress(address & 0xFFFF);
				if (symbol)
				{
					sprintf(tBuffer, "%s:\n", symbol);
					strcat(tmp, tBuffer);
				}

				sprintf(tBuffer, "%06X : ", address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see
				strcat(tmp, tBuffer);

				for (a = 0; a < disMe.bytesRead; a++)
				{
					sprintf(tBuffer, "%02X ", PeekByte(address + a));
					strcat(tmp, tBuffer);
				}
				for (a = 0; a < spacer - disMe.bytesRead; a++)
				{
					strcat(tmp, "   ");
				}
				sprintf(tBuffer, "%s\n", (char*)GetOutputBuffer());
				strcat(tmp, tBuffer);

				address += disMe.bytesRead;
				tmp += strlen(tmp);
				*tmp = 0;
			}
			else
				break;
		}
		*tmp = 0;

		const char** WPort = NULL;
		const char** RPort = NULL;
		int regCnt = 128;
		int portWidth = 2;
		switch (curSystem)
		{
		case ESS_P88:
		case ESS_P89:
			break;
		case ESS_CP1:
			WPort = CP1_WritePortInfo;
			RPort = CP1_ReadPortInfo;
			break;
		case ESS_MSU:
			WPort = MSU_WritePortInfo;
			RPort = MSU_ReadPortInfo;
			break;
		case ESS_FL1:
			WPort = FL1_WritePortInfo;
			RPort = FL1_ReadPortInfo;
			portWidth = 1;
			regCnt = 256;
			break;
		}

		if (WPort != NULL && RPort != NULL)
		{
			tmp = (char*)pMapASIC;
			for (int a = 0; a < regCnt; a++)
			{
				char* ptr = tBuffer;
				*ptr = 0;
				if (WPort[a] != 0 || RPort[a] != 0)
				{
					if (portWidth == 2)
						sprintf(ptr, "%02X : ", a * 2);
					else
						sprintf(ptr, "%02X : ", a);
					ptr += strlen(ptr);
					if (WPort[a])
					{
						if (portWidth == 2)
							sprintf(ptr, "%04X %s", ASIC_DEB_PORTS[a], WPort[a]);
						else
							sprintf(ptr, "%02X %s", ASIC_DEB_BYTE_PORT_WRITE[a], WPort[a]);
						ptr += strlen(ptr);
					}
					else
					{
						if (portWidth == 2)
							sprintf(ptr, "\t\t");
						else
							sprintf(ptr, "\t");
						ptr += strlen(ptr);
					}
					if (RPort[a])
					{
						if (portWidth == 2)
							sprintf(ptr, "\t(%04X %s)\n", GetPortW(a * 2), RPort[a]);
						else
							sprintf(ptr, "\t(%02X %s)\n", 0xFF & GetPortB(a), RPort[a]);
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
		}


		// DSP Registers
		switch (curSystem)
		{
		case ESS_FL1:
			FL1_DSP_REGISTERS(pMapDSPRegisters);
			FL1_DSP_DISASSEMBLE(pMapDSP);
			break;
		}
	}

	if (pMapControl[0] != pMapControl[1])
	{
		cmd = pMapControl[0];
		pMapControl[1] = pMapControl[0];
	}
	return cmd;
}

extern uint16_t FL1DSP_DEBUG_FETCH(uint32_t reg, uint16_t offset);

void FL1_DSP_REGISTERS(char* output)
{
	char tmp[1024];
	sprintf(tmp, "IR               %04X\n", FL1DSP_DEBUG_FETCH(2,0)); strcpy(output, tmp);
	sprintf(tmp, "NextInstruction  %04X\n", FL1DSP_DEBUG_FETCH(3,0)); strcat(output, tmp);
	sprintf(tmp, "DataAddress      %04X\n", FL1DSP_DEBUG_FETCH(4,0)); strcat(output, tmp);
	sprintf(tmp, "PrevDataAddress  %04X\n\t\n", FL1DSP_DEBUG_FETCH(5,0)); strcat(output, tmp);
	sprintf(tmp, "PC               %04X\n", FL1DSP_DEBUG_FETCH(6,0)&0x7FF); strcat(output, tmp);
	sprintf(tmp, "MZ0              %04X\n", FL1DSP_DEBUG_FETCH(7,0)); strcat(output, tmp);
	sprintf(tmp, "MZ1              %04X\n", FL1DSP_DEBUG_FETCH(8,0)); strcat(output, tmp);
	sprintf(tmp, "MX               %04X\n", FL1DSP_DEBUG_FETCH(9,0)); strcat(output, tmp);
	sprintf(tmp, "MY               %04X\n", FL1DSP_DEBUG_FETCH(10,0)); strcat(output, tmp);
	sprintf(tmp, "AX               %04X\n", FL1DSP_DEBUG_FETCH(16,0)); strcat(output, tmp);
	sprintf(tmp, "AZ               %04X\n", FL1DSP_DEBUG_FETCH(17,0)); strcat(output, tmp);
	sprintf(tmp, "C                %04X\n", FL1DSP_DEBUG_FETCH(18,0)); strcat(output, tmp);
	sprintf(tmp, "CMPR             %04X\n\t\n", FL1DSP_DEBUG_FETCH(11,0)); strcat(output, tmp);
	sprintf(tmp, "DMA0             %04X\n", FL1DSP_DEBUG_FETCH(12,0)); strcat(output, tmp);
	sprintf(tmp, "DMA1             %04X\n", FL1DSP_DEBUG_FETCH(13,0)); strcat(output, tmp);
	sprintf(tmp, "DMD              %04X\n", FL1DSP_DEBUG_FETCH(14,0)); strcat(output, tmp);
	sprintf(tmp, "INTRA            %04X\n", FL1DSP_DEBUG_FETCH(15,0)); strcat(output, tmp);
}

extern uint16_t	FL1DSP_DEBUG_PC;

const char* FL1DSP_decodeDisasm(uint8_t* table[32], uint16_t word);
extern uint8_t *FL1DSP_DIS_[32];			// FROM EDL

void FL1_DSP_DISASSEMBLE(char* output)
{
	char tmp[1024];
	unsigned int address = 0x800+FL1DSP_DEBUG_PC;

	const char* retVal = FL1DSP_decodeDisasm(FL1DSP_DIS_, FL1DSP_DEBUG_FETCH(2, 0));
	sprintf(tmp, "LAST :     \t%s\n", retVal);
	strcpy(output, tmp);
	retVal = FL1DSP_decodeDisasm(FL1DSP_DIS_, FL1DSP_DEBUG_FETCH(3, 0));
	sprintf(tmp, "NEXT :     \t%s\n\t\n", retVal);
	strcat(output, tmp);
	for (int a = 0; a < 16; a++)
	{
		uint16_t word = FL1DSP_PEEK(address);
		retVal = FL1DSP_decodeDisasm(FL1DSP_DIS_, word);
		sprintf(tmp, "%04X : %04X\t%s\n", address&0x7FF, FL1DSP_PEEK(address), retVal);
		strcat(output, tmp);
		address++;
		address &= 0xFFF;
	}
}

const char* FL1DSP_decodeDisasm(uint8_t* table[32], uint16_t word)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];
	uint16_t data=word&0x7FF;
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
			sprintf(sprintBuffer,"%03X (%04X)",data,FL1DSP_PEEK(data));
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

#endif
