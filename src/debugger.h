#ifndef _DEBUGGER__H
#define _DEBUGGER__H

extern int doDebug;
extern int doShowPortStuff;
extern uint32_t doDebugTrapWriteAt;
extern int debugWatchWrites;
extern int debugWatchReads;
extern int doShowBlits;

extern uint8_t *MSU_DIS_[256];				// FROM EDL
extern uint32_t MSU_DIS_max_;				// FROM EDL
extern uint8_t *MSU_DIS_TABLE_DECINC_MOD[256];		// FROM EDL
extern uint32_t MSU_DIS_max_TABLE_DECINC_MOD;		// FROM EDL
extern uint8_t *MSU_DIS_TABLE_OP_MOD_IMM[256];		// FROM EDL
extern uint32_t MSU_DIS_max_TABLE_OP_MOD_IMM;		// FROM EDL
extern uint8_t *MSU_DIS_TABLE_SH1_MOD[256];		// FROM EDL
extern uint32_t MSU_DIS_max_TABLE_SH1_MOD;		// FROM EDL
extern uint8_t *MSU_DIS_TABLE_SHV_MOD[256];		// FROM EDL
extern uint32_t MSU_DIS_max_TABLE_SHV_MOD;		// FROM EDL
extern uint8_t *MSU_DIS_TABLE_SOP_MOD[256];		// FROM EDL
extern uint32_t MSU_DIS_max_TABLE_SOP_MOD;		// FROM EDL
extern uint8_t *MSU_DIS_TABLE_OP_MOD[256];		// FROM EDL
extern uint32_t MSU_DIS_max_TABLE_OP_MOD;		// FROM EDL

extern uint32_t	MSU_EAX;				// FROM EDL
extern uint32_t	MSU_EBX;				// FROM EDL
extern uint32_t	MSU_ECX;				// FROM EDL
extern uint32_t	MSU_EDX;				// FROM EDL
extern uint32_t	MSU_ESP;				// FROM EDL
extern uint32_t	MSU_EBP;				// FROM EDL
extern uint32_t	MSU_ESI;				// FROM EDL
extern uint32_t	MSU_EDI;				// FROM EDL
extern uint32_t	MSU_CR0;				// FROM EDL
extern uint16_t	MSU_CS;					// FROM EDL
extern uint16_t	MSU_DS;					// FROM EDL
extern uint16_t	MSU_ES;					// FROM EDL
extern uint16_t	MSU_SS;					// FROM EDL
extern uint16_t	MSU_FS;					// FROM EDL
extern uint16_t	MSU_GS;					// FROM EDL
extern uint16_t	MSU_EIP;				// FROM EDL
extern uint16_t MSU_EFLAGS;				// FROM EDL
extern uint16_t MSU_CYCLES;				// FROM EDL
extern uint32_t MSU_GETPHYSICAL_EIP();

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

extern uint16_t	AX;					// FROM EDL
extern uint16_t	BX;					// FROM EDL
extern uint16_t	CX;					// FROM EDL
extern uint16_t	DX;					// FROM EDL
extern uint16_t	SP;					// FROM EDL
extern uint16_t	BP;					// FROM EDL
extern uint16_t	SI;					// FROM EDL
extern uint16_t	DI;					// FROM EDL
extern uint16_t	CS;					// FROM EDL
extern uint16_t	DS;					// FROM EDL
extern uint16_t	ES;					// FROM EDL
extern uint16_t	SS;					// FROM EDL
extern uint16_t	IP;					// FROM EDL
extern uint16_t FLAGS;					// FROM EDL
extern uint16_t	PC;					// FROM EDL
extern uint16_t CYCLES;					// FROM EDL

extern uint8_t *Z80_DIS_[256];				// FROM EDL
extern uint32_t Z80_DIS_max_;				// FROM EDL
extern uint8_t *Z80_DIS_CB[256];			// FROM EDL
extern uint32_t Z80_DIS_max_CB;				// FROM EDL
extern uint8_t *Z80_DIS_DD[256];			// FROM EDL
extern uint32_t Z80_DIS_max_DD;				// FROM EDL
extern uint8_t *Z80_DIS_DDCB[256];			// FROM EDL
extern uint32_t Z80_DIS_max_DDCB;			// FROM EDL
extern uint8_t *Z80_DIS_ED[256];			// FROM EDL
extern uint32_t Z80_DIS_max_ED;				// FROM EDL
extern uint8_t *Z80_DIS_FD[256];			// FROM EDL
extern uint32_t Z80_DIS_max_FD;				// FROM EDL
extern uint8_t *Z80_DIS_FDCB[256];			// FROM EDL
extern uint32_t Z80_DIS_max_FDCB;			// FROM EDL

extern uint16_t	Z80_AF;					// FROM EDL
extern uint16_t	Z80_BC;					// FROM EDL
extern uint16_t	Z80_DE;					// FROM EDL
extern uint16_t	Z80_HL;					// FROM EDL
extern uint16_t	Z80__AF;				// FROM EDL
extern uint16_t	Z80__BC;				// FROM EDL
extern uint16_t	Z80__DE;				// FROM EDL
extern uint16_t	Z80__HL;				// FROM EDL
extern uint16_t	Z80_IX;					// FROM EDL
extern uint16_t	Z80_IY;					// FROM EDL
extern uint16_t	Z80_PC;					// FROM EDL
extern uint16_t	Z80_SP;					// FROM EDL
extern uint16_t	Z80_IR;					// FROM EDL

extern uint8_t Z80_IM;					// FROM EDL
extern uint8_t Z80_IFF1;				// FROM EDL
extern uint8_t Z80_IFF2;				// FROM EDL

extern uint8_t Z80_HALTED;				// FROM EDL
extern uint8_t Z80_CYCLES;				// FROM EDL


uint8_t PeekByte(uint32_t addr);
void DebugWPort(uint16_t port);
void DebugRPort(uint16_t port);

int Disassemble80386(unsigned int address,int registers);
int Disassemble8086(unsigned int address,int registers);
int DisassembleZ80(unsigned int address,int registers);

void FETCH_REGISTERS8086(char* tmp);
void FETCH_REGISTERSZ80(char* tmp);

int FETCH_DISASSEMBLE8086(unsigned int address,char* tmp);
int FETCH_DISASSEMBLEZ80(unsigned int address,char* tmp);

#endif//_DEBUGGER__H

