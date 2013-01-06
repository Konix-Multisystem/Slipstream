/*

 Debugger interfaces

*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "system.h"
#include "logfile.h"
#include "debugger.h"
#include "memory.h"

int doDebug=1;
int doShowPortStuff=0;
uint32_t doDebugTrapWriteAt=0xFFFFF;
int debugWatchWrites=0;
int debugWatchReads=0;

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
		case ESS_FL1:
			CONSOLE_OUTPUT("PORT WRITE UNKNOWN (%04X)- TODO\n",port);
			exit(-1);
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
		case ESS_FL1:
			CONSOLE_OUTPUT("PORT READ UNKNOWN (%04X)- TODO\n",port);
			exit(-1);
			break;
	}
#endif
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
			*cnt=2;
			sprintf(tmpBuffer,"%02X%02X[%s]",PeekByte(address+1),PeekByte(address),modregs[modrm&7]);
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

int DoModRegRMDW(int direc,int word,uint32_t address,char** tPtr)
{
	char tmpBuffer[256];
	// Extract register,EA
	int nextByte=PeekByte(address);
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

int DoModRegRM(uint8_t op,uint32_t address,char** tPtr)
{
	int word=op&1;
	int direc=op&2;

	return DoModRegRMDW(direc,word,address,tPtr);
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
	static char segOveride2[2048];
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
			strcpy(segOveride2,mnemonic);
			strcat(segOveride2," ");
			strcat(segOveride2,sPtr);
			return segOveride2;
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
						if (strcmp(tmpCommand,"MODleaRM")==0)
						{
							counting+=1+DoModRegRMDW(1,1,address+counting+1,&dPtr);
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
						if ((strcmp(tmpCommand,"IMM8")==0)||(strcmp(tmpCommand,"VECTOR")==0))
						{
							sprintf(sprintBuffer,"#%02X",PeekByte(address+counting+1));
							counting+=1;
							tPtr=sprintBuffer;
							while (*tPtr)
							{
								*dPtr++=*tPtr++;
							}
						}
						else
						if (strcmp(tmpCommand,"IMM16")==0)
						{
							sprintf(sprintBuffer,"#%02X%02X",PeekByte(address+counting+2),PeekByte(address+counting+1));
							counting+=2;
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
						if (strcmp(tmpCommand,"FAR")==0)
						{
							sprintf(sprintBuffer,"[%02X%02X:%02X%02X]",PeekByte(address+counting+2),PeekByte(address+counting+1),PeekByte(address+counting+4),PeekByte(address+counting+3));
							counting+=4;
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

int Disassemble8086(unsigned int address,int registers)
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

#endif

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


