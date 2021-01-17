//
//
//

#ifndef _DISASM_H
#define _DISASM_H

enum CPU
{
    CPU_X86,
    CPU_Z80
};

struct InStream
{
    enum CPU cpu;
	unsigned int bytesRead;
	unsigned int curAddress;
	unsigned int useAddress;
	const char* (*findSymbol)(unsigned int address);
	const unsigned char* (*PeekByte)(unsigned int address);
};

typedef struct InStream InStream;

void Disassemble(InStream* stream,int _32bitCode);
const char* GetOutputBuffer();

#endif//_DISASM_H
