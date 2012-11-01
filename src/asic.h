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

	void TickAsic(int cycles);
	void ASIC_Write(uint16_t port,uint8_t byte,int warnIgnore);

	void ASIC_HostDSPMemWrite(uint16_t address,uint8_t byte);
	uint8_t ASIC_HostDSPMemRead(uint16_t address);

#endif//_ASIC__H
