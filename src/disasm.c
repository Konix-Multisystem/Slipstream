/*

 Standalone x86 disassembler - Since complexity of automated disassembler generation on x86 processors from EDL is proving awkward.
			      This provides a replacement.

 Intel x32 / x64 disassembler

*/

#include <stdio.h>
#include <string.h>

#include "disasm.h"

/*

A Direct address: the instruction has no ModR/M byte; the address of the operand is encoded in the instruction.
	No base register, index register, or scaling factor can be applied (for example, far JMP (EA)).
B The VEX.vvvv field of the VEX prefix selects a general purpose register.
C The reg field of the ModR/M byte selects a control register (for example, MOV (0F20, 0F22)).

D The reg field of the ModR/M byte selects a debug register (for example,
	MOV (0F21,0F23)).
E A ModR/M byte follows the opcode and specifies the operand. The operand is either a general-purpose
	register or a memory address. If it is a memory address, the address is computed from a segment register
	and any of the following values: a base register, an index register, a scaling factor, a displacement.
F EFLAGS/RFLAGS Register.
G The reg field of the ModR/M byte selects a general register (for example, AX (000)).
H The VEX.vvvv field of the VEX prefix selects a 128-bit XMM register or a 256-bit YMM register, determined
	by operand type. For legacy SSE encodings this operand does not exist, changing the instruction to
	destructive form.
I Immediate data: the operand value is encoded in subsequent bytes of the instruction.
J The instruction contains a relative offset to be added to the instruction pointer register (for example, JMP
	(0E9), LOOP).
L The upper 4 bits of the 8-bit immediate selects a 128-bit XMM register or a 256-bit YMM register, determined
	by operand type. (the MSB is ignored in 32-bit mode)
M The ModR/M byte may refer only to memory (for example, BOUND, LES, LDS, LSS, LFS, LGS,
	CMPXCHG8B).
N The R/M field of the ModR/M byte selects a packed-quadword, MMX technology register.
O The instruction has no ModR/M byte. The offset of the operand is coded as a word or double word
	(depending on address size attribute) in the instruction. No base register, index register, or scaling factor
	can be applied (for example, MOV (A0–A3)).
P The reg field of the ModR/M byte selects a packed quadword MMX technology register.
Q A ModR/M byte follows the opcode and specifies the operand. The operand is either an MMX technology
	register or a memory address. If it is a memory address, the address is computed from a segment register
	and any of the following values: a base register, an index register, a scaling factor, and a displacement.
R The R/M field of the ModR/M byte may refer only to a general register (for example, MOV (0F20-0F23)).
S The reg field of the ModR/M byte selects a segment register (for example, MOV (8C,8E)).
U The R/M field of the ModR/M byte selects a 128-bit XMM register or a 256-bit YMM register, determined by
	operand type.
V The reg field of the ModR/M byte selects a 128-bit XMM register or a 256-bit YMM register, determined by
	operand type.
W A ModR/M byte follows the opcode and specifies the operand. The operand is either a 128-bit XMM register,
	a 256-bit YMM register (determined by operand type), or a memory address. If it is a memory address, the
	address is computed from a segment register and any of the following values: a base register, an index
	register, a scaling factor, and a displacement.
X Memory addressed by the DS:rSI register pair (for example, MOVS, CMPS, OUTS, or LODS).
Y Memory addressed by the ES:rDI register pair (for example, MOVS, CMPS, INS, STOS, or SCAS).

a Two one-word operands in memory or two double-word operands in memory, depending on operand-size
	attribute (used only by the BOUND instruction).
b Byte, regardless of operand-size attribute.
c Byte or word, depending on operand-size attribute.
d Doubleword, regardless of operand-size attribute.
dq Double-quadword, regardless of operand-size attribute.
p 32-bit, 48-bit, or 80-bit pointer, depending on operand-size attribute.
pd 128-bit or 256-bit packed double-precision floating-point data.
pi Quadword MMX technology register (for example: mm0).
ps 128-bit or 256-bit packed single-precision floating-point data.
q Quadword, regardless of operand-size attribute.
qq Quad-Quadword (256-bits), regardless of operand-size attribute.
s 6-byte or 10-byte pseudo-descriptor.
sd Scalar element of a 128-bit double-precision floating data.
ss Scalar element of a 128-bit single-precision floating data.
si Doubleword integer register (for example: eax).
v Word, doubleword or quadword (in 64-bit mode), depending on operand-size attribute.
w Word, regardless of operand-size attribute.
x dq or qq based on the operand-size attribute.
y Doubleword or quadword (in 64-bit mode), depending on operand-size attribute.
z Word for 16-bit operand-size or doubleword for 32 or 64-bit operand-size.


Additional flags section

1A Bits 5, 4, and 3 of ModR/M byte used as an opcode extension (refer to Section A.4, “Opcode Extensions For One-Byte
	And Two-byte Opcodes”).
1B Use the 0F0B opcode (UD2 instruction) or the 0FB9H opcode when deliberately trying to generate an invalid opcode
	exception (#UD).
1C Some instructions use the same two-byte opcode. If the instruction has variations, or the opcode represents
	different instructions, the ModR/M byte will be used to differentiate the instruction. For the value of the ModR/M
	byte needed to decode the instruction, see Table A-6.
i64 The instruction is invalid or not encodable in 64-bit mode. 40 through 4F (single-byte INC and DEC) are REX prefix
	combinations when in 64-bit mode (use FE/FF Grp 4 and 5 for INC and DEC).
o64 Instruction is only available when in 64-bit mode.
d64 When in 64-bit mode, instruction defaults to 64-bit operand size and cannot encode 32-bit operand size.
f64 The operand size is forced to a 64-bit operand size when in 64-bit mode (prefixes that change operand size are
	ignored for this instruction in 64-bit mode).
v VEX form only exists. There is no legacy SSE form of the instruction. For Integer GPR instructions it means VEX
	prefix required.
v1 VEX128 & SSE forms only exist (no VEX256), when can’t be inferred from the data size.


So something like this :

"ADD"	0x00	Eb,Gb	

	ADD  opcode 0x00 2 arguments, first Mod/RM (always byte mode), second reg field of mod r/m = register (hint modR/M is mm111222 m=mod 111 = reg src 222 = reg dst

	Ex 00 00 is ADD BYTE PTR [EAX],al
	   00 C0 is ADD al,al
	   00 40 12 is ADD BYTE PTR [EAX+0x12],al
	   00 80 78 56 34 12 is ADD BYTE PTR [EAX+0x12345678],al

"ADD"	0x01	Ev,Gv

*/



/* Intel Format
Prefixes: 0-4 bytes
Opcode: 1-2 bytes
ModR/M: 1 byte
SIB: 1 byte
Displacement: 1 byte or word
Immediate: 1 byte or word
*/

enum OperandFlags		// Strictly speaking these are instruction flags
{
	OF_None = 0,
	OF_Terminator = 1<<0,
	OF_Branch = 1<<1,
	OF_CondBranch = 1<<2,
};

enum OperandMode
{
	OM_MODRM,				// Indicates prescence of MODRM byte
	OM_Precision66,				// (S or D)Precision is decided by 0x66 prefix
	OM_Precision66F3F2,			// Precision is decided by 0x66,0xF3,0xF2 prefix  (PS/PD/SS/SD)
	OM_Cd,					// reg field is control register
	OM_Dd,					// reg field is debug register
	OM_Eb,					// byte Memory from MODRM
	OM_Ep,					// FAR Ptr
	OM_Ev,					// 16/32/64 Memory from MODRM
	OM_Ew,					// 16 bit memory reference from MODRM
	OM_Gb,					// byte Register from MODRM
	OM_Gv,					// 16/32/64 register from MODRM
	OM_Gw,					// 16 bit register from MODRM
	OM_Gy,					// 32 or 64 bit register from MODRM
	OM_Gz,					// 16/32 bit register from MODRM
	OM_AL,					// byte register 0
	OM_eAX,					// 16/32 register 0
	OM_CL,					// byte register 1
	OM_Ib,					// Imm8
	OM_Iv,					// Imm32/16/64
	OM_Iw,					// Imm16
	OM_Iz,					// Imm32/16
	OM_ES,					// ES
	OM_CS,					// CS
	OM_SS,					// SS
	OM_DS,					// DS
	OM_FS,					// FS
	OM_GS,					// GS
	OM_GPRb,				// bottom 3 bits of opcode represent 8bit GPR
	OM_GPR,					// bottom 3 bits of opcode represent 32/16 GPR
	OM_FAR,					// 16bit Segment : 32/16 bit ptr
	OM_Ma,					// must be memory 16/32 (reg reg disallowed) from MODRM
	OM_M,					// must be memory 16/32 (reg reg disallowed) hide PTR from MODRM
	OM_Mp,					// must be memory (FAR Ptr) from MODRM
	OM_Mf,					// must be memory (FWORD Ptr) from MODRM
	OM_Rd,					// rm field is 32 bit gpr
	OM_Sw,					// 16 bit Segment register from MODRM
	OM_DX,					// DX
	OM_Yb,					// BYTE PTR ES:[EDI]
	OM_Yv,					// DWORD/WORD/QWORD PTR ES:[EDI]
	OM_Yz,					// DWORD/WORD PTR ES:[EDI]
	OM_Xb,					// BYTE PTR DS:[ESI]
	OM_Xv,					// DWORD/WORD/QWORD PTR DS:[ESI]
	OM_Xz,					// DWORD/WORD PTR DS:[ESI]
	OM_cc,					// bottom 4 bits indicate condition code
	OM_Jb,					// Imm8 displacement
	OM_Jz,					// Imm32/16 displacement
	OM_Ob,					// offset 16/32
	OM_Ov,					// offset 16/32
	OM_Ups,					// Reg field of MODRM is xmm register
	OM_Vps,					// Reg field of MODRM is xmm register
	OM_Wps,					// Memory field (128 bit pointer or xmm register)
	OM_Grp1_0,				// Grp1_0 from MODRM
	OM_Grp1_1,				// Grp1_1 from MODRM
	OM_Grp1_2,				// Grp1_2 from MODRM
	OM_Grp1A,				// Grp1A from MODRM
	OM_Grp2_0,				// Grp2_0 from MODRM
	OM_Grp2_1,				// Grp2_1 from MODRM
	OM_Grp2_2,				// Grp2_2 from MODRM
	OM_Grp2_3,				// Grp2_3 from MODRM
	OM_Grp2_4,				// Grp2_4 from MODRM
	OM_Grp2_5,				// Grp2_5 from MODRM
	OM_Grp3_0,				// Grp3_0 from MODRM
	OM_Grp3_1,				// Grp3_1 from MODRM
	OM_Grp4,				// Grp4 from MODRM
	OM_Grp5,				// Grp5 from MODRM
	OM_Grp7,				// Grp7 from MODRM
	OM_Grp11_0,				// Grp11_0 from MODRM
	OM_Grp11_1,				// Grp11_1 from MODRM
	OM_Grp12,				// Grp12 from MODRM
	OM_WDEBW,				// 32bit - CWDE / 16bit CBW
	OM_DQWD,				// 32bit - CDQ / 16bit CWD
	OM_XLAT,				// BYTE PTR DS:[EBX]
	OM_SPACE,				// Add a space to the disassembly
	OM_ONE,
	OM_HasW,				// W postfix added to mnemonic when word sized (for when operand followed by a space)
	OM_JustW,				// W postfix added to mnemonic when word sized (for when operand not followed by a space)

