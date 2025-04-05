#ifndef _MEMORY__H
#define _MEMORY__H

#define SEGTOPHYS(seg,off)	( ((seg)<<4) + (off) )				// Convert Segment,offset pair to physical address

#define RAM_SIZE	(1024*1024)			// Right lets get a bit more serious with available ram. (Ill make the assumption for now it extends from segment 0x0000 -> 0xC000
                                        // which is 768k - hardware chips reside above this point (with the bios assumed to reside are E000) - NB: Memory map differs for earlier models!
                                        // Upgraded due to Flare One (to be honest a lot of the space is unused, but this keeps the mapping simpler)

#define ROM_SIZE	(128*1024)

extern unsigned char RAM[RAM_SIZE];							
extern unsigned char ROM[ROM_SIZE];							

uint8_t Z80_GetByte(uint16_t addr);
void Z80_SetByte(uint16_t addr,uint8_t byte);
uint8_t Z80_GetPort(uint16_t addr);
void Z80_SetPort(uint16_t addr,uint8_t byte);

uint8_t GetByte(uint32_t addr);
void SetByte(uint32_t addr,uint8_t byte);
uint8_t GetPortB(uint16_t port);
void SetPortB(uint16_t port,uint8_t byte);
uint16_t GetPortW(uint16_t port);
void SetPortW(uint16_t port,uint16_t word);

void TickKeyboard();
void PALETTE_INIT();
void VECTORS_INIT();			// Because i have no bios, this will setup interrupt vectors - which is a job the bios normally does
void MEMORY_INIT();

extern uint16_t DSP_STATUS;


#endif//_MEMORY__H

