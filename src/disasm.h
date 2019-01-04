//
//
//

#ifndef _DISASM_H
#define _DISASM_H

struct InStream
{
	unsigned int bytesRead;
	unsigned int curAddress;
	unsigned int useAddress;
};

typedef struct InStream InStream;

void Disassemble(InStream* stream,int _32bitCode);
const char* GetOutputBuffer();

unsigned char PeekByte(unsigned int addr);


#endif//_DISASM_H