	OM_LOCK,				// LOCK PREFIX
	OM_REPNE,				// REPNE PREFIX
	OM_REP,					// REP PREFIX

	OM_NoOperands,				// No operands

	OM_Illegal,				// Illegal instruction

	OM_FPUEscape,				// D8-DF FPU Escape codes
	OM_OpcodeExtensionByte,			// not an operand - flag for disassembler

	OM_OSize,				// Operand Size Prefix
	OM_MSize,				// Memory Size Prefix
};

typedef enum OperandMode OperandMode;

unsigned char GetNextByteFromStream(InStream* stream)
{
	unsigned char byte = PeekByte(stream->curAddress);
	stream->bytesRead++;
	stream->curAddress++;
	return byte;
}

unsigned char PeekByteFromStreamOffset(InStream* stream, int offset)
{
    unsigned char byte = PeekByte(stream->curAddress + offset);
    return byte;
}

#define TOTAL_OPERANDS	(6)

struct Table
{
	unsigned char mnemonic;
	unsigned char flags;
	unsigned char operands[TOTAL_OPERANDS];			// Enough?
};

typedef struct Table Table;

#define NUM_OPS	(256)

const char* Mnemonics[]= 		{
						"",			//0 - Used to indicate prefix/2byte etc
						"ADD ",			//1
						"PUSH ",		//2
						"POP ",			//3
						"OR ",			//4
						"ADC ",			//5
						"SBB ",			//6
						"AND ",			//7
						"DAA",			//8
						"SUB ",			//9
						"DAS",			//10
						"XOR ",			//11
						"AAA",			//12
						"CMP ",			//13
						"AAS",			//14
						"INC ",			//15
						"DEC ",			//16
						"PUSHA",		//17
						"POPA",			//18
						"BOUND ",		//19
						"ARPL ",		//20
						"IMUL ",		//21
						"INS ",			//22
						"OUTS ",		//23
						"J",			//24
						"",			//25 Group Extensions
						"TEST ",		//26
						"XCHG ",		//27
						"MOV ",			//28
						"LEA ",			//29
						"C",			//30
						"CALL ",		//31
						"FWAIT",		//32
						"PUSHF",		//33
						"POPF",			//34
						"SAHF",			//35
						"LAHF",			//36
						"MOVS ",		//37
						"CMPS ",		//38
						"STOS ",		//39
						"LODS ",		//40
						"SCAS ",		//41
						"RET",			//42
						"LES ",			//43
						"LDS ",			//44
						"ENTER ",		//45
						"LEAVE",		//46
						"RETF",			//47
						"INT3",			//48
						"INT",			//49
						"INTO",			//50
						"IRET",			//51
						"AAM ",			//52
						"AAD ",			//53
						"XLAT",			//54
						"LOOPNE ",		//55
						"LOOPE ",		//56
						"LOOP ",		//57
						"JECXZ ",		//58
						"IN ",			//59
						"OUT ",			//60
						"JMP ",			//61
						"HLT",			//62
						"CMC",			//63
						"CLC",			//64
						"STC",			//65
						"CLI",			//66
						"STI",			//67
						"CLD",			//68
						"STD",			//69
						"MOVZX ",		//70
						"CMOV",			//71
						"VMOVAP",		//72
						"VSHUFP",		//73
						"VMOVMSKP",		//74
						"VSUB",			//75
						"VDIV",			//76
						"VMUL",			//77
						"VADD",			//78
                        "WBINVD",		//79
					};

const char* grp1Mnemonics[8]=	{
					"ADD",
					"OR",
					"ADC",
					"SBB",
					"AND",
					"SUB",
					"XOR",
					"CMP"
				};

Table grp1EbIb[8]=	{
				{ 0, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
				{ 1, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
				{ 2, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
				{ 3, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
				{ 4, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
				{ 5, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
				{ 6, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
				{ 7, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } }
			};

Table grp1EvIz[8]=	{
				{ 0, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } },
				{ 1, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } },
				{ 2, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } },
				{ 3, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } },
				{ 4, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } },
				{ 5, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } },
				{ 6, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } },
				{ 7, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } }
			};

