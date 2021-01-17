#ifndef _PDS_H
#define _PDS_H

uint32_t PDS_GETPHYSICAL_EIP();
void PDS_INTERRUPT(uint8_t);
void PDS_STEP(void);
void PDS_RESET(void);

extern uint8_t *PDS_DIS_[256];				// FROM EDL
extern uint32_t PDS_DIS_max_;				// FROM EDL
extern uint8_t *PDS_DIS_TABLE_DECINC_MOD[256];		// FROM EDL
extern uint32_t PDS_DIS_max_TABLE_DECINC_MOD;		// FROM EDL
extern uint8_t *PDS_DIS_TABLE_OP_MOD_IMM[256];		// FROM EDL
extern uint32_t PDS_DIS_max_TABLE_OP_MOD_IMM;		// FROM EDL
extern uint8_t *PDS_DIS_TABLE_SH1_MOD[256];		// FROM EDL
extern uint32_t PDS_DIS_max_TABLE_SH1_MOD;		// FROM EDL
extern uint8_t *PDS_DIS_TABLE_SHV_MOD[256];		// FROM EDL
extern uint32_t PDS_DIS_max_TABLE_SHV_MOD;		// FROM EDL
extern uint8_t *PDS_DIS_TABLE_SOP_MOD[256];		// FROM EDL
extern uint32_t PDS_DIS_max_TABLE_SOP_MOD;		// FROM EDL
extern uint8_t *PDS_DIS_TABLE_OP_MOD[256];		// FROM EDL
extern uint32_t PDS_DIS_max_TABLE_OP_MOD;		// FROM EDL

extern uint32_t	PDS_EAX;				// FROM EDL
extern uint32_t	PDS_EBX;				// FROM EDL
extern uint32_t	PDS_ECX;				// FROM EDL
extern uint32_t	PDS_EDX;				// FROM EDL
extern uint32_t	PDS_ESP;				// FROM EDL
extern uint32_t	PDS_EBP;				// FROM EDL
extern uint32_t	PDS_ESI;				// FROM EDL
extern uint32_t	PDS_EDI;				// FROM EDL
extern uint32_t	PDS_CR0;				// FROM EDL
extern uint32_t	PDS_DR7;				// FROM EDL
extern uint16_t	PDS_CS;					// FROM EDL
extern uint16_t	PDS_DS;					// FROM EDL
extern uint16_t	PDS_ES;					// FROM EDL
extern uint16_t	PDS_SS;					// FROM EDL
extern uint16_t	PDS_FS;					// FROM EDL
extern uint16_t	PDS_GS;					// FROM EDL
extern uint32_t	PDS_EIP;				// FROM EDL
extern uint16_t PDS_EFLAGS;				// FROM EDL
extern uint16_t PDS_CYCLES;				// FROM EDL
extern uint8_t PDS_cSize;

extern uint32_t PDS_SegBase[8];


#endif//_PDS_H
