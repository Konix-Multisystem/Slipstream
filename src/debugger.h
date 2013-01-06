#ifndef _DEBUGGER__H
#define _DEBUGGER__H

extern int doDebug;
extern int doShowPortStuff;
extern uint32_t doDebugTrapWriteAt;
extern int debugWatchWrites;
extern int debugWatchReads;
extern int doShowBlits;

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


uint8_t PeekByte(uint32_t addr);
void DebugWPort(uint16_t port);
void DebugRPort(uint16_t port);

int Disassemble8086(unsigned int address,int registers);

#endif//_DEBUGGER__H

