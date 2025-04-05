/*

   ASIC test

   Currently contains some REGISTERS and some video hardware - will move to EDL eventually
   */

#ifndef _ASIC__H
#define _ASIC__H

// According to docs for PAL - 17.734475 Mhz crystal - divided by 1.5	-- 11.822983 Mhz clock
//
//  11822983 ticks / 50  = 236459.66  (236459 ticks per frame)
//  236459 / 312 = 757 clocks per line
//
// Clocks per line is approximate but probably close enough - 757 clocks - matches documentation
//
//  active display is 120 to 631 horizontal		-- From documentation
//  active display is 33 to 288 vertical		-- From documentation
//

#define WIDTH	(757)			// Should probably remove hsync period and overscan
#define	HEIGHT	(312)			// Should probably remove vsync period and overscan

void TickAsicMSU(int cycles);
void TickAsicP88(int cycles);
void TickAsicP89(int cycles);
void TickAsicFL1(int cycles);
void TickAsicCP1(int cycles);

void ASIC_WriteMSU(uint16_t port,uint8_t byte,int warnIgnore);
void ASIC_WriteP88(uint16_t port,uint8_t byte,int warnIgnore);
void ASIC_WriteP89(uint16_t port,uint8_t byte,int warnIgnore);
void ASIC_WriteFL1(uint16_t port,uint8_t byte,int warnIgnore);

uint8_t ASIC_ReadP88(uint16_t port,int warnIgnore);
uint8_t ASIC_ReadP89(uint16_t port,int warnIgnore);
uint8_t ASIC_ReadFL1(uint16_t port,int warnIgnore);

extern uint32_t		ASIC_BANK0;
extern uint32_t		ASIC_BANK1;
extern uint32_t		ASIC_BANK2;
extern uint32_t		ASIC_BANK3;

void ASIC_INIT();

#endif//_ASIC__H