Table grp1EvIb[8]=	{
				{ 0, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
				{ 1, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
				{ 2, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
				{ 3, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
				{ 4, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
				{ 5, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
				{ 6, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
				{ 7, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } }
			};

const char* grp1AMnemonics[8]=	{
					"POP",
					"",
					"",
					"",
					"",
					"",
					"",
					""
				};

Table grp1A[8]=	{
			{ 0, OF_None , { OM_Ev, OM_NoOperands } },
			{ 1, OF_None , { OM_Illegal } },
			{ 2, OF_None , { OM_Illegal } },
			{ 3, OF_None , { OM_Illegal } },
			{ 4, OF_None , { OM_Illegal } },
			{ 5, OF_None , { OM_Illegal } },
			{ 6, OF_None , { OM_Illegal } },
			{ 7, OF_None , { OM_Illegal } }
		};

const char* grp2Mnemonics[8]=	{
					"ROL",
					"ROR",
					"RCL",
					"RCR",
					"SHL",
					"SHR",
					"",
					"SAR"
				};

Table grp2EbIb[8]=	{
			{ 0, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
			{ 1, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
			{ 2, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
			{ 3, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
			{ 4, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
			{ 5, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
			{ 6, OF_None , { OM_Illegal } },
			{ 7, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } }
		};

Table grp2EvIb[8]=	{
			{ 0, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
			{ 1, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
			{ 2, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
			{ 3, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
			{ 4, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
			{ 5, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } },
			{ 6, OF_None , { OM_Illegal } },
			{ 7, OF_None , { OM_Ev, OM_Ib, OM_NoOperands } }
		};

Table grp2Eb1[8]=	{
			{ 0, OF_None , { OM_Eb, OM_ONE, OM_NoOperands } },
			{ 1, OF_None , { OM_Eb, OM_ONE, OM_NoOperands } },
			{ 2, OF_None , { OM_Eb, OM_ONE, OM_NoOperands } },
			{ 3, OF_None , { OM_Eb, OM_ONE, OM_NoOperands } },
			{ 4, OF_None , { OM_Eb, OM_ONE, OM_NoOperands } },
			{ 5, OF_None , { OM_Eb, OM_ONE, OM_NoOperands } },
			{ 6, OF_None , { OM_Illegal } },
			{ 7, OF_None , { OM_Eb, OM_ONE, OM_NoOperands } }
		};

Table grp2Ev1[8]=	{
			{ 0, OF_None , { OM_Ev, OM_ONE, OM_NoOperands } },
			{ 1, OF_None , { OM_Ev, OM_ONE, OM_NoOperands } },
			{ 2, OF_None , { OM_Ev, OM_ONE, OM_NoOperands } },
			{ 3, OF_None , { OM_Ev, OM_ONE, OM_NoOperands } },
			{ 4, OF_None , { OM_Ev, OM_ONE, OM_NoOperands } },
			{ 5, OF_None , { OM_Ev, OM_ONE, OM_NoOperands } },
			{ 6, OF_None , { OM_Illegal } },
			{ 7, OF_None , { OM_Ev, OM_ONE, OM_NoOperands } }
		};

Table grp2EbCL[8]=	{
			{ 0, OF_None , { OM_Eb, OM_CL, OM_NoOperands } },
			{ 1, OF_None , { OM_Eb, OM_CL, OM_NoOperands } },
			{ 2, OF_None , { OM_Eb, OM_CL, OM_NoOperands } },
			{ 3, OF_None , { OM_Eb, OM_CL, OM_NoOperands } },
			{ 4, OF_None , { OM_Eb, OM_CL, OM_NoOperands } },
			{ 5, OF_None , { OM_Eb, OM_CL, OM_NoOperands } },
			{ 6, OF_None , { OM_Illegal } },
			{ 7, OF_None , { OM_Eb, OM_CL, OM_NoOperands } }
		};

Table grp2EvCL[8]=	{
			{ 0, OF_None , { OM_Ev, OM_CL, OM_NoOperands } },
			{ 1, OF_None , { OM_Ev, OM_CL, OM_NoOperands } },
			{ 2, OF_None , { OM_Ev, OM_CL, OM_NoOperands } },
			{ 3, OF_None , { OM_Ev, OM_CL, OM_NoOperands } },
			{ 4, OF_None , { OM_Ev, OM_CL, OM_NoOperands } },
			{ 5, OF_None , { OM_Ev, OM_CL, OM_NoOperands } },
			{ 6, OF_None , { OM_Illegal } },
			{ 7, OF_None , { OM_Ev, OM_CL, OM_NoOperands } }
		};

const char* grp3Mnemonics[8]=	{
					"TEST",
					"",
					"NOT",
					"NEG",
					"MUL",
					"IMUL",
					"DIV",
					"IDIV"
				};

Table grp3_0[8]=	{
				{ 0, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
				{ 1, OF_None , { OM_Illegal } },
				{ 2, OF_None , { OM_NoOperands } },
				{ 3, OF_None , { OM_NoOperands } },
				{ 4, OF_None , { OM_Eb, OM_NoOperands } },
				{ 5, OF_None , { OM_Eb, OM_NoOperands } },
				{ 6, OF_None , { OM_Eb, OM_NoOperands } },
				{ 7, OF_None , { OM_Eb, OM_NoOperands } }
			};

Table grp3_1[8]=	{
				{ 0, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } },
				{ 1, OF_None , { OM_Illegal } },
				{ 2, OF_None , { OM_NoOperands } },
				{ 3, OF_None , { OM_NoOperands } },
				{ 4, OF_None , { OM_Ev, OM_NoOperands } },
				{ 5, OF_None , { OM_Ev, OM_NoOperands } },
				{ 6, OF_None , { OM_Ev, OM_NoOperands } },
				{ 7, OF_None , { OM_Ev, OM_NoOperands } }
			};

const char* grp4Mnemonics[8]=	{
					"INC",
					"DEC",
					"",
					"",
					"",
					"",
					"",
					""
				};

Table grp4[8]=	{
				{ 0, OF_None , { OM_Eb, OM_NoOperands } },
				{ 1, OF_None , { OM_Eb, OM_NoOperands } },
				{ 2, OF_None , { OM_Illegal } },
				{ 3, OF_None , { OM_Illegal } },
				{ 4, OF_None , { OM_Illegal } },
				{ 5, OF_None , { OM_Illegal } },
				{ 6, OF_None , { OM_Illegal } },
				{ 7, OF_None , { OM_Illegal } }
			};

const char* grp5Mnemonics[8]=	{
					"INC",
					"DEC",
					"CALL",
					"CALL",
					"JMP",
					"JMP",
					"PUSH",
					""
				};

Table grp5[8]=	{
				{ 0, OF_None , { OM_Ev, OM_NoOperands } },
				{ 1, OF_None , { OM_Ev, OM_NoOperands } },
				{ 2, OF_None , { OM_Ev, OM_NoOperands } },
				{ 3, OF_None , { OM_Ep, OM_NoOperands } },
				{ 4, OF_Branch , { OM_Ev, OM_NoOperands } },
				{ 5, OF_Branch , { OM_Mp, OM_NoOperands } },
				{ 6, OF_None , { OM_Ev, OM_NoOperands } },
				{ 7, OF_None , { OM_Illegal } }
			};



const char* grp7Mnemonics[8]=	{
					"SGDT",
					"SIDT",
					"LGDT",
					"LIDT",
					"SMSW",
					"",
					"",
					""
				};

Table grp7[8]=	{
				{ 0, OF_None , { OM_Illegal } },
				{ 1, OF_None , { OM_Illegal } },
				{ 2, OF_None , { OM_Mf, OM_NoOperands } },
				{ 3, OF_None , { OM_Mf, OM_NoOperands } },
				{ 4, OF_None , { OM_Ew, OM_NoOperands } },
				{ 5, OF_None , { OM_Illegal } },
				{ 6, OF_None , { OM_Illegal } },
				{ 7, OF_None , { OM_Illegal } }
			};

const char* grp11_0Mnemonics[8]=	{
						"MOV",
						"",
						"",
						"",
						"",
						"",
						"",
						"XABORT"
					};

Table grp11_1[8]=	{
			{ 0, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } },
			{ 1, OF_None , { OM_Illegal } },
			{ 2, OF_None , { OM_Illegal } },
			{ 3, OF_None , { OM_Illegal } },
			{ 4, OF_None , { OM_Illegal } },
			{ 5, OF_None , { OM_Illegal } },
			{ 6, OF_None , { OM_Illegal } },
			{ 7, OF_None , { OM_Ev, OM_Iz, OM_NoOperands } }
		};

const char* grp11_1Mnemonics[8]=	{
						"MOV",
						"",
						"",
						"",
						"",
						"",
						"",
						"XBEGIN"
					};

Table grp11_0[8]=	{
			{ 0, OF_None , { OM_Eb, OM_Ib, OM_NoOperands } },
			{ 1, OF_None , { OM_Illegal } },
			{ 2, OF_None , { OM_Illegal } },
			{ 3, OF_None , { OM_Illegal } },
			{ 4, OF_None , { OM_Illegal } },
			{ 5, OF_None , { OM_Illegal } },
			{ 6, OF_None , { OM_Illegal } },
			{ 7, OF_None , { OM_Jz, OM_NoOperands } }
		};

const char* grp12Mnemonics[8] = {
	"",
	"",
	"",
	"",
	"BT",
	"BTS",
	"BTR",
	"BTC"
};

Table grp12[8] = {
	{ 0, OF_None ,{ OM_Illegal } },
	{ 1, OF_None ,{ OM_Illegal } },
	{ 2, OF_None ,{ OM_Illegal } },
	{ 3, OF_None ,{ OM_Illegal } },
	{ 4, OF_None ,{ OM_Ev, OM_Ib, OM_NoOperands } },
	{ 5, OF_None ,{ OM_Ev, OM_Ib, OM_NoOperands } },
	{ 6, OF_None ,{ OM_Ev, OM_Ib, OM_NoOperands } },
	{ 7, OF_None ,{ OM_Ev, OM_Ib, OM_NoOperands } }
};

const Table _1byte[NUM_OPS]=	{
					{ 1, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x00
					{ 1, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x01
					{ 1, OF_None , { OM_MODRM, OM_Gb, OM_Eb, OM_NoOperands } },		//0x02
					{ 1, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_NoOperands } },		//0x03
					{ 1, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },			//0x04
					{ 1, OF_None , { OM_eAX, OM_Iz, OM_NoOperands } },	     		//0x05
					{ 2, OF_None , { OM_HasW, OM_ES, OM_NoOperands } },  		     	//0x06
					{ 3, OF_None , { OM_HasW, OM_ES, OM_NoOperands } },  		     	//0x07
					{ 4, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x08
					{ 4, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x09
					{ 4, OF_None , { OM_MODRM, OM_Gb, OM_Eb, OM_NoOperands } },		//0x0A
					{ 4, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_NoOperands } },		//0x0B
					{ 4, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },	     		//0x0C
					{ 4, OF_None , { OM_eAX, OM_Iz, OM_NoOperands } },	     		//0x0D
					{ 2, OF_None , { OM_HasW, OM_CS, OM_NoOperands } },  		     	//0x0E
					{25, OF_None , { OM_OpcodeExtensionByte, OM_NoOperands } },		//0x0F
					{ 5, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x10
					{ 5, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x11
					{ 5, OF_None , { OM_MODRM, OM_Gb, OM_Eb, OM_NoOperands } },		//0x12
					{ 5, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_NoOperands } },		//0x13
					{ 5, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },	     		//0x14
					{ 5, OF_None , { OM_eAX, OM_Iz, OM_NoOperands } },	     		//0x15
					{ 2, OF_None , { OM_HasW, OM_SS, OM_NoOperands } },  		    	//0x16
					{ 3, OF_None , { OM_HasW, OM_SS, OM_NoOperands } },  		     	//0x17
					{ 6, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x18
					{ 6, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x19
					{ 6, OF_None , { OM_MODRM, OM_Gb, OM_Eb, OM_NoOperands } },		//0x1A
					{ 6, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_NoOperands } },		//0x1B
					{ 6, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },	     		//0x1C
					{ 6, OF_None , { OM_eAX, OM_Iz, OM_NoOperands } },	     		//0x1D
					{ 2, OF_None , { OM_HasW, OM_DS, OM_NoOperands } },  	     		//0x1E
					{ 3, OF_None , { OM_HasW, OM_DS, OM_NoOperands } },		     	//0x1F
					{ 7, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x20
					{ 7, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x21
					{ 7, OF_None , { OM_MODRM, OM_Gb, OM_Eb, OM_NoOperands } },		//0x22
					{ 7, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_NoOperands } },		//0x23
					{ 7, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },	     		//0x24
					{ 7, OF_None , { OM_eAX, OM_Iz, OM_NoOperands } },	     		//0x25
					{ 0, OF_None , { OM_ES, OM_NoOperands } },  		     		//0x26	- PREFIX ES OVERRIDE
					{ 8, OF_None , { OM_NoOperands } },  		     			//0x27
					{ 9, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x28
					{ 9, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x29
					{ 9, OF_None , { OM_MODRM, OM_Gb, OM_Eb, OM_NoOperands } },		//0x2A
					{ 9, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_NoOperands } },		//0x2B
					{ 9, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },	     		//0x2C
					{ 9, OF_None , { OM_eAX, OM_Iz, OM_NoOperands } },	     		//0x2D
					{ 0, OF_None , { OM_CS, OM_NoOperands } },  		     		//0x2E	- PREFIX CS OVERRIDE
					{10, OF_None , { OM_NoOperands } },		     			//0x2F
					{11, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x30
					{11, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x31
					{11, OF_None , { OM_MODRM, OM_Gb, OM_Eb, OM_NoOperands } },		//0x32
					{11, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_NoOperands } },		//0x33
					{11, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },	     		//0x34
					{11, OF_None , { OM_eAX, OM_Iz, OM_NoOperands } },	     		//0x35
					{ 0, OF_None , { OM_SS, OM_NoOperands } },  		     		//0x36	- PREFIX SS OVERRIDE
					{12, OF_None , { OM_NoOperands } },  		     			//0x37
					{13, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x38
					{13, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x39
					{13, OF_None , { OM_MODRM, OM_Gb, OM_Eb, OM_NoOperands } },		//0x3A
					{13, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_NoOperands } },		//0x3B
					{13, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },	     		//0x3C
					{13, OF_None , { OM_eAX, OM_Iz, OM_NoOperands } },	     		//0x3D
					{ 0, OF_None , { OM_DS, OM_NoOperands } },  		     		//0x3E	- PREFIX DS OVERRIDE
					{14, OF_None , { OM_NoOperands } },		     			//0x3F
					{15, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x40
					{15, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x41
					{15, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x42
					{15, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x43
					{15, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x44
					{15, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x45
					{15, OF_None , { OM_GPR, OM_NoOperands } }, 		     		//0x46
					{15, OF_None , { OM_GPR, OM_NoOperands } }, 		     		//0x47
					{16, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x48
					{16, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x49
					{16, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x4A
					{16, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x4B
					{16, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x4C
					{16, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x4D
					{16, OF_None , { OM_GPR, OM_NoOperands } }, 		     		//0x4E
					{16, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x4F
					{ 2, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x50
					{ 2, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x51
					{ 2, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x52
					{ 2, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x53
					{ 2, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x54
					{ 2, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x55
					{ 2, OF_None , { OM_GPR, OM_NoOperands } }, 		     		//0x56
					{ 2, OF_None , { OM_GPR, OM_NoOperands } }, 		     		//0x57
					{ 3, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x58
					{ 3, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x59
					{ 3, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x5A
					{ 3, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x5B
					{ 3, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x5C
					{ 3, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x5D
					{ 3, OF_None , { OM_GPR, OM_NoOperands } }, 		     		//0x5E
					{ 3, OF_None , { OM_GPR, OM_NoOperands } },		     		//0x5F
					{17, OF_None , { OM_JustW, OM_NoOperands } },			     	//0x60
					{18, OF_None , { OM_JustW, OM_NoOperands } },			     	//0x61
					{19, OF_None , { OM_MODRM, OM_Gv, OM_Ma, OM_NoOperands } },		//0x62
					{20, OF_None , { OM_MODRM, OM_Ew, OM_Gw, OM_NoOperands } },		//0x63
					{ 0, OF_None , { OM_FS, OM_NoOperands } },		     		//0x64 - FS PREFIX
					{ 0, OF_None , { OM_GS, OM_NoOperands } },		     		//0x65 - GS PREFIX
					{ 0, OF_None , { OM_OSize, OM_NoOperands } }, 	     			//0x66 - Operand Size PREFIX
					{ 0, OF_None , { OM_MSize, OM_NoOperands } }, 	     			//0x67 - Memory Size PREFIX
					{ 2, OF_None , { OM_HasW, OM_Iz, OM_NoOperands } },		  	//0x68
					{21, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_Iz, OM_NoOperands } },	//0x69
					{ 2, OF_None , { OM_HasW, OM_Ib, OM_NoOperands } },			//0x6A
					{21, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_Ib, OM_NoOperands } },	//0x6B
					{22, OF_None , { OM_Yb, OM_DX, OM_NoOperands } },			//0x6C
					{22, OF_None , { OM_Yz, OM_DX, OM_NoOperands } },			//0x6D
					{23, OF_None , { OM_DX, OM_Xb, OM_NoOperands } },			//0x6E
					{23, OF_None , { OM_DX, OM_Xz, OM_NoOperands } },			//0x6F
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x70
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x71
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x72
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x73
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x74
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x75
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x76
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x77
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x78
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x79
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x7A
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x7B
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x7C
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x7D
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x7E
					{24, OF_CondBranch , { OM_cc, OM_Jb, OM_NoOperands } },			//0x7F
					{25, OF_None , { OM_MODRM, OM_Grp1_0, OM_NoOperands } },		//0x80
					{25, OF_None , { OM_MODRM, OM_Grp1_1, OM_NoOperands } },		//0x81
					{25, OF_None , { OM_MODRM, OM_Grp1_0, OM_NoOperands } },		//0x82
					{25, OF_None , { OM_MODRM, OM_Grp1_2, OM_NoOperands } },		//0x83
					{26, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x84
					{26, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x85
					{27, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x86
					{27, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x87
					{28, OF_None , { OM_MODRM, OM_Eb, OM_Gb, OM_NoOperands } },		//0x88
					{28, OF_None , { OM_MODRM, OM_Ev, OM_Gv, OM_NoOperands } },		//0x89
					{28, OF_None , { OM_MODRM, OM_Gb, OM_Eb, OM_NoOperands } },		//0x8A
					{28, OF_None , { OM_MODRM, OM_Gv, OM_Ev, OM_NoOperands } },		//0x8B
					{28, OF_None , { OM_MODRM, OM_Ew, OM_Sw, OM_NoOperands } },		//0x8C
					{29, OF_None , { OM_MODRM, OM_Gv, OM_M, OM_NoOperands } },		//0x8D
					{28, OF_None , { OM_MODRM, OM_Sw, OM_Ew, OM_NoOperands } },		//0x8E
					{25, OF_None , { OM_MODRM, OM_Grp1A, OM_NoOperands } },			//0x8F
					{27, OF_None , { OM_GPR, OM_eAX, OM_NoOperands } },			//0x90
					{27, OF_None , { OM_GPR, OM_eAX, OM_NoOperands } },			//0x91
					{27, OF_None , { OM_GPR, OM_eAX, OM_NoOperands } },			//0x92
					{27, OF_None , { OM_GPR, OM_eAX, OM_NoOperands } },			//0x93
					{27, OF_None , { OM_GPR, OM_eAX, OM_NoOperands } },			//0x94
					{27, OF_None , { OM_GPR, OM_eAX, OM_NoOperands } },			//0x95
					{27, OF_None , { OM_GPR, OM_eAX, OM_NoOperands } },			//0x96
					{27, OF_None , { OM_GPR, OM_eAX, OM_NoOperands } },			//0x97
					{30, OF_None , { OM_WDEBW, OM_NoOperands } },				//0x98
					{30, OF_None , { OM_DQWD, OM_NoOperands } },				//0x99
					{31, OF_None , { OM_FAR, OM_NoOperands } },				//0x9A
					{32, OF_None , { OM_NoOperands } },					//0x9B
					{33, OF_None , { OM_NoOperands } },					//0x9C
					{34, OF_None , { OM_NoOperands } },					//0x9D
					{35, OF_None , { OM_NoOperands } },					//0x9E
					{36, OF_None , { OM_NoOperands } },					//0x9F
					{28, OF_None , { OM_AL, OM_Ob, OM_NoOperands } },			//0xA0
					{28, OF_None , { OM_eAX, OM_Ov, OM_NoOperands } },			//0xA1
					{28, OF_None , { OM_Ob, OM_AL, OM_NoOperands } },			//0xA2
					{28, OF_None , { OM_Ov, OM_eAX, OM_NoOperands } },			//0xA3
					{37, OF_None , { OM_Yb, OM_Xb, OM_NoOperands } },			//0xA4
					{37, OF_None , { OM_Yv, OM_Xv, OM_NoOperands } },			//0xA5
					{38, OF_None , { OM_Xb, OM_Yb, OM_NoOperands } },			//0xA6
					{38, OF_None , { OM_Xv, OM_Yv, OM_NoOperands } },			//0xA7
					{26, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },			//0xA8
					{26, OF_None , { OM_eAX, OM_Iz, OM_NoOperands } },			//0xA9
					{39, OF_None , { OM_Yb, OM_AL, OM_NoOperands } },			//0xAA
					{39, OF_None , { OM_Yv, OM_eAX, OM_NoOperands } },			//0xAB
					{40, OF_None , { OM_AL, OM_Xb, OM_NoOperands } },			//0xAC
					{40, OF_None , { OM_eAX, OM_Xv, OM_NoOperands } },			//0xAD
					{41, OF_None , { OM_AL, OM_Yb, OM_NoOperands } },			//0xAE
					{41, OF_None , { OM_eAX, OM_Yv, OM_NoOperands } },			//0xAF
					{28, OF_None , { OM_GPRb, OM_Ib, OM_NoOperands } },			//0xB0
					{28, OF_None , { OM_GPRb, OM_Ib, OM_NoOperands } },			//0xB1
					{28, OF_None , { OM_GPRb, OM_Ib, OM_NoOperands } },			//0xB2
					{28, OF_None , { OM_GPRb, OM_Ib, OM_NoOperands } },			//0xB3
					{28, OF_None , { OM_GPRb, OM_Ib, OM_NoOperands } },			//0xB4
					{28, OF_None , { OM_GPRb, OM_Ib, OM_NoOperands } },			//0xB5
					{28, OF_None , { OM_GPRb, OM_Ib, OM_NoOperands } },			//0xB6
					{28, OF_None , { OM_GPRb, OM_Ib, OM_NoOperands } },			//0xB7
					{28, OF_None , { OM_GPR, OM_Iv, OM_NoOperands } },			//0xB8
					{28, OF_None , { OM_GPR, OM_Iv, OM_NoOperands } },			//0xB9
					{28, OF_None , { OM_GPR, OM_Iv, OM_NoOperands } },			//0xBA
					{28, OF_None , { OM_GPR, OM_Iv, OM_NoOperands } },			//0xBB
					{28, OF_None , { OM_GPR, OM_Iv, OM_NoOperands } },			//0xBC
					{28, OF_None , { OM_GPR, OM_Iv, OM_NoOperands } },			//0xBD
					{28, OF_None , { OM_GPR, OM_Iv, OM_NoOperands } },			//0xBE
					{28, OF_None , { OM_GPR, OM_Iv, OM_NoOperands } },			//0xBF
					{25, OF_None , { OM_MODRM, OM_Grp2_0, OM_NoOperands } },		//0xC0
					{25, OF_None , { OM_MODRM, OM_Grp2_1, OM_NoOperands } },		//0xC1
					{42, OF_Terminator , { OM_JustW, OM_SPACE, OM_Iw, OM_NoOperands } },	//0xC2
					{42, OF_Terminator , { OM_JustW, OM_NoOperands, OM_NoOperands } },	//0xC3
					{43, OF_None , { OM_MODRM, OM_Gz, OM_Mp, OM_NoOperands } },		//0xC4
					{44, OF_None , { OM_MODRM, OM_Gz, OM_Mp, OM_NoOperands } },		//0xC5
					{25, OF_None , { OM_MODRM, OM_Grp11_0, OM_NoOperands } },		//0xC6
					{25, OF_None , { OM_MODRM, OM_Grp11_1, OM_NoOperands } },		//0xC7
					{45, OF_None , { OM_Iw, OM_Ib, OM_NoOperands } },			//0xC8
					{46, OF_None , { OM_JustW, OM_NoOperands, OM_NoOperands } },		//0xC9
					{47, OF_Terminator , { OM_JustW, OM_SPACE, OM_Iw, OM_NoOperands } },	//0xCA
					{47, OF_Terminator , { OM_JustW, OM_NoOperands } },			//0xCB
					{48, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xCC
					{49, OF_None , { OM_SPACE, OM_Ib, OM_NoOperands } },			//0xCD
					{50, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xCE
					{51, OF_Terminator , { OM_JustW, OM_NoOperands, OM_NoOperands } },	//0xCF
					{25, OF_None , { OM_MODRM, OM_Grp2_2, OM_NoOperands } },		//0xD0
					{25, OF_None , { OM_MODRM, OM_Grp2_3, OM_NoOperands } },		//0xD1
					{25, OF_None , { OM_MODRM, OM_Grp2_4, OM_NoOperands } },		//0xD2
					{25, OF_None , { OM_MODRM, OM_Grp2_5, OM_NoOperands } },		//0xD3
					{52, OF_None , { OM_Ib, OM_NoOperands } },				//0xD4
					{53, OF_None , { OM_Ib, OM_NoOperands } },				//0xD5
					{25, OF_None , { OM_Illegal, OM_NoOperands } },				//0xD6
					{54, OF_None , { OM_XLAT, OM_NoOperands } },				//0xD7
					{ 0, OF_None , { OM_FPUEscape, OM_NoOperands } },			//0xD8
					{ 0, OF_None , { OM_FPUEscape, OM_NoOperands } },			//0xD9
					{ 0, OF_None , { OM_FPUEscape, OM_NoOperands } },			//0xDA
					{ 0, OF_None , { OM_FPUEscape, OM_NoOperands } },			//0xDB
					{ 0, OF_None , { OM_FPUEscape, OM_NoOperands } },			//0xDC
					{ 0, OF_None , { OM_FPUEscape, OM_NoOperands } },			//0xDD
					{ 0, OF_None , { OM_FPUEscape, OM_NoOperands } },			//0xDE
					{ 0, OF_None , { OM_FPUEscape, OM_NoOperands } },			//0xDF
					{55, OF_CondBranch , { OM_Jb, OM_NoOperands } },			//0xE0
					{56, OF_CondBranch , { OM_Jb, OM_NoOperands } },			//0xE1
					{57, OF_CondBranch , { OM_Jb, OM_NoOperands } },			//0xE2
					{58, OF_CondBranch , { OM_Jb, OM_NoOperands } },			//0xE3
					{59, OF_None , { OM_AL, OM_Ib, OM_NoOperands } },			//0xE4
					{59, OF_None , { OM_eAX, OM_Ib, OM_NoOperands } },			//0xE5
					{60, OF_None , { OM_Ib, OM_AL, OM_NoOperands } },			//0xE6
					{60, OF_None , { OM_Ib, OM_eAX, OM_NoOperands } },			//0xE7
					{31, OF_None , { OM_HasW, OM_Jz, OM_NoOperands } },			//0xE8
					{61, OF_Branch , { OM_HasW, OM_Jz, OM_NoOperands } },			//0xE9
					{61, OF_Branch , { OM_FAR, OM_NoOperands } },				//0xEA
					{61, OF_Branch , { OM_Jb, OM_NoOperands } },				//0xEB
					{59, OF_None , { OM_AL, OM_DX, OM_NoOperands } },			//0xEC
					{59, OF_None , { OM_eAX, OM_DX, OM_NoOperands } },			//0xED
					{60, OF_None , { OM_DX, OM_AL, OM_NoOperands } },			//0xEE
					{60, OF_None , { OM_DX, OM_eAX, OM_NoOperands } },			//0xEF
					{ 0, OF_None , { OM_LOCK, OM_NoOperands } },				//0xF0	- LOCK PREFIX
					{ 0, OF_None , { OM_Illegal, OM_NoOperands } },				//0xF1
					{ 0, OF_None , { OM_REPNE, OM_NoOperands } },				//0xF2	- REPNE PREFIX
					{ 0, OF_None , { OM_REP, OM_NoOperands } },				//0xF3	- REP PREFIX
					{62, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xF4
					{63, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xF5
					{25, OF_None , { OM_MODRM, OM_Grp3_0, OM_NoOperands } },		//0xF6
					{25, OF_None , { OM_MODRM, OM_Grp3_1, OM_NoOperands } },		//0xF7
					{64, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xF8
					{65, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xF9
					{66, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xFA
					{67, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xFB
					{68, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xFC
					{69, OF_None , { OM_NoOperands, OM_NoOperands } },			//0xFD
					{25, OF_None , { OM_MODRM, OM_Grp4, OM_NoOperands } },			//0xFE
					{25, OF_None , { OM_MODRM, OM_Grp5, OM_NoOperands } },			//0xFF
				};

const Table _2byte[NUM_OPS]=	{
					{25, OF_None, { OM_Illegal } },								//0x0F00
					{25, OF_None, { OM_MODRM, OM_Grp7, OM_NoOperands } },					//0x0F01
					{25, OF_None, { OM_Illegal } },								//0x0F02
					{25, OF_None, { OM_Illegal } },								//0x0F03
					{25, OF_None, { OM_Illegal } },								//0x0F04
					{25, OF_None, { OM_Illegal } },								//0x0F05
					{25, OF_None, { OM_Illegal } },								//0x0F06
					{25, OF_None, { OM_Illegal } },								//0x0F07
					{25, OF_None, { OM_Illegal } },								//0x0F08
					{79, OF_None, { OM_NoOperands } },								//0x0F09
					{25, OF_None, { OM_Illegal } },								//0x0F0A
					{25, OF_None, { OM_Illegal } },								//0x0F0B
					{25, OF_None, { OM_Illegal } },								//0x0F0C
					{25, OF_None, { OM_Illegal } },								//0x0F0D
					{25, OF_None, { OM_Illegal } },								//0x0F0E
					{25, OF_None, { OM_Illegal } },								//0x0F10
					{25, OF_None, { OM_Illegal } },								//0x0F10
					{25, OF_None, { OM_Illegal } },								//0x0F11
					{25, OF_None, { OM_Illegal } },								//0x0F12
					{25, OF_None, { OM_Illegal } },								//0x0F13
					{25, OF_None, { OM_Illegal } },							  	//0x0F14
					{25, OF_None, { OM_Illegal } },							  	//0x0F15
					{25, OF_None, { OM_Illegal } },							  	//0x0F16
					{25, OF_None, { OM_Illegal } },							  	//0x0F17
					{25, OF_None, { OM_Illegal } },								//0x0F18
					{25, OF_None, { OM_Illegal } },								//0x0F19
					{25, OF_None, { OM_Illegal } },								//0x0F1A
					{25, OF_None, { OM_Illegal } },								//0x0F1B
					{25, OF_None, { OM_Illegal } },							  	//0x0F1C
					{25, OF_None, { OM_Illegal } },							  	//0x0F1D
					{25, OF_None, { OM_Illegal } },							  	//0x0F1E
					{25, OF_None, { OM_Illegal } },							  	//0x0F1F
					{28, OF_None, { OM_MODRM, OM_Rd, OM_Cd, OM_NoOperands } },				//0x0F20
					{28, OF_None, { OM_MODRM, OM_Rd, OM_Dd, OM_NoOperands } },				//0x0F21
					{28, OF_None, { OM_MODRM, OM_Cd, OM_Rd, OM_NoOperands } },				//0x0F22
					{28, OF_None, { OM_MODRM, OM_Dd, OM_Rd, OM_NoOperands } },				//0x0F23
					{25, OF_None, { OM_Illegal } },							  	//0x0F24
					{25, OF_None, { OM_Illegal } },							  	//0x0F25
					{25, OF_None, { OM_Illegal } },							  	//0x0F26
					{25, OF_None, { OM_Illegal } },							  	//0x0F27
					{72, OF_None, { OM_Precision66, OM_MODRM, OM_Vps, OM_Wps, OM_NoOperands } },		//0x0F28
					{72, OF_None, { OM_Precision66, OM_MODRM, OM_Wps, OM_Vps, OM_NoOperands } },		//0x0F29
					{25, OF_None, { OM_Illegal } },								//0x0F2A
					{25, OF_None, { OM_Illegal } },								//0x0F2B
					{25, OF_None, { OM_Illegal } },							  	//0x0F2C
					{25, OF_None, { OM_Illegal } },							  	//0x0F2D
					{25, OF_None, { OM_Illegal } },							  	//0x0F2E
					{25, OF_None, { OM_Illegal } },							  	//0x0F2F
					{25, OF_None, { OM_Illegal } },								//0x0F30
					{25, OF_None, { OM_Illegal } },								//0x0F31
					{25, OF_None, { OM_Illegal } },								//0x0F32
					{25, OF_None, { OM_Illegal } },								//0x0F33
					{25, OF_None, { OM_Illegal } },							  	//0x0F34
					{25, OF_None, { OM_Illegal } },							  	//0x0F35
					{25, OF_None, { OM_Illegal } },							  	//0x0F36
					{25, OF_None, { OM_Illegal } },							  	//0x0F37
					{25, OF_None, { OM_Illegal } },								//0x0F38
					{25, OF_None, { OM_Illegal } },								//0x0F39
					{25, OF_None, { OM_Illegal } },								//0x0F3A
					{25, OF_None, { OM_Illegal } },								//0x0F3B
					{25, OF_None, { OM_Illegal } },							  	//0x0F3C
					{25, OF_None, { OM_Illegal } },							  	//0x0F3D
					{25, OF_None, { OM_Illegal } },								//0x0F3E
					{25, OF_None, { OM_Illegal } },								//0x0F3F
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F40
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F41
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F42
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F43
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F44
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F45
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F46
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F47
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F48
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F49
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F4A
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F4B
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F4C
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F4D
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F4E
					{71, OF_None, { OM_MODRM, OM_cc, OM_Gv, OM_Ev, OM_NoOperands } },			//0x0F4F
					{74, OF_None, { OM_Precision66, OM_MODRM, OM_Gy, OM_Ups, OM_NoOperands } },		//0x0F50
					{25, OF_None, { OM_Illegal } },								//0x0F51
					{25, OF_None, { OM_Illegal } },								//0x0F52
					{25, OF_None, { OM_Illegal } },								//0x0F53
					{25, OF_None, { OM_Illegal } },								//0x0F54
					{25, OF_None, { OM_Illegal } },								//0x0F55
					{25, OF_None, { OM_Illegal } },								//0x0F56
					{25, OF_None, { OM_Illegal } },								//0x0F57
					{78, OF_None, { OM_Precision66F3F2, OM_MODRM, OM_Vps, OM_Wps, OM_NoOperands } },	//0x0F58
					{77, OF_None, { OM_Precision66F3F2, OM_MODRM, OM_Vps, OM_Wps, OM_NoOperands } },	//0x0F59
					{25, OF_None, { OM_Illegal } },								//0x0F5A
					{25, OF_None, { OM_Illegal } },								//0x0F5B
					{75, OF_None, { OM_Precision66F3F2, OM_MODRM, OM_Vps, OM_Wps, OM_NoOperands } },	//0x0F5C
					{25, OF_None, { OM_Illegal } },								//0x0F5D
					{76, OF_None, { OM_Precision66F3F2, OM_MODRM, OM_Vps, OM_Wps, OM_NoOperands } },	//0x0F5E
					{25, OF_None, { OM_Illegal } },								//0x0F5F
					{25, OF_None, { OM_Illegal } },								//0x0F60
					{25, OF_None, { OM_Illegal } },								//0x0F61
					{25, OF_None, { OM_Illegal } },								//0x0F62
					{25, OF_None, { OM_Illegal } },								//0x0F63
					{25, OF_None, { OM_Illegal } },								//0x0F64
					{25, OF_None, { OM_Illegal } },								//0x0F65
					{25, OF_None, { OM_Illegal } },								//0x0F66
					{25, OF_None, { OM_Illegal } },								//0x0F67
					{25, OF_None, { OM_Illegal } },								//0x0F68
					{25, OF_None, { OM_Illegal } },								//0x0F69
					{25, OF_None, { OM_Illegal } },								//0x0F6A
					{25, OF_None, { OM_Illegal } },								//0x0F6B
					{25, OF_None, { OM_Illegal } },								//0x0F6C
					{25, OF_None, { OM_Illegal } },								//0x0F6D
					{25, OF_None, { OM_Illegal } },								//0x0F6E
					{25, OF_None, { OM_Illegal } },								//0x0F6F
					{25, OF_None, { OM_Illegal } },								//0x0F70
					{25, OF_None, { OM_Illegal } },								//0x0F71
					{25, OF_None, { OM_Illegal } },								//0x0F72
					{25, OF_None, { OM_Illegal } },								//0x0F73
					{25, OF_None, { OM_Illegal } },								//0x0F74
					{25, OF_None, { OM_Illegal } },								//0x0F75
					{25, OF_None, { OM_Illegal } },								//0x0F76
					{25, OF_None, { OM_Illegal } },								//0x0F77
					{25, OF_None, { OM_Illegal } },								//0x0F78
					{25, OF_None, { OM_Illegal } },								//0x0F79
					{25, OF_None, { OM_Illegal } },								//0x0F7A
					{25, OF_None, { OM_Illegal } },								//0x0F7B
					{25, OF_None, { OM_Illegal } },								//0x0F7C
					{25, OF_None, { OM_Illegal } },								//0x0F7D
					{25, OF_None, { OM_Illegal } },								//0x0F7E
					{25, OF_None, { OM_Illegal } },								//0x0F7F
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F80
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F81
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F82
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F83
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F84
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F85
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F86
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F87
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F88
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F89
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F8A
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F8B
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F8C
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F8D
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F8E
					{24, OF_CondBranch , { OM_cc, OM_Jz, OM_NoOperands } },					//0x0F8F
					{25, OF_None, { OM_Illegal } },								//0x0F90
					{25, OF_None, { OM_Illegal } },								//0x0F91
					{25, OF_None, { OM_Illegal } },								//0x0F92
					{25, OF_None, { OM_Illegal } },								//0x0F93
					{25, OF_None, { OM_Illegal } },								//0x0F94
					{25, OF_None, { OM_Illegal } },								//0x0F95
					{25, OF_None, { OM_Illegal } },								//0x0F96
					{25, OF_None, { OM_Illegal } },								//0x0F97
					{25, OF_None, { OM_Illegal } },								//0x0F98
					{25, OF_None, { OM_Illegal } },								//0x0F99
					{25, OF_None, { OM_Illegal } },								//0x0F9A
					{25, OF_None, { OM_Illegal } },								//0x0F9B
					{25, OF_None, { OM_Illegal } },								//0x0F9C
					{25, OF_None, { OM_Illegal } },								//0x0F9D
					{25, OF_None, { OM_Illegal } },								//0x0F9E
					{25, OF_None, { OM_Illegal } },								//0x0F9F
					{25, OF_None, { OM_Illegal } },								//0x0FA0
					{25, OF_None, { OM_Illegal } },								//0x0FA1
					{25, OF_None, { OM_Illegal } },								//0x0FA2
					{25, OF_None, { OM_Illegal } },								//0x0FA3
					{25, OF_None, { OM_Illegal } },								//0x0FA4
					{25, OF_None, { OM_Illegal } },								//0x0FA5
					{25, OF_None, { OM_Illegal } },								//0x0FA6
					{25, OF_None, { OM_Illegal } },								//0x0FA7
					{25, OF_None, { OM_Illegal } },								//0x0FA8
					{25, OF_None, { OM_Illegal } },								//0x0FA9
					{25, OF_None, { OM_Illegal } },								//0x0FAA
					{25, OF_None, { OM_Illegal } },								//0x0FAB
					{25, OF_None, { OM_Illegal } },								//0x0FAC
					{25, OF_None, { OM_Illegal } },								//0x0FAD
					{25, OF_None, { OM_Illegal } },								//0x0FAE
					{25, OF_None, { OM_Illegal } },								//0x0FAF
					{25, OF_None, { OM_Illegal } },								//0x0FB0
					{25, OF_None, { OM_Illegal } },								//0x0FB1
					{25, OF_None, { OM_Illegal } },								//0x0FB2
					{25, OF_None, { OM_Illegal } },								//0x0FB3
					{25, OF_None, { OM_Illegal } },								//0x0FB4
					{25, OF_None, { OM_Illegal } },								//0x0FB5
					{70, OF_None, { OM_MODRM, OM_Gv, OM_Eb, OM_NoOperands } },				//0x0FB6
					{70, OF_None, { OM_MODRM, OM_Gv, OM_Ew, OM_NoOperands } },				//0x0FB7
					{25, OF_None, { OM_Illegal } },								//0x0FB8
					{25, OF_None, { OM_Illegal } },								//0x0FB9
					{25, OF_None ,{ OM_MODRM, OM_Grp12, OM_NoOperands } },		//0x0FBA
					{25, OF_None, { OM_Illegal } },								//0x0FBB
					{25, OF_None, { OM_Illegal } },								//0x0FBC
					{25, OF_None, { OM_Illegal } },								//0x0FBD
					{25, OF_None, { OM_Illegal } },								//0x0FBE
					{25, OF_None, { OM_Illegal } },								//0x0FBF
					{25, OF_None, { OM_Illegal } },								//0x0FC0
					{25, OF_None, { OM_Illegal } },								//0x0FC1
					{25, OF_None, { OM_Illegal } },								//0x0FC2
					{25, OF_None, { OM_Illegal } },								//0x0FC3
					{25, OF_None, { OM_Illegal } },								//0x0FC4
					{25, OF_None, { OM_Illegal } },								//0x0FC5
					{73, OF_None, { OM_Precision66, OM_MODRM, OM_Vps, OM_Wps, OM_Ib, OM_NoOperands } },	//0x0FC6		-- 64bit requires OM_Hps
					{25, OF_None, { OM_Illegal } },								//0x0FC7
					{25, OF_None, { OM_Illegal } },								//0x0FC8
					{25, OF_None, { OM_Illegal } },								//0x0FC9
					{25, OF_None, { OM_Illegal } },								//0x0FCA
					{25, OF_None, { OM_Illegal } },								//0x0FCB
					{25, OF_None, { OM_Illegal } },								//0x0FCC
					{25, OF_None, { OM_Illegal } },								//0x0FCD
					{25, OF_None, { OM_Illegal } },								//0x0FCE
					{25, OF_None, { OM_Illegal } },								//0x0FCF
					{25, OF_None, { OM_Illegal } },								//0x0FD0
					{25, OF_None, { OM_Illegal } },								//0x0FD1
					{25, OF_None, { OM_Illegal } },								//0x0FD2
					{25, OF_None, { OM_Illegal } },								//0x0FD3
					{25, OF_None, { OM_Illegal } },								//0x0FD4
					{25, OF_None, { OM_Illegal } },								//0x0FD5
					{25, OF_None, { OM_Illegal } },								//0x0FD6
					{25, OF_None, { OM_Illegal } },								//0x0FD7
					{25, OF_None, { OM_Illegal } },								//0x0FD8
					{25, OF_None, { OM_Illegal } },								//0x0FD9
					{25, OF_None, { OM_Illegal } },								//0x0FDA
					{25, OF_None, { OM_Illegal } },								//0x0FDB
					{25, OF_None, { OM_Illegal } },								//0x0FDC
					{25, OF_None, { OM_Illegal } },								//0x0FDD
					{25, OF_None, { OM_Illegal } },								//0x0FDE
					{25, OF_None, { OM_Illegal } },								//0x0FDF
					{25, OF_None, { OM_Illegal } },								//0x0FE0
					{25, OF_None, { OM_Illegal } },								//0x0FE1
					{25, OF_None, { OM_Illegal } },								//0x0FE2
					{25, OF_None, { OM_Illegal } },								//0x0FE3
					{25, OF_None, { OM_Illegal } },								//0x0FE4
					{25, OF_None, { OM_Illegal } },								//0x0FE5
					{25, OF_None, { OM_Illegal } },								//0x0FE6
					{25, OF_None, { OM_Illegal } },								//0x0FE7
					{25, OF_None, { OM_Illegal } },								//0x0FE8
					{25, OF_None, { OM_Illegal } },								//0x0FE9
					{25, OF_None, { OM_Illegal } },								//0x0FEA
					{25, OF_None, { OM_Illegal } },								//0x0FEB
					{25, OF_None, { OM_Illegal } },								//0x0FEC
					{25, OF_None, { OM_Illegal } },								//0x0FED
					{25, OF_None, { OM_Illegal } },								//0x0FEE
					{25, OF_None, { OM_Illegal } },								//0x0FEF
					{25, OF_None, { OM_Illegal } },								//0x0FF0
					{25, OF_None, { OM_Illegal } },								//0x0FF1
					{25, OF_None, { OM_Illegal } },								//0x0FF2
					{25, OF_None, { OM_Illegal } },								//0x0FF3
					{25, OF_None, { OM_Illegal } },								//0x0FF4
					{25, OF_None, { OM_Illegal } },								//0x0FF5
					{25, OF_None, { OM_Illegal } },								//0x0FF6
					{25, OF_None, { OM_Illegal } },								//0x0FF7
					{25, OF_None, { OM_Illegal } },								//0x0FF8
					{25, OF_None, { OM_Illegal } },								//0x0FF9
					{25, OF_None, { OM_Illegal } },								//0x0FFA
					{25, OF_None, { OM_Illegal } },								//0x0FFB
					{25, OF_None, { OM_Illegal } },								//0x0FFC
					{25, OF_None, { OM_Illegal } },								//0x0FFD
					{25, OF_None, { OM_Illegal } },								//0x0FFE
					{25, OF_None, { OM_Illegal } },								//0x0FFF
				};
char outputDis[32768];

void AddToOutput(const char* text)
{
	// Find insertion point
	char* insertAt=outputDis;
	while (*insertAt!=0)
	{
		insertAt++;
	}

	while(*text!=0)
	{
		*insertAt++=*text++;
	}
	*insertAt=0;
}

void AddToOutputChar(const char c)
{
	char* insertAt=outputDis;
	while (*insertAt!=0)
	{
		insertAt++;
	}

    *insertAt++ = c;
	*insertAt=0;
}

void Remove1FromOutput()
{
	// Find end point
	char* insertAt=outputDis;
	while (*insertAt!=0)
	{
		insertAt++;
	}
	insertAt--;
	*insertAt=0;
}

const char cc[16][3]= {
					"O",
					"NO",
					"B",
					"AE",
					"E",
					"NE",
					"BE",
					"A",
					"S",
					"NS",
					"P",
					"NP",
					"L",
					"GE",
					"LE",
					"G"
				};

const char reg8[8][3]=	{
					"AL",
					"CL",
					"DL",
					"BL",
					"AH",
					"CH",
					"DH",
					"BH"
				};

const char reg16[8][3]=	{
						"AX",
						"CX",
						"DX",
						"BX",
						"SP",
						"BP",
						"SI",
						"DI"
					};

const char reg32[8][4]=	{
						"EAX",
						"ECX",
						"EDX",
						"EBX",
						"ESP",
						"EBP",
						"ESI",
						"EDI"
					};

const char regCr[8][4]=	{
						"CR0",
						"CR1",
						"CR2",
						"CR3",
						"CR4",
						"ERR",
						"ERR",
						"ERR"
					};

const char regDr[8][4]=	{
						"DR0",
						"DR1",
						"DR2",
						"DR3",
						"DR4",
						"DR5",
						"DR6",
						"DR7"
					};

const char segReg[8][3]={
				"ES",
				"CS",
				"SS",
				"DS",
				"FS",
				"GS",
				"?",
				"?"
			  };

const char regMMX[8][5]={
				"XMM0",
				"XMM1",
				"XMM2",
				"XMM3",
				"XMM4",
				"XMM5",
				"XMM6",
				"XMM7"
			  };

const char hex[16][2]={"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};

void AppendHexValue32(unsigned int value)
{
	int a;

	for (a=0;a<8;a++)
	{
		int val=(value&0xF0000000)>>28;
		AddToOutput(hex[val]);
		value<<=4;
	}
}

void AppendHexValue16(unsigned short value)
{
	int a;

	for (a=0;a<4;a++)
	{
		int val=(value&0xF000)>>12;
		AddToOutput(hex[val]);
		value<<=4;
	}
}

void AppendHexValue8(unsigned char value)
{
	int a;

	for (a=0;a<2;a++)
	{
		int val=(value&0xF0)>>4;
		AddToOutput(hex[val]);
		value<<=4;
	}
}

void Extract8BitDisp(InStream* stream)
{
	unsigned int immediate=(char)GetNextByteFromStream(stream);

	if (stream->useAddress)
	{
		immediate+=stream->curAddress;
	}
	
	AppendHexValue32(immediate);
}

unsigned int Get32BitImm(InStream* stream)
{
	unsigned int disp=0;
	int a;
	for (a=0;a<4;a++)
	{
		disp>>=8;
		disp|=GetNextByteFromStream(stream)<<24;
	}
	return disp;
}

unsigned short Get16BitImm(InStream* stream)
{
	unsigned short disp=0;
	int a;
	for (a=0;a<2;a++)
	{
		disp>>=8;
		disp|=GetNextByteFromStream(stream)<<8;
	}
	return disp;
}

void Extract16BitDisp(InStream* stream)
{
	unsigned int immediate=Get16BitImm(stream);
		
	if (stream->useAddress)
	{
		immediate+=stream->curAddress;
	}
	AppendHexValue32(immediate);
}

void Extract32BitDisp(InStream* stream)
{
	unsigned int immediate=Get32BitImm(stream);
		
	if (stream->useAddress)
	{
		immediate+=stream->curAddress;
	}
	AppendHexValue32(immediate);
}

void Extract8BitImm(InStream* stream)
{
	AppendHexValue8(GetNextByteFromStream(stream));
}

void Extract16BitImm(InStream* stream)
{
	AppendHexValue16(Get16BitImm(stream));
}

void Extract32BitImm(InStream* stream)
{
	AppendHexValue32(Get32BitImm(stream));
}
				
void ExtractImm(int rSize,InStream* stream)
{
	AddToOutput("0x");
	switch (rSize)
	{
		case 0:
			Extract8BitImm(stream);
			break;
		case 1:
			Extract16BitImm(stream);
			break;
		case 2:
			Extract32BitImm(stream);
			break;
		default:
			AddToOutput("ExtractImm - Error rSize");
			break;
	}
}

void ExtractDisp(int rSize,InStream* stream)
{
	AddToOutput("0x");
	switch (rSize)
	{
		case 0:
			Extract8BitDisp(stream);
			break;
		case 1:
			Extract16BitDisp(stream);
			break;
		case 2:
			Extract32BitDisp(stream);
			break;
		default:
			AddToOutput("ExtractDisp - Error rSize");
			break;
	}
}

void ExtractOffset(int seg,int mSize,InStream* stream)
{
	if (seg==-1)
	{
		AddToOutput(segReg[3]);
	}
	else
	{
		AddToOutput(segReg[seg]);
	}
	switch (mSize)
	{
		case 1:
			AddToOutput(":0x");
			Extract16BitImm(stream);
			break;
		case 2:
			AddToOutput(":0x");
			Extract32BitImm(stream);
			break;
		default:
			AddToOutput("ExtractOffset - Error in size");
			break;
	}
}


void ExtractSIB(int m,InStream* stream)
{
	const char ss[4][3]={"*1","*2","*4","*8"};
	unsigned char SIB = GetNextByteFromStream(stream);
	int scale = (SIB&0xC0)>>6;
	int index = (SIB&0x38)>>3;
	int base = (SIB&0x07)>>0;

	if (base!=5)
	{
		AddToOutput(reg32[base]);
		if (index!=4)
		{
			AddToOutput("+");
		}
	}
	if (base==5 && m!=0)
	{
		AddToOutput(reg32[5]);
		if (index!=4)
		{
			AddToOutput("+");
		}
	}
	if (index!=4)
	{
		AddToOutput(reg32[index]);
		AddToOutput(ss[scale]);
	}
	if (base==5 && m==0)
	{
		if (m==0)
		{
			if (index!=4)
			{
				AddToOutput("+");
			}
			AddToOutput("0x");
			Extract32BitImm(stream);
		}
	}
}


// if msize is 1 we need to use a slightly different decoding logic to generate 16 bit references (x64 this is not available!)
// // TODO cope with m and r size overrides -- currently assumes 32 bit
//seg -1 means no override.. when override specified we always display the segment
//
//
// 16 bit addressing						32 bit addressing
//
// [BX+SI]							
// [BX+DI]
// [BP+SI]
// [BP+DI]
// [SI]
// [DI]
// disp16
// [BX]
//
//
//
//
//
//
//
//
void OutputMemoryReference(int seg,int rsize,int psize,int msize,int mr,int disp,InStream* stream)
{
	switch (psize)
	{
		case -1:
			break;		// Surpress PTR (used by LEA)
		case 0:
			AddToOutput("BYTE PTR ");
			break;
		case 1:
			AddToOutput("WORD PTR ");
			break;
		case 2:
			AddToOutput("DWORD PTR ");
			break;
		case 4:
			AddToOutput("QWORD PTR ");
			break;
		case 16:
			AddToOutput("XMMWORD PTR ");
			break;
		case 17:
			AddToOutput("DWORD PTR ");
			break;
		case 18:
			AddToOutput("FWORD PTR ");
			break;
		default:
			AddToOutput("OutputMemoryReference-Todo");
			break;
	}

	// Segment override
	if (seg!=-1)
	{
		AddToOutput(segReg[seg]);
		AddToOutput(":");
	}

	if (msize==1)
	{
		switch (mr)
		{
			case 0:
				AddToOutput("[BX+SI");
				break;
			case 1:
				AddToOutput("[BX+DI");
				break;
			case 2:
				AddToOutput("[BP+SI");
				break;
			case 3:
				AddToOutput("[BP+DI");
				break;
			case 4:
				AddToOutput("[SI");
				break;
			case 5:
				AddToOutput("[DI");
				break;
			case 6:
				if (disp==1 || disp==2)
				{
					AddToOutput("[BP");
				}
				else
				{
					if (seg==-1)
					{
						AddToOutput("DS:");
					}
					AddToOutput("0x");
					Extract16BitImm(stream);
				}
				break;
			case 7:
				AddToOutput("[BX");
				break;
		}

		if (disp==1)
		{
			AddToOutput("+0x");
			Extract8BitImm(stream);
		}
		if (disp==2)
		{
			AddToOutput("+0x");
			Extract16BitImm(stream);
		}

		if (mr!=6 || disp==1 || disp==2)
		{
			AddToOutput("]");
		}
	}
	else
	{
		switch (mr)
		{
			case 4:
				AddToOutput("[");
				ExtractSIB(disp,stream);
				break;
			case 5:
				if (disp==0)
				{
					if (seg==-1)
					{
						AddToOutput("DS:");
					}
					AddToOutput("0x");
					Extract32BitImm(stream);
					break;
				}
				// Intentional fall through
			default:
				AddToOutput("[");
				AddToOutput(reg32[mr]);
				break;
		}

		if (disp==1)
		{
			AddToOutput("+0x");
			Extract8BitImm(stream);
		}
		if (disp==2)
		{
			AddToOutput("+0x");
			Extract32BitImm(stream);
		}

		if (mr!=5 || disp!=0)
		{
			AddToOutput("]");
		}
	}
}

void ExtractSR(int r)
{
	AddToOutput(segReg[r]);
}

void ExtractMMX(int r)
{
	AddToOutput(regMMX[r]);
}

void ExtractR(int size,int r)
{
	switch (size)
	{
		case 0:
			AddToOutput(reg8[r]);
			break;
		case 1:
			AddToOutput(reg16[r]);
			break;
		case 2:
			AddToOutput(reg32[r]);
			break;
		default:
			AddToOutput("ExtractR-Todo");
			break;
	}
}

void ExtractCR(int r)
{
	AddToOutput(regCr[r]);
}

void ExtractDR(int r)
{
	AddToOutput(regDr[r]);
}

void ExtractMOD(int seg,int rsize,int psize,int msize,int m,int rm,InStream* stream)
{
	switch (m)
	{
		default:
			OutputMemoryReference(seg,rsize,psize,msize,rm,m,stream);
			break;
		case 3:
			ExtractR(rsize,rm);
			break;
	}
}

void ExtractMODMMX(int seg,int rsize,int psize,int msize,int m,int rm,InStream* stream)
{
	switch (m)
	{
		default:
			OutputMemoryReference(seg,rsize,psize,msize,rm,m,stream);
			break;
		case 3:
			ExtractMMX(rm);
			break;
	}
}

void ProcessOperands(int seg,int rSize,int mSize,unsigned char opcode,const Table* cur,InStream* stream)
{
	int a;
	unsigned char MODRM;
	unsigned char m=0,r=0,rm=0;

	for (a=0;a<TOTAL_OPERANDS;a++)
	{
		if (a!=0 && cur[opcode].operands[a-1]!=OM_HasW && cur[opcode].operands[a-1]!=OM_JustW && cur[opcode].operands[a-1]!=OM_MODRM && cur[opcode].operands[a]!=OM_NoOperands && cur[opcode].operands[a-1]!=OM_cc && cur[opcode].operands[a-1]!=OM_SPACE && cur[opcode].operands[a-1]!=OM_Precision66 && cur[opcode].operands[a-1]!=OM_Precision66F3F2)
		{
			AddToOutput(",");
		}
		switch (cur[opcode].operands[a])
		{
			case OM_JustW:
				if (rSize==1)
				{
					AddToOutput("W");
				}
				break;
			case OM_HasW:
				if (rSize==1)
				{
					Remove1FromOutput();
					AddToOutput("W");
					if (cur[opcode].operands[a+1]!=OM_NoOperands)
					{
						AddToOutput(" ");
					}
				}
				break;
			case OM_SPACE:
				AddToOutput(" ");
				break;
			case OM_ONE:
				AddToOutput("1");
				break;
			case OM_MODRM:
				MODRM=GetNextByteFromStream(stream);
				m=(MODRM&0xC0)>>6;
				r=(MODRM&0x38)>>3;
				rm=(MODRM&0x07)>>0;
				break;
			case OM_Cd:
				ExtractCR(r);
				break;
			case OM_Dd:
				ExtractDR(r);
				break;
			case OM_Eb:
				ExtractMOD(seg,0,0,mSize,m,rm,stream);
				break;
			case OM_Ep:
				ExtractMOD(seg,rSize,rSize+16,mSize,m,rm,stream);
				break;
			case OM_Ev:
				ExtractMOD(seg,rSize,rSize,mSize,m,rm,stream);
				break;
			case OM_Ew:
				ExtractMOD(seg,1,1,mSize,m,rm,stream);
				break;
			case OM_Gb:
				ExtractR(0,r);
				break;
			case OM_Gv:
				ExtractR(rSize,r);
				break;
			case OM_Gw:
				ExtractR(1,r);
				break;
			case OM_Gy:
				ExtractR(2,r);
				break;
			case OM_Gz:
				ExtractR(rSize,r);
				break;
			case OM_AL:
				ExtractR(0,0);
				break;
			case OM_eAX:
				ExtractR(rSize,0);
				break;
			case OM_CL:
				ExtractR(0,1);
				break;
			case OM_Ib:
				ExtractImm(0,stream);
				break;
			case OM_Iv:
				ExtractImm(rSize,stream);
				break;
			case OM_Iw:
				ExtractImm(1,stream);
				break;
			case OM_Iz:
				ExtractImm(rSize,stream);
				break;
			case OM_ES:
			case OM_CS:
			case OM_SS:
			case OM_DS:
			case OM_FS:
			case OM_GS:
				AddToOutput(segReg[cur[opcode].operands[a]-OM_ES]);
				break;
			case OM_GPRb:
				ExtractR(0,opcode&7);
				break;
			case OM_GPR:
				ExtractR(rSize,opcode&7);
				break;
			case OM_Ma:
				if (m==3)
				{
					stream->bytesRead=0;
					return;
				}
				ExtractMOD(seg,rSize,rSize*2,mSize,m,rm,stream);
				break;
			case OM_M:
				if (m==3)
				{
					stream->bytesRead=0;
					return;
				}
				ExtractMOD(seg,rSize,-1,mSize,m,rm,stream);
				break;
			case OM_Mp:
				if (m==3)
				{
					stream->bytesRead=0;
					return;
				}
				ExtractMOD(seg,rSize,rSize+16,mSize,m,rm,stream);
				break;
			case OM_Mf:
				if (m==3)
				{
					stream->bytesRead=0;
					return;
				}
				ExtractMOD(seg,1,18,1,m,rm,stream);
				break;
			case OM_Rd:
				ExtractR(2,rm);
				break;
			case OM_Sw:
				ExtractSR(r);
				break;
			case OM_Yb:
				if (mSize==1)
				{
					OutputMemoryReference(seg!=-1?seg:0,0,0,mSize,5,0,stream);
				}
				else
				{
					OutputMemoryReference(seg!=-1?seg:0,0,0,mSize,7,0,stream);
				}
				break;
			case OM_Yv:
				if (mSize==1)
				{
					OutputMemoryReference(seg!=-1?seg:0,rSize,rSize,mSize,5,0,stream);
				}
				else
				{
					OutputMemoryReference(seg!=-1?seg:0,rSize,rSize,mSize,7,0,stream);
				}
				break;
			case OM_Yz:
				if (mSize==1)
				{
					OutputMemoryReference(seg!=-1?seg:0,rSize,rSize,mSize,5,0,stream);
				}
				else
				{
					OutputMemoryReference(seg!=-1?seg:0,rSize,rSize,mSize,7,0,stream);
				}
				break;
			case OM_DX:
				ExtractR(1,2);
				break;
			case OM_Xb:
				if (mSize==1)
				{
					OutputMemoryReference(seg!=-1?seg:3,0,0,mSize,4,0,stream);
				}
				else
				{
					OutputMemoryReference(seg!=-1?seg:3,0,0,mSize,6,0,stream);
				}
				break;
			case OM_Xv:
				if (mSize==1)
				{
					OutputMemoryReference(seg!=-1?seg:3,rSize,rSize,mSize,4,0,stream);
				}
				else
				{
					OutputMemoryReference(seg!=-1?seg:3,rSize,rSize,mSize,6,0,stream);
				}
				break;
			case OM_Xz:
				if (mSize==1)
				{
					OutputMemoryReference(seg!=-1?seg:3,rSize,rSize,mSize,4,0,stream);
				}
				else
				{
					OutputMemoryReference(seg!=-1?seg:3,rSize,rSize,mSize,6,0,stream);
				}
				break;
			case OM_Precision66:
				if (rSize==2)
				{
					AddToOutput("S ");
				}
				else
				{
					AddToOutput("D ");
				}
				break;
			case OM_Precision66F3F2:
				/// TODO F3/F2
				if (rSize==2)
				{
					AddToOutput("PS ");
				}
				else
				{
					AddToOutput("PD ");
				}
				break;
			case OM_Ups:
				if (m!=3)
				{
					stream->bytesRead=0;
					return;
				}
				ExtractMMX(rm);
				break;
			case OM_Vps:
				ExtractMMX(r);
				break;
			case OM_Wps:
				ExtractMODMMX(seg,16,16,16,m,rm,stream);
				break;
			case OM_Ob:
				ExtractOffset(seg,mSize,stream);
				break;
			case OM_Ov:
				ExtractOffset(seg,mSize,stream);
				break;
			case OM_Jb:
				ExtractDisp(0,stream);
				break;
			case OM_Jz:
				ExtractDisp(rSize,stream);
				break;
			case OM_cc:
				AddToOutput(cc[opcode&0xF]);
				AddToOutput(" ");
				break;
			case OM_FAR:
				switch (rSize)
				{
					case 1:
						{
							unsigned short addr = Get16BitImm(stream);
							AddToOutput("0x");
							Extract16BitImm(stream);
							AddToOutput(":0x");
							AppendHexValue16(addr);
							break;

						}
					case 2:
						{
							unsigned int addr = Get32BitImm(stream);
							AddToOutput("0x");
							Extract16BitImm(stream);
							AddToOutput(":0x");
							AppendHexValue32(addr);
							break;
						}
					default:
						AddToOutput("ProcessOperands - Error OM_FAR");
						break;
				}
				break;
			case OM_WDEBW:
				switch (rSize)
				{
					case 1:
						AddToOutput("BW");
						break;
					case 2:
						AddToOutput("WDE");
						break;
					default:
						AddToOutput("Unknown OM_WDEBW");
						break;
				}
				break;
			case OM_DQWD:
				switch (rSize)
				{
					case 1:
						AddToOutput("WD");
						break;
					case 2:
						AddToOutput("DQ");
						break;
					default:
						AddToOutput("Unknown OM_DQWD");
						break;
				}
				break;
			case OM_XLAT:
				AddToOutput(" ");
				if (mSize==1)
				{
					OutputMemoryReference(seg!=-1?seg:3,0,0,mSize,7,0,stream);
				}
				else
				{
					OutputMemoryReference(seg!=-1?seg:3,0,0,mSize,3,0,stream);
				}
				break;
			case OM_Grp1_0:
				cur=grp1EbIb;
				opcode=r;
				AddToOutput(grp1Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp1_1:
				cur=grp1EvIz;
				opcode=r;
				AddToOutput(grp1Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp1_2:
				cur=grp1EvIb;
				opcode=r;
				AddToOutput(grp1Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp1A:
				cur=grp1A;
				opcode=r;
				AddToOutput(grp1AMnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp2_0:
				cur=grp2EbIb;
				opcode=r;
				AddToOutput(grp2Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp2_1:
				cur=grp2EvIb;
				opcode=r;
				AddToOutput(grp2Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp2_2:
				cur=grp2Eb1;
				opcode=r;
				AddToOutput(grp2Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp2_3:
				cur=grp2Ev1;
				opcode=r;
				AddToOutput(grp2Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp2_4:
				cur=grp2EbCL;
				opcode=r;
				AddToOutput(grp2Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp2_5:
				cur=grp2EvCL;
				opcode=r;
				AddToOutput(grp2Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp3_0:
				cur=grp3_0;
				opcode=r;
				AddToOutput(grp3Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp3_1:
				cur=grp3_1;
				opcode=r;
				AddToOutput(grp3Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp4:
				cur=grp4;
				opcode=r;
				AddToOutput(grp4Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp5:
				cur=grp5;
				opcode=r;
				AddToOutput(grp5Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp7:
				cur=grp7;
				opcode=r;
				AddToOutput(grp7Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp11_0:
				cur=grp11_0;
				opcode=r;
				AddToOutput(grp11_0Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp11_1:
				cur=grp11_1;
				opcode=r;
				AddToOutput(grp11_1Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a=-1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_Grp12:
				cur = grp12;
				opcode = r;
				AddToOutput(grp12Mnemonics[cur[opcode].mnemonic]);
				AddToOutput(" ");
				a = -1;			// -1 since we will do the a++ at the start of the loop
				break;
			case OM_NoOperands:
				return;
			case OM_OpcodeExtensionByte:
				opcode=GetNextByteFromStream(stream);
				cur=_2byte;
				AddToOutput(Mnemonics[cur[opcode].mnemonic]);
				a=-1;
				break;

			case OM_Illegal:
				AddToOutput("Illegal Opcode Sequence");
				return;

			default:
				AddToOutput("Operand Unknown");
				break;

		}
	}
}

extern const char *Z80_DIS_[256];			// FROM EDL
extern const char *Z80_DIS_CB[256];			// FROM EDL
extern const char *Z80_DIS_DD[256];			// FROM EDL
extern const char *Z80_DIS_DDCB[256];		// FROM EDL
extern const char *Z80_DIS_ED[256];			// FROM EDL
extern const char *Z80_DIS_FD[256];			// FROM EDL
extern const char *Z80_DIS_FDCB[256];		// FROM EDL
extern unsigned int Z80_DIS_max_;			// FROM EDL
extern unsigned int Z80_DIS_max_ED;			// FROM EDL
extern unsigned int Z80_DIS_max_FD;			// FROM EDL
extern unsigned int Z80_DIS_max_FDCB;		// FROM EDL
extern unsigned int Z80_DIS_max_CB;			// FROM EDL
extern unsigned int Z80_DIS_max_DD;			// FROM EDL
extern unsigned int Z80_DIS_max_DDCB;		// FROM EDL
void DisassembleZ80(InStream* stream, const char *table[256], unsigned int realLength);

void Disassemble(InStream* stream,int _32bitCode)
{
	outputDis[0]=0;

    if (stream->cpu == CPU_Z80)
    {
        DisassembleZ80(stream, Z80_DIS_,Z80_DIS_max_);
        return;
    }

	int currSize=_32bitCode?2:1;	// 32/16 bit mode
	int curmSize=_32bitCode?2:1;
	int seg=-1;
	int prefixCrude;
	const Table* table = _1byte;


	for (prefixCrude=0;prefixCrude<4;prefixCrude++)
	{
		unsigned char nxtByte=GetNextByteFromStream(stream);

		// Handle PREFIXES etc
		if (table[nxtByte].mnemonic==0)
		{
			switch (table[nxtByte].operands[0])
			{
				case OM_OSize:
					currSize=_32bitCode?1:2;
					break;
				case OM_MSize:
					curmSize=_32bitCode?1:2;
					break;
				case OM_ES:
				case OM_CS:
				case OM_SS:
				case OM_DS:
				case OM_FS:
				case OM_GS:
					seg=table[nxtByte].operands[0]-OM_ES;
					break;
				case OM_REP:
					AddToOutput("REP ");
					break;
				case OM_REPNE:
					AddToOutput("REPNE ");
					break;
				default:
					AddToOutput("Unhandled prefix");		// NOTE, F2/F3 need to be flagged forward for mmx instructions due to use for encoding their precision
					return;
			}
		}
		else
		{
			AddToOutput(Mnemonics[table[nxtByte].mnemonic]);
			ProcessOperands(seg,currSize,curmSize,nxtByte,table,stream);
			return;
		}
	}
}

const char* GetOutputBuffer()
{
	return outputDis;
}

//// Z80


void DisassembleZ80(InStream* stream, const char *table[256], unsigned int realLength)
{
	int peekBytes = 0;
    unsigned char byte = PeekByteFromStreamOffset(stream,0);
	peekBytes++;
    if (byte>realLength)
    {
        AddToOutput("Unknown Opcode");
        stream->bytesRead = 0;
        return;
    }

    const char* mnemonic = (char*)table[byte];
	
    if (!mnemonic)
    {
        AddToOutput("Unknown Opcode");
        stream->bytesRead = 0;
        return;
    }

    if (strcmp(mnemonic, "CB") == 0)
    {
        DisassembleZ80(stream, Z80_DIS_CB, Z80_DIS_max_CB);
    }
    else if (strcmp(mnemonic, "DD") == 0)
    {
        DisassembleZ80(stream, Z80_DIS_DD, Z80_DIS_max_DD);
    }
    else if (strcmp(mnemonic, "DDCB") == 0)
    {
        DisassembleZ80(stream, Z80_DIS_DDCB, Z80_DIS_max_DDCB);
    }
    else if (strcmp(mnemonic, "FDCB") == 0)
    {
        DisassembleZ80(stream, Z80_DIS_FDCB, Z80_DIS_max_FDCB);
    }
    else if (strcmp(mnemonic, "ED") == 0)
    {
        DisassembleZ80(stream, Z80_DIS_ED, Z80_DIS_max_ED);
    }
    else if (strcmp(mnemonic, "FD") == 0)
    {
        DisassembleZ80(stream, Z80_DIS_FD, Z80_DIS_max_FD);
    }
    else
    {
        char sprintBuffer[3];
        const char *sPtr = mnemonic;
        int doingDecode = 0;
        while (*sPtr)
        {
            if (!doingDecode)
            {
                if (*sPtr == '%')
                {
                    doingDecode = 1;
                }
                else
                {
                    AddToOutputChar(*sPtr);
                }
            }
            else
            {
                int negOffs = 1;
                if (*sPtr == '-')
                {
                    sPtr++;
                    negOffs = -1;
                }
                int offset = (*sPtr - '0')*negOffs;
                sprintf(sprintBuffer, "%02X", PeekByteFromStreamOffset(stream, offset));
				peekBytes++;
                AddToOutputChar(sprintBuffer[0]);
                AddToOutputChar(sprintBuffer[1]);
                doingDecode = 0;
            }
            sPtr++;
        }
    }
	stream->bytesRead += peekBytes;
}

