#ifndef _FDISK_H
#define _FDISK_H

void FDC_SetCommand(uint8_t byte);
void FDC_SetTrack(uint8_t byte);
void FDC_SetSector(uint8_t byte);
void FDC_SetData(uint8_t byte);
void FDC_SetSide(uint8_t byte);
void FDC_SetDrive(uint8_t byte);
uint8_t FDC_GetStatus();
uint8_t FDC_GetTrack();
uint8_t FDC_GetSector();
uint8_t FDC_GetData();

#endif//_FDISK_H
