/*

 Floppy Disk Emulation (WD1772-PH)

 Far from complete. Basically enough to get CPM/BIOS booting (hopefully)


 i've artificially slowed the drive speed down (mostly so the initial screen doesn't get missed)

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "system.h"
#include "logfile.h"

uint8_t FDCDrive=0;
uint8_t FDCSide=0;
uint8_t FDCTrack=0;
uint8_t FDCSector=0;
uint8_t FDCData=0;
uint8_t FDCCurCommand=0;
uint8_t FDCStatus=0;

//#if ENABLE_DEBUG
void DUMP_COMMAND_FORMAT(uint8_t byte)
{
	switch (byte&0xF0)
	{
		case 0x00:		// RESTORE
			CONSOLE_OUTPUT("FDC RESTORE : %02X\n",byte);
			break;

		case 0x10:		// SEEK
			CONSOLE_OUTPUT("FDC SEEK : %02X\n",byte);
			break;

		case 0x20:		// STEP
		case 0x30:
			CONSOLE_OUTPUT("FDC STEP : %02X\n",byte);
			break;

		case 0x40:		// STEP-IN
		case 0x50:
			CONSOLE_OUTPUT("FDC STEP-IN : %02X\n",byte);
			break;

		case 0x60:		// STEP-OUT
		case 0x70:
			CONSOLE_OUTPUT("FDC STEP-OUT : %02X\n",byte);
			break;

		case 0x80:		// READ SECTOR
		case 0x90:
			CONSOLE_OUTPUT("FDC READ SECTOR : %02X\n",byte);
			break;

		case 0xA0:		// WRITE SECTOR
		case 0xB0:
			CONSOLE_OUTPUT("FDC WRITE SECTOR : %02X\n",byte);
			break;

		case 0xC0:		// READ ADDRESS
			CONSOLE_OUTPUT("FDC READ ADDRESS : %02X\n",byte);
			break;

		case 0xD0:		// FORCE INTERRUPT
			CONSOLE_OUTPUT("FDC FORCE INTERRUPT : %02X\n",byte);
			break;

		case 0xE0:		// READ TRACK
			CONSOLE_OUTPUT("FDC READ TRACK : %02X\n",byte);
			break;

		case 0xF0:		// WRITE TRACK
			CONSOLE_OUTPUT("FDC WRITE TRACK : %02X\n",byte);
			break;

	}

	switch (byte&0xF0)
	{
		case 0x00:
		case 0x10:
		case 0x20:		// STEP
		case 0x30:
		case 0x40:		// STEP-IN
		case 0x50:
		case 0x60:		// STEP-OUT
		case 0x70:
			CONSOLE_OUTPUT("Motor %s : Verify %s : Step Rate %s\n",
			byte&0x08?"Off":"On",
			byte&0x04?"On":"Off",
			(byte&0x03)==0?"6ms":(byte&0x03)==1?"12ms":(byte&0x03)==2?"2ms":"3ms"
			);
			break;

		case 0x80:		// READ SECTOR
		case 0x90:
		case 0xC0:		// READ ADDRESS
		case 0xE0:		// READ TRACK
			CONSOLE_OUTPUT("%s Sector : Motor %s : Delay %s\n",
			byte&0x10?"Multiple":"Single",
			byte&0x08?"Off":"On",
			byte&0x04?"15ms":"0ms"
			);
			break;
		case 0xA0:		// WRITE SECTOR
		case 0xB0:
		case 0xF0:		// WRITE TRACK
			CONSOLE_OUTPUT("%s Sector : Motor %s : Delay %s : Write Precomp %s : Write %s Data Mark\n",
			byte&0x10?"Multiple":"Single",
			byte&0x08?"Off":"On",
			byte&0x04?"15ms":"0ms",
			byte&0x02?"Off":"On",
			byte&0x04?"Deleted":"Normal"
			);
			break;

		case 0xD0:		// FORCE INTERRUPT
			break;
	}
}
/*#else
void DUMP_COMMAND_FORMAT(uint8_t byte)
{
}
#endif*/


// 9 Sectors (512 byte) * 80 tracks apparantly	720k (double sided!!) disks

// Track 0 appears to be just the directory table

#define SECTOR_LEN	(0x200)

extern uint8_t dskABuffer[720*1024];
extern uint8_t dskBBuffer[720*1024];

uint8_t SectorBuffer[SECTOR_LEN];
uint16_t sectorPos=0;
uint8_t Z80_GetByte(uint16_t addr);

void GoDebug();

