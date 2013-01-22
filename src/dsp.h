/*

	ASIC DSP stuff (the memory --- cpu is in edl)

*/

#ifndef _DSP__H
#define _DSP__H

	void ASIC_HostDSPMemWriteMSU(uint16_t address,uint8_t byte);
	uint8_t ASIC_HostDSPMemReadMSU(uint16_t address);
	void ASIC_HostDSPMemWriteP88(uint16_t address,uint8_t byte);
	uint8_t ASIC_HostDSPMemReadP88(uint16_t address);

	void TickDSP();
	void TickFL1DSP();
	void DSP_RAM_INIT();

	extern int doDSPDisassemble;

	void DSP_TranslateInstruction(uint16_t addr,uint16_t pWord);
	void DSP_TranslateInstructionFL1(uint16_t addr,uint16_t pWord);

#endif//_DSP__H