void FDC_SetCommand(uint8_t byte)
{
	//DUMP_COMMAND_FORMAT(byte);
	FDCCurCommand=0;
	switch (byte&0xF0)
	{
		case 0x00:		// RESTORE
			memset(SectorBuffer,0xFF,SECTOR_LEN);
			FDCTrack=0;
			break;

		case 0x10:		// SEEK
			FDCTrack=FDCData;
			break;

		case 0x20:		// STEP
		case 0x30:
			break;

		case 0x40:		// STEP-IN
		case 0x50:
			break;

		case 0x60:		// STEP-OUT
		case 0x70:
			break;

		case 0x80:		// READ SECTOR
		case 0x90:
		{
			// Disk Format is interleaved sides.. so SIDE 0 TRACK 0 SECTORS 1-9, SIDE 1 TRACK 0 SECTORS 1-9, SIDE 0 TRACK 1 SECTORS 1-9.....

			int calcOffsetInDiskBuffer=(512*9)*(FDCSide&1);
			calcOffsetInDiskBuffer+=(FDCSector-1)*512;
			calcOffsetInDiskBuffer+=FDCTrack*(512*9*2);

			if (FDCDrive&1)
			{
				memcpy(SectorBuffer,&dskBBuffer[calcOffsetInDiskBuffer],512);
			}
			else
			{
				memcpy(SectorBuffer,&dskABuffer[calcOffsetInDiskBuffer],512);
			}

			FDCCurCommand=1;
			FDCData=SectorBuffer[0];
			sectorPos=0;
			FDCStatus|=0x03;	// Set busy -- but not data ready... we slow things down this way for now    |and data ready on drive
			break;
		}
		case 0xA0:		// WRITE SECTOR
		case 0xB0:
			FDCCurCommand=2;
			FDCStatus|=0x03;
			sectorPos=0;
			break;

		case 0xC0:		// READ ADDRESS
			break;

		case 0xD0:		// FORCE INTERRUPT
			break;

		case 0xE0:		// READ TRACK
			break;

		case 0xF0:		// WRITE TRACK
			break;

	}
}

void FDC_SetTrack(uint8_t byte)
{
	FDCTrack=byte;
	//CONSOLE_OUTPUT("FDC Track : %02X\n",byte);
}

void FDC_SetSector(uint8_t byte)
{
	FDCSector=byte;
	//CONSOLE_OUTPUT("FDC Sector : %02X\n",byte);
}

void FDC_SetData(uint8_t byte)
{
	FDCData=byte;
	if (FDCCurCommand==2)
	{
		SectorBuffer[sectorPos]=byte;
		sectorPos++;
		if (sectorPos>=SECTOR_LEN)
		{
			int calcOffsetInDiskBuffer=(512*9)*(FDCSide&1);
			calcOffsetInDiskBuffer+=(FDCSector-1)*512;
			calcOffsetInDiskBuffer+=FDCTrack*(512*9*2);

			if (FDCDrive&1)
			{
				memcpy(&dskBBuffer[calcOffsetInDiskBuffer],SectorBuffer,512);
			}
			else
			{
				memcpy(&dskABuffer[calcOffsetInDiskBuffer],SectorBuffer,512);
			}
			FDCStatus&=~0x3;	// Clear ready and busy flags
		}
	}
	else
	{
//		CONSOLE_OUTPUT("FDC Data : %02X\n",byte);
	}
}

void FDC_SetSide(uint8_t byte)
{
	FDCSide=byte;
//	CONSOLE_OUTPUT("FDC Side : %02X\n",byte);
}

void FDC_SetDrive(uint8_t byte)
{
	FDCDrive=byte;
//	CONSOLE_OUTPUT("FDC Drive : %02X\n",byte);
}

int drainCounter=100;

uint8_t FDC_GetStatus()
{
	if (FDCCurCommand==1)
	{
		if (drainCounter>0)
		{
			drainCounter--;
		}
		else
		{
			FDCStatus|=2;
			drainCounter=100;
		}
	}

	return FDCStatus;
}

uint8_t FDC_GetTrack()
{
	return FDCTrack;
}

uint8_t FDC_GetSector()
{
	return FDCSector;
}

uint8_t FDC_GetData()
{
	uint8_t ret = FDCData;

	if (FDCCurCommand==1)
	{
		sectorPos++;
		if (sectorPos>=SECTOR_LEN)
		{
			FDCStatus&=~0x3;	// Clear ready and busy flags
			FDCCurCommand=0;
		}
		else
		{
			FDCStatus&=~0x2;
			FDCData=SectorBuffer[sectorPos];
		}
	}
	return ret;
}


