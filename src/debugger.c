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
#include "disasm.h"

int disable_exit=1;
int doDebug=0;
int doShowPortStuff=0;
uint32_t doDebugTrapWriteAt=0xFFFFF;
int debugWatchWrites=0;
int debugWatchReads=0;
int sourceMemoryMappedDebugger = 0;

#define exit brkExit

void brkExit(int dnc)
{
#if OS_WINDOWS
    __debugbreak();
#else
    exit(dnc);
#endif
}

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

extern int pause;
uint8_t PeekByteZ80(uint32_t addr)
{
#if ENABLE_DEBUG
    uint8_t ret;
    int opause = pause;
    int tmp=debugWatchReads;
    debugWatchReads=0;
    pause = -1;
    ret=Z80_GetByte(addr);
    if (pause == -1)
        pause = opause;
    debugWatchReads=tmp;
    return ret;
#else
    return Z80_GetByte(addr);
#endif
}

void DebugWPort(uint16_t port)
{
#if ENABLE_DEBUG
    if (sourceMemoryMappedDebugger)
    {
        return;
    }
    switch (curSystem)
    {
        case ESS_P89:

            switch (port)
            {
                case 0x0000:
                    CONSOLE_OUTPUT("INTL - Vertical line interrupt location\n");
                    break;
                case 0x0001:
                    CONSOLE_OUTPUT("INTH - Vertical line interrupt location\n");
                    break;
                case 0x0004:
                    CONSOLE_OUTPUT("STARTL - screen line start\n");
                    break;
                case 0x0005:
                    CONSOLE_OUTPUT("STARTH - screen line start\n");
                    break;
                case 0x0008:
                    CONSOLE_OUTPUT("HCNTL - Horizontal Counter (diagnostic)\n");
                    break;
                case 0x0009:
                    CONSOLE_OUTPUT("HCNTH - Horizontal Counter (diagnostic)\n");
                    break;
                case 0x000C:
                    CONSOLE_OUTPUT("VCNTL - Vertical Counter (diagnostic)\n");
                    break;
                case 0x000D:
                    CONSOLE_OUTPUT("VCNTH - Vertical Counter (diagnostic)\n");
                    break;
                case 0x0010:
                    CONSOLE_OUTPUT("SCROLL1 - TL pixel address LSB\n");
                    break;
                case 0x0012:
                    CONSOLE_OUTPUT("SCROLL2 - TL pixel address middle byte\n");
                    break;
                case 0x0014:
                    CONSOLE_OUTPUT("SCROLL3 - TL pixel address MSB\n");
                    break;
                case 0x0016:
                    CONSOLE_OUTPUT("ACK - interrupt acknowledge\n");
                    break;
                case 0x0018:
                    CONSOLE_OUTPUT("MODE - screen mode  [CVHBEGMM]  MM - 0 low, 1 medium, 2 high, 3 unused - \n");
                    break;
                case 0x001A:
                    CONSOLE_OUTPUT("BORDL - border colour\n");
                    break;
                case 0x001B:
                    CONSOLE_OUTPUT("BORDH - border colour\n");
                    break;
                case 0x001E:
                    CONSOLE_OUTPUT("PMASK - palette mask\n");
                    break;
                case 0x0020:
                    CONSOLE_OUTPUT("INDEX - palette index\n");
                    break;
                case 0x0022:
                    CONSOLE_OUTPUT("ENDL - screen line end\n");
                    break;
                case 0x0023:
                    CONSOLE_OUTPUT("ENDH - screen line end\n");
                    break;
                case 0x0026:
                    CONSOLE_OUTPUT("MEM - memory configuration  [00000sMM] - s Swap Screen and DRAM addresses - MM Memory at EXP address\n");
                    break;
                case 0x0028:
                    CONSOLE_OUTPUT("GPR - General Purpose Register\n");
                    break;
                case 0x002A:
                    CONSOLE_OUTPUT("DIAG - diagnostics [000x00NP] - P set for LPEN=HVCounters - N set for NTSC - x screen mode immediate\n");
                    break;
                case 0x002C:
                    CONSOLE_OUTPUT("DIS - disable interupts [000FAAAV] - F disable floppy interrupt - A disable analogue interrupts - V disable video interrupts\n");
                    break;
                case 0x002E:
                    CONSOLE_OUTPUT("JOYO - joystick outputs\n");
                    break;
                case 0x0040:
                    CONSOLE_OUTPUT("BLPROG0\n");
                    break;
                case 0x0041:
                    CONSOLE_OUTPUT("BLPROG1\n");
                    break;
                case 0x0042:
                    CONSOLE_OUTPUT("BLPROG2\n");
                    break;
                case 0x0043:
                    CONSOLE_OUTPUT("BLTCMD\n");
                    break;
                case 0x0044:
                    CONSOLE_OUTPUT("BLTCON - blitter control [00000rRI] r - Abort blit after collision - R resume blit after collision - I disable blit stop on interrupt\n");
                    break;
                case 0x0048:
                    CONSOLE_OUTPUT("FRC - floppy read control\n");
                    break;
                case 0x0080:
                    CONSOLE_OUTPUT("DRVC - floppy drive control\n");
                    break;
                default:
                    CONSOLE_OUTPUT("PORT WRITE UNKNOWN %04X- TODO\n",port);
                    if (!disable_exit)
                        exit(-1);
                    break;
            }
            break;

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
                case 0x0071:
                    CONSOLE_OUTPUT("SERC - Serial Control\n");
                    break;
                case 0x0073:
                    CONSOLE_OUTPUT("SERD - Serial Data\n");
                    break;
                default:
                    CONSOLE_OUTPUT("PORT WRITE UNKNOWN - TODO\n");
                    if (!disable_exit)
                        exit(-1);
                    break;
            }

            break;
        case ESS_CP1:
            switch (port)
            {
                // Video Timings
                case 0x0002:
                    CONSOLE_OUTPUT("V_PERIOD - 000000vvvvvvvvvv\n");
                    break;
                case 0x0004:
                    CONSOLE_OUTPUT("V_DIS_BEG - 000000vvvvvvvvvv\n");
                    break;
                case 0x0006:
                    CONSOLE_OUTPUT("V_BLK_END - 000000vvvvvvvvvv\n");
                    break;
                case 0x000A:
                    CONSOLE_OUTPUT("V_DIS_END - 000000vvvvvvvvvv\n");
                    break;
                case 0x000E:
                    CONSOLE_OUTPUT("V_BLK_BEG - 000000vvvvvvvvvv\n");
                    break;
                case 0x0014:
                    CONSOLE_OUTPUT("V_SYNC - 000000vvvvvvvvvv\n");
                    break;
                case 0x001C:
                    CONSOLE_OUTPUT("H_PERIOD - 00000hhhhhhhhhhh\n");
                    break;
                case 0x0022:
                    CONSOLE_OUTPUT("H_BLK_END - 00000hhhhhhhhhhh\n");
                    break;
                case 0x0024:
                    CONSOLE_OUTPUT("H_FCH_BEG - 00000hhhhhhhhhhh\n");
                    break;
                case 0x002E:
                    CONSOLE_OUTPUT("H_DIS_BEG - 00000hhhhhhhhhhh\n");
                    break;
                case 0x0030:
                    CONSOLE_OUTPUT("H_FCH_END - 00000hhhhhhhhhhh\n");
                    break;
                case 0x0032:
                    CONSOLE_OUTPUT("H_DIS_END - 00000hhhhhhhhhhh\n");
                    break;
                case 0x0034:
                    CONSOLE_OUTPUT("H_BLK_BEG - 00000hhhhhhhhhhh\n");
                    break;
                case 0x0036:
                    CONSOLE_OUTPUT("H_SYNC - 00000hhhhhhhhhhh\n");
                    break;
                case 0x0038:
                    CONSOLE_OUTPUT("H_VSYNC - 00000hhhhhhhhhhh\n");
                    break;
                case 0x003C:
                    CONSOLE_OUTPUT("CHR - 000000000000dddd\n");
                    break;
                    // Rest
                case 0x0010:
                    CONSOLE_OUTPUT("SCROLL0 - aaaaaaaaaaaaaaaa\n");
                    break;
                case 0x0012:
                    CONSOLE_OUTPUT("SCROLL1 - 00000000aaaaaaaa\n");
                    break;
                case 0x0016:
                    CONSOLE_OUTPUT("ACKW -");
                    break;
                case 0x0018:
                    CONSOLE_OUTPUT("MODE - 0HXXXvvvaaabccmm\n");
                    break;
                case 0x001A:
                    CONSOLE_OUTPUT("BORDER - rrrrrggggggbbbbb\n");
                    break;
                case 0x001E:
                    CONSOLE_OUTPUT("MASK - \n");
                    break;
                case 0x0020:
                    CONSOLE_OUTPUT("INDEX - \n");
                    break;
                case 0x0026:
                    CONSOLE_OUTPUT("MEM - 00000000000000tt\n");
                    break;
                case 0x002A:
                    CONSOLE_OUTPUT("MODE2 - 000000NHVmTrrvTo\n");
                    break;
                case 0x002C:
                    CONSOLE_OUTPUT("INT - \n");
                    break;
                case 0x0040:
                    CONSOLE_OUTPUT("BLTPC0 - \n");
                    break;
                case 0x0042:
                    CONSOLE_OUTPUT("BLTPC1 - \n");
                    break;
                case 0x0044:
                    CONSOLE_OUTPUT("BLTCMD - \n");
                    break;
                default:
                    CONSOLE_OUTPUT("PORT WRITE UNKNOWN - TODO\n");
                    if (!disable_exit)
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
                case 0x0002:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
                    break;
                case 0x0004:
                    CONSOLE_OUTPUT("STARTL - screen line start (Byte address)\n");
                    break;
                case 0x0006:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
                    break;
                case 0x000A:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
                    break;
                case 0x000E:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
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
                case 0x001C:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
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
                case 0x0024:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
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
                case 0x002E:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
                    break;
                case 0x0030:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
                    break;
                case 0x0032:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
                    break;
                case 0x0034:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
                    break;
                case 0x0036:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
                    break;
                case 0x003C:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
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
                case 0x00A0:
                    CONSOLE_OUTPUT("UNKNWN - used by bios on init!\n");
                    break;
                case 0x00C0:
                    CONSOLE_OUTPUT("ADP - (Word address) - Anologue/digital port reset?\n");
                    break;
                case 0x00E0:
                    CONSOLE_OUTPUT("???? - (Byte address) - number pad reset?\n");
                    break;
                default:
                    CONSOLE_OUTPUT("PORT WRITE UNKNOWN - TODO\n");
                    if (!disable_exit)
                        exit(-1);
                    break;
            }
            break;
        case ESS_FL1:
            switch (port)
            {
                case 0x0014:
                    CONSOLE_OUTPUT("RUNST - DSP Start/Stop - Memory mapped on later models as DSP_STATUS?\n");
                    break;
                case 0x0007:
                    CONSOLE_OUTPUT("INTREG - low 8 bits of interrupt line\n");
                    break;
                case 0x0008:
                    CONSOLE_OUTPUT("CMD1 - (bit 2 is msb of INTL? , bit 6 indicates which of the 2 screen banks is visible) ... rest unknown)\n");
                    break;
                case 0x0009:
                    CONSOLE_OUTPUT("CMD2 - (bit 0 is lo/hi res select (could be MODE register),bit 3 is Colour Hold Mode)\n");
                    break;
                case 0x000A:
                    CONSOLE_OUTPUT("BORDER - (presumably palette index - but palette might be formed directly from index\n");
                    break;
                case 0x000B:
                    CONSOLE_OUTPUT("SCRLH - horizontal scroll register\n");
                    break;
                case 0x000C:
                    CONSOLE_OUTPUT("SCRLV - vertical scroll register\n");
                    break;
                case 0x0013:
                    CONSOLE_OUTPUT("MPROG - dsp program word register (double write)\n");
                    break;
                case 0x0015:
                    CONSOLE_OUTPUT("PROGRAM - dsp program address register (double write)\n");
                    break;
                case 0x0018:
                    CONSOLE_OUTPUT("BLTPC (byte 0)\n");
                    break;
                case 0x0019:
                    CONSOLE_OUTPUT("BLTPC (byte 1)\n");
                    break;
                case 0x001A:
                    CONSOLE_OUTPUT("BLTPC (byte 2)\n");
                    break;
                case 0x0020:
                    CONSOLE_OUTPUT("BLTCMD\n");
                    break;
                case 0x001B:
                    CONSOLE_OUTPUT("KBDCTL - Keyboard control registers\n");
                    break;
                case 0x0022:
                    CONSOLE_OUTPUT("GPO - General Purpose Output Port - ??? unknown use\n");
                    break;
                case 0x0000:
                    CONSOLE_OUTPUT("BANK0 - Bank Switch 0x0000-0x3FFF range of cpu\n");
                    break;
                case 0x0001:
                    CONSOLE_OUTPUT("BANK1 - Bank Switch 0x4000-0x7FFF range of cpu\n");
                    break;
                case 0x0002:
                    CONSOLE_OUTPUT("BANK2 - Bank Switch 0x8000-0xBFFF range of cpu\n");
                    break;
                case 0x0003:
                    CONSOLE_OUTPUT("BANK3 - Bank Switch 0xC000-0xFFFF range of cpu\n");
                    break;
                case 0x000D:
                    CONSOLE_OUTPUT("TRANS - pal index used for colour hold mode\n");
                    break;
                case 0x0050:
                    CONSOLE_OUTPUT("PALAW - Palette Address to change\n");
                    break;
                case 0x0051:
                    CONSOLE_OUTPUT("PALVAL - Palette entry - written 3 times, Red,Green,Blue\n");
                    break;
                case 0x0052:
                    CONSOLE_OUTPUT("PALMASK - unclear but should only affect palette board loading I suspect\n");
                    break;
                case 0x0010:
                    CONSOLE_OUTPUT("INTRD - Intrude Data Register\n");
                    break;
                case 0x0011:
                    CONSOLE_OUTPUT("INTRDP - Intrude Data Register (with Post increment Intrude Address)\n");
                    break;
                case 0x0012:
                    CONSOLE_OUTPUT("INTRA - Intrude Address Register (reportedly 12 bits)\n");
                    break;
                case 0x00E0:
                    CONSOLE_OUTPUT("CONTROLL_P - Potentiometer selector\n");
                    break;
                case 0x00C0:
                    CONSOLE_OUTPUT("CHAIR_P - This is the signal to the chair?\n");
                    break;
                case 0x0030:
                    CONSOLE_OUTPUT("COMREG - Floppy Disk Command\n");
                    break;
                case 0x0031:
                    CONSOLE_OUTPUT("TRKREG - Floppy Disk Track\n");
                    break;
                case 0x0032:
                    CONSOLE_OUTPUT("SECREG - Floppy Disk Sector\n");
                    break;
                case 0x0033:
                    CONSOLE_OUTPUT("DATAREG - Floppy Disk Data\n");
                    break;
                case 0x0024:
                case 0x0028:
                case 0x002C:
                    CONSOLE_OUTPUT("UART%d Control\n", (port-0x24)/4);
                    break;
                case 0x0025:
                case 0x0029:
                case 0x002D:
                    CONSOLE_OUTPUT("UART%d Write Data\n", (port-0x25)/4);
                    break;
                case 0x0006:
                    CONSOLE_OUTPUT("BAUD - Set Baud Rate for UART1\n");
                    break;
                case 0x000E:
                    CONSOLE_OUTPUT("MAG - physical colour to replace magenta in hires");
                    break;
                case 0x000F:
                    CONSOLE_OUTPUT("MAG - physical colour to replace yellow in hires");
                    break;
                default:
                    CONSOLE_OUTPUT("PORT WRITE UNKNOWN (%04X)- TODO\n",port);
                    if (!disable_exit)
                    {
                        exit(-1);
                    }
                    break;
            }
            break;

    }

#endif
}

void DebugRPort(uint16_t port)
{
#if ENABLE_DEBUG
    if (sourceMemoryMappedDebugger)
    {
        return;
    }
    switch (curSystem)
    {
        case ESS_P89:

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
                case 0x0008:
                    CONSOLE_OUTPUT("JOYI - Joystick Inputs\n");
                    break;
                case 0x0009:
                    CONSOLE_OUTPUT("JOYI - Joystick Inputs\n");
                    break;
                case 0x000C:
                    CONSOLE_OUTPUT("STAT - Machine Status\n");
                    break;
                case 0x0040:
                    CONSOLE_OUTPUT("BLTDST0 - Blitter Destination Address 0\n");
                    break;
                case 0x0042:
                    CONSOLE_OUTPUT("BLTDST1 - Blitter Destination Address 1\n");
                    break;
                case 0x0044:
                    CONSOLE_OUTPUT("BLTSRC0 - Blitter Source Address 0\n");
                    break;
                case 0x0046:
                    CONSOLE_OUTPUT("BLTSRC1 - Blitter Source Address 1\n");
                    break;
                case 0x0048:
                    CONSOLE_OUTPUT("FSTAT - Floppy Read Status\n");
                    break;
                case 0x0080:
                    CONSOLE_OUTPUT("DSTAT - Drive Status\n");
                    break;
                default:
                    CONSOLE_OUTPUT("PORT READ UNKNOWN - TODO\n");
                    if (!disable_exit)
                        exit(-1);
                    break;
            }

            break;
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
                case 0x0071:
                    CONSOLE_OUTPUT("SERC - Serial Control\n");
                    break;
                case 0x0073:
                    CONSOLE_OUTPUT("SERD - Serial Data\n");
                    break;
                default:
                    CONSOLE_OUTPUT("PORT READ UNKNOWN - TODO\n");
                    if (!disable_exit)
                        exit(-1);
                    break;
            }

            break;
        case ESS_CP1:
            switch (port)
            {
                case 0x0C:
                    CONSOLE_OUTPUT("STAT - 00000000000000LP\n");
                    break;
                default:
                    CONSOLE_OUTPUT("PORT READ UNKNOWN - TODO\n");
                    if (!disable_exit)
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
                    if (!disable_exit)
                        exit(-1);
                    break;
            }
            break;
        case ESS_FL1:
            switch (port)
            {
                case 0x0018:
                    CONSOLE_OUTPUT("BLPC0 - low byte of Blitter address\n");
                    break;
                case 0x0019:
                    CONSOLE_OUTPUT("BLPC1 - middle byte of blitter address\n");
                    break;
                case 0x001A:
                    CONSOLE_OUTPUT("BLPC2 - upper byte of blitter address\n");
                    break;
                case 0x0020:
                    CONSOLE_OUTPUT("STOP_DST_L - x coord of dst register - used in line collisions\n");
                    break;
                case 0x0021:
                    CONSOLE_OUTPUT("STOP_DST_H - y coord of dst register - used in line collisions\n");
                    break;
                case 0x0022:
                    CONSOLE_OUTPUT("GP0 - also appears to be controller states!\n");
                    break;
                case 0x00E0:
                    CONSOLE_OUTPUT("CONTROLL_P - Potentiometer read value\n");
                    break;
                case 0x00A0:
                    CONSOLE_OUTPUT("IP_PORT - joystick button states?\n");
                    break;
                case 0x0007:
                    CONSOLE_OUTPUT("INTACK - Acknowledge interrupts on read\n");
                    break;
                case 0x0014:
                    CONSOLE_OUTPUT("RUNST - DSP Start/Stop - Memory mapped on later models as DSP_STATUS?\n");
                    break;
                case 0x0000:
                    CONSOLE_OUTPUT("BANK0 - Bank Switch 0x0000-0x3FFF range of cpu\n");
                    break;
                case 0x0001:
                    CONSOLE_OUTPUT("BANK1 - Bank Switch 0x4000-0x7FFF range of cpu\n");
                    break;
                case 0x0002:
                    CONSOLE_OUTPUT("BANK2 - Bank Switch 0x8000-0xBFFF range of cpu\n");
                    break;
                case 0x0003:
                    CONSOLE_OUTPUT("BANK3 - Bank Switch 0xC000-0xFFFF range of cpu\n");
                    break;
                case 0x0030:
                    CONSOLE_OUTPUT("COMREG - Floppy Disk Command\n");
                    break;
                case 0x0031:
                    CONSOLE_OUTPUT("TRKREG - Floppy Disk Track\n");
                    break;
                case 0x0032:
                    CONSOLE_OUTPUT("SECREG - Floppy Disk Sector\n");
                    break;
                case 0x0033:
                    CONSOLE_OUTPUT("DATAREG - Floppy Disk Data\n");
                    break;
                case 0x0040:
                    CONSOLE_OUTPUT("VADC - Video Analogue to digital port\n");
                    break;

                default:
                    CONSOLE_OUTPUT("PORT READ UNKNOWN (%04X)- TODO\n",port);
                    if (!disable_exit)
                    {
                        exit(-1);
                    }
                    break;
            }
            break;
    }
#endif
}

#if MEMORY_MAPPED_DEBUGGER

int GetILength80386(unsigned int address, int x86)
{
    InStream disMe;
    disMe.cpu = x86?CPU_X86:CPU_Z80;
    disMe.bytesRead = 0;
    disMe.curAddress = address;
    disMe.useAddress = 1;
    disMe.findSymbol = NULL;
    disMe.PeekByte = PeekByte;
    Disassemble(&disMe, MSU_cSize);

    return disMe.bytesRead;
}

int GetILength86(unsigned int address, int x86)
{
    InStream disMe;
    disMe.cpu = CPU_X86;
    disMe.bytesRead = 0;
    disMe.curAddress = address;
    disMe.useAddress = 1;
    disMe.findSymbol = NULL;
    disMe.PeekByte = PeekByte;
    Disassemble(&disMe, 0);

    return disMe.bytesRead;
}


void FETCH_REGISTERS8086(char* tmp)
{
    sprintf(tmp,"--------\nFLAGS = O  D  I  T  S  Z  -  A  -  P  -  C\n        %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s\nAX= %04X\nBX= %04X\nCX= %04X\nDX= %04X\nSP= %04X\nBP= %04X\nSI= %04X\nDI= %04X\nCS= %04X\nDS= %04X\nES= %04X\nSS= %04X\n--------\n",
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
            FLAGS&0x001 ? "1" : "0",
            AX,BX,CX,DX,SP,BP,SI,DI,CS,DS,ES,SS);
}

void FETCH_REGISTERS80386(char* tmp)
{
    sprintf(tmp,"--------\nFLAGS = O  D  I  T  S  Z  -  A  -  P  -  C\n        %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s\nEAX= %08X\nEBX= %08X\nECX= %08X\nEDX= %08X\nESP= %08X\nEBP= %08X\nESI= %08X\nEDI= %08X\nCS= %04X\nDS= %04X\nES= %04X\nSS= %04X\nFS= %04X\nGS= %04X\n\nEIP= %08X\n\nCR0= %08X\nDR7= %08X\n--------\n",
            MSU_EFLAGS&0x800 ? "1" : "0",
            MSU_EFLAGS&0x400 ? "1" : "0",
            MSU_EFLAGS&0x200 ? "1" : "0",
            MSU_EFLAGS&0x100 ? "1" : "0",
            MSU_EFLAGS&0x080 ? "1" : "0",
            MSU_EFLAGS&0x040 ? "1" : "0",
            MSU_EFLAGS&0x020 ? "1" : "0",
            MSU_EFLAGS&0x010 ? "1" : "0",
            MSU_EFLAGS&0x008 ? "1" : "0",
            MSU_EFLAGS&0x004 ? "1" : "0",
            MSU_EFLAGS&0x002 ? "1" : "0",
            MSU_EFLAGS&0x001 ? "1" : "0",
            MSU_EAX,MSU_EBX,MSU_ECX,MSU_EDX,MSU_ESP,MSU_EBP,MSU_ESI,MSU_EDI,MSU_CS,MSU_DS,MSU_ES,MSU_SS,MSU_FS,MSU_GS,MSU_EIP,MSU_CR0,MSU_DR7);
}

void FETCH_REGISTERS80386_8086(char* tmp)
{
    sprintf(tmp,"--------\nFLAGS = O  D  I  T  S  Z  -  A  -  P  -  C\n        %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s\nAX= %04X\nBX= %04X\nCX= %04X\nDX= %04X\nSP= %04X\nBP= %04X\nSI= %04X\nDI= %04X\nCS= %04X\nDS= %04X\nES= %04X\nSS= %04X\n--------\n",
            MSU_EFLAGS&0x800 ? "1" : "0",
            MSU_EFLAGS&0x400 ? "1" : "0",
            MSU_EFLAGS&0x200 ? "1" : "0",
            MSU_EFLAGS&0x100 ? "1" : "0",
            MSU_EFLAGS&0x080 ? "1" : "0",
            MSU_EFLAGS&0x040 ? "1" : "0",
            MSU_EFLAGS&0x020 ? "1" : "0",
            MSU_EFLAGS&0x010 ? "1" : "0",
            MSU_EFLAGS&0x008 ? "1" : "0",
            MSU_EFLAGS&0x004 ? "1" : "0",
            MSU_EFLAGS&0x002 ? "1" : "0",
            MSU_EFLAGS&0x001 ? "1" : "0",
            MSU_EAX&0xFFFF,MSU_EBX&0xFFFF,MSU_ECX&0xFFFF,MSU_EDX&0xFFFF,MSU_ESP&0xFFFF,MSU_EBP&0xFFFF,MSU_ESI&0xFFFF,MSU_EDI&0xFFFF,MSU_CS,MSU_DS,MSU_ES,MSU_SS);
}

void REGISTER_ADDRESS_Z80(char* buffer, char* name, uint16_t regpair)
{
    sprintf(buffer, "%s\t%04X\n",
            name,
            regpair
           );
}

void REGISTER_ADDRESS_Z80_MEM(char* buffer, char* name, uint16_t regpair)
{
    sprintf(buffer, "%s\t%04X\t\t%02X %02X %02X %02X %02X %02X %02X [%02X] %02X %02X %02X %02X %02X %02X %02X %02X\n",
            name,
            regpair,
            PeekByteZ80(regpair - 7),
            PeekByteZ80(regpair - 6),
            PeekByteZ80(regpair - 5),
            PeekByteZ80(regpair - 4),
            PeekByteZ80(regpair - 3),
            PeekByteZ80(regpair - 2),
            PeekByteZ80(regpair - 1),
            PeekByteZ80(regpair + 0),
            PeekByteZ80(regpair + 1),
            PeekByteZ80(regpair + 2),
            PeekByteZ80(regpair + 3),
            PeekByteZ80(regpair + 4),
            PeekByteZ80(regpair + 5),
            PeekByteZ80(regpair + 6),
            PeekByteZ80(regpair + 7),
            PeekByteZ80(regpair + 8)
           );
}

void FETCH_REGISTERSZ80(char* tmp)
{
    char tmpBuffer[256];

    sprintf(tmpBuffer, "FLAGS\t=\tS\tZ\t-\tH\t-\tP\tN\tC\n\t\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n\n",
            Z80_AF & 0x80 ? "1" : "0",
            Z80_AF & 0x40 ? "1" : "0",
            Z80_AF & 0x20 ? "1" : "0",
            Z80_AF & 0x10 ? "1" : "0",
            Z80_AF & 0x08 ? "1" : "0",
            Z80_AF & 0x04 ? "1" : "0",
            Z80_AF & 0x02 ? "1" : "0",
            Z80_AF & 0x01 ? "1" : "0");
    strcpy(tmp, tmpBuffer);
    strcat(tmp, "\n\t\n\t\t\t-7 -6 -5 -4 -3 -2 -1      +1 +2 +3 +4 +5 +6 +7 +8\n");
    REGISTER_ADDRESS_Z80(tmpBuffer, "AF", Z80_AF); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "BC", Z80_BC); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "DE", Z80_DE); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "HL", Z80_HL); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80(tmpBuffer, "AF'", Z80__AF); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "BC'", Z80__BC); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "DE'", Z80__DE); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "HL'", Z80__HL); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "IX", Z80_IX); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "IY", Z80_IY); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80(tmpBuffer, "IR", Z80_IR); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "SP", Z80_SP); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80_MEM(tmpBuffer, "PC", Z80_PC); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80(tmpBuffer, "IFF1", Z80_IFF1&1); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80(tmpBuffer, "IFF2", Z80_IFF2&1); strcat(tmp, tmpBuffer);
    REGISTER_ADDRESS_Z80(tmpBuffer, "IM", Z80_IM&0x3); strcat(tmp, tmpBuffer);
}

#endif

#if 0
void DUMP_REGISTERS8086()
{
    char tmp[1024];
    FETCH_REGISTERS8086(tmp);
    CONSOLE_OUTPUT(tmp);
}

void DUMP_REGISTERS80386()
{
    char tmp[1024];
    FETCH_REGISTERS80386(tmp);
    CONSOLE_OUTPUT(tmp);
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

const char* decodeDisasm8086(uint8_t *table[256],unsigned int address,int *count,int realLength)
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
            sPtr=decodeDisasm8086(DIS_,address+1,&tmpcount,DIS_max_);
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
            sPtr=decodeDisasm8086(DIS_,address+1,&tmpcount,DIS_max_);
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

int DoDisassemble8086(unsigned int address,int registers,char* tmp)
{
    char tBuffer[1024];
    int a;
    int numBytes=0;
    const char* retVal = decodeDisasm8086(DIS_,address,&numBytes,DIS_max_);

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
        //		DUMP_REGISTERS8086();
        //		exit(-1);
    }

    if (registers)
    {
        DUMP_REGISTERS8086();
    }
    sprintf(tmp,"%05X :",address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see

    for (a=0;a<numBytes+1;a++)
    {
        sprintf(tBuffer,"%02X ",PeekByte(address+a));
        strcat(tmp,tBuffer);
    }
    for (a=0;a<8-(numBytes+1);a++)
    {
        strcat(tmp,"   ");
    }
    sprintf(tBuffer,"%s\n",retVal);
    strcat(tmp,tBuffer);

    return numBytes+1;
}

int Disassemble8086(unsigned int address,int registers)
{
#if 1
    char tmp[2048];
    char tBuffer[2048];
    int a;

    InStream disMe;
    disMe.bytesRead=0;
    disMe.curAddress=address;
    disMe.useAddress=1;
    disMe.PeekByte = PeekByte;
    Disassemble(&disMe,0);

    if (disMe.bytesRead==0)
    {
        CONSOLE_OUTPUT("UNKNOWN AT : %05X\n",address);		// TODO this will fail to wrap which may show up bugs that the CPU won't see
        CONSOLE_OUTPUT("\nNext 7 Bytes : ");
        for (a=0;a<7;a++)
        {
            CONSOLE_OUTPUT("%02X ",PeekByte(address+disMe.bytesRead+a));
        }
        CONSOLE_OUTPUT("\n");
        DUMP_REGISTERS8086();
        if (!disable_exit)
            exit(-1);
    }

    if (registers)
    {
        DUMP_REGISTERS8086();
    }
    sprintf(tmp,"%05X : ",address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see

    for (a=0;a<disMe.bytesRead;a++)
    {
        sprintf(tBuffer,"%02X ",PeekByte(address+a));
        strcat(tmp,tBuffer);
    }
    for (a=0;a<8-disMe.bytesRead;a++)
    {
        strcat(tmp,"   ");
    }
    sprintf(tBuffer,"%s\n",(char*)GetOutputBuffer());
    strcat(tmp,tBuffer);

    CONSOLE_OUTPUT("--------\n");
    CONSOLE_OUTPUT(tmp);
    CONSOLE_OUTPUT("--------\n");
    return disMe.bytesRead;
#else
    int res;
    char tmp[2048];
    res=DoDisassemble8086(address,registers,tmp);
    CONSOLE_OUTPUT("--------\n");
    CONSOLE_OUTPUT(tmp);
    CONSOLE_OUTPUT("--------\n");
    return res;
#endif
}



int Disassemble80386(unsigned int address,int registers)
{
    char tmp[2048];
    char tBuffer[2048];
    int a;

    InStream disMe;
    disMe.bytesRead=0;
    disMe.curAddress=address;
    disMe.useAddress=1;
    disMe.PeekByte = PeekByte;
    Disassemble(&disMe,MSU_cSize);

    if (disMe.bytesRead==0)
    {
        CONSOLE_OUTPUT("UNKNOWN AT : %06X\n",address);		// TODO this will fail to wrap which may show up bugs that the CPU won't see
        CONSOLE_OUTPUT("\nNext 7 Bytes : ");
        for (a=0;a<7;a++)
        {
            CONSOLE_OUTPUT("%02X ",PeekByte(address+disMe.bytesRead+a));
        }
        CONSOLE_OUTPUT("\n");
        DUMP_REGISTERS80386();
        if (!disable_exit)
            exit(-1);
    }

    if (registers)
    {
        DUMP_REGISTERS80386();
    }
    sprintf(tmp,"%06X : ",address);				// TODO this will fail to wrap which may show up bugs that the CPU won't see

    for (a=0;a<disMe.bytesRead;a++)
    {
        sprintf(tBuffer,"%02X ",PeekByte(address+a));
        strcat(tmp,tBuffer);
    }
    for (a=0;a<15-disMe.bytesRead;a++)
    {
        strcat(tmp,"   ");
    }
    sprintf(tBuffer,"%s\n",(char*)GetOutputBuffer());
    strcat(tmp,tBuffer);

    CONSOLE_OUTPUT("--------\n");
    CONSOLE_OUTPUT(tmp);
    CONSOLE_OUTPUT("--------\n");
    return disMe.bytesRead;
}

int FETCH_DISASSEMBLE8086(unsigned int address,char* tmp)
{
    return DoDisassemble8086(address,0,tmp);
}

void DUMP_REGISTERSZ80()
{
    char tmp[1024];
    FETCH_REGISTERSZ80(tmp);
    CONSOLE_OUTPUT(tmp);
}

const char* decodeDisasmZ80(uint8_t *table[256],unsigned int address,int *count,int realLength)
{
    static char temporaryBuffer[2048];
    char sprintBuffer[256];

    uint8_t byte = PeekByteZ80(address);
    if (byte>realLength)
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

        if (strcmp(mnemonic,"CB")==0)
        {
            int tmpCount=0;
            decodeDisasmZ80(Z80_DIS_CB,address+1,&tmpCount,Z80_DIS_max_CB);
            *count=tmpCount+1;
            return temporaryBuffer;
        }
        if (strcmp(mnemonic,"DD")==0)
        {
            int tmpCount=0;
            decodeDisasmZ80(Z80_DIS_DD,address+1,&tmpCount,Z80_DIS_max_DD);
            *count=tmpCount+1;
            return temporaryBuffer;
        }
        if (strcmp(mnemonic,"DDCB")==0)
        {
            int tmpCount=0;
            decodeDisasmZ80(Z80_DIS_DDCB,address+2,&tmpCount,Z80_DIS_max_DDCB);
            *count=tmpCount+1;
            return temporaryBuffer;
        }
        if (strcmp(mnemonic,"FDCB")==0)
        {
            int tmpCount=0;
            decodeDisasmZ80(Z80_DIS_FDCB,address+2,&tmpCount,Z80_DIS_max_FDCB);
            *count=tmpCount+1;
            return temporaryBuffer;
        }
        if (strcmp(mnemonic,"ED")==0)
        {
            int tmpCount=0;
            decodeDisasmZ80(Z80_DIS_ED,address+1,&tmpCount,Z80_DIS_max_ED);
            *count=tmpCount+1;
            return temporaryBuffer;
        }
        if (strcmp(mnemonic,"FD")==0)
        {
            int tmpCount=0;
            decodeDisasmZ80(Z80_DIS_FD,address+1,&tmpCount,Z80_DIS_max_FD);
            *count=tmpCount+1;
            return temporaryBuffer;
        }

        while (*sPtr)
        {
            if (!doingDecode)
            {
                if (*sPtr=='%')
                {
                    doingDecode=1;
                }
                else
                {
                    *dPtr++=*sPtr;
                }
            }
            else
            {
                char *tPtr=sprintBuffer;
                int negOffs=1;
                if (*sPtr=='-')
                {
                    sPtr++;
                    negOffs=-1;
                }
                int offset=(*sPtr-'0')*negOffs;
                sprintf(sprintBuffer,"%02X",PeekByteZ80(address+offset));
                while (*tPtr)
                {
                    *dPtr++=*tPtr++;
                }
                doingDecode=0;
                counting++;
            }
            sPtr++;
        }
        *dPtr=0;
        *count=counting;
    }
    return temporaryBuffer;
}

int DoDisassembleZ80(unsigned int address,int registers,char* tmp)
{
    char tBuffer[1024];
    int a;
    int numBytes=0;
    const char* retVal;

    if (Z80_HALTED&1)
    {
        return 0;
    }

    retVal = decodeDisasmZ80(Z80_DIS_,address,&numBytes,Z80_DIS_max_);

    if (strcmp(retVal,"UNKNOWN OPCODE")==0)
    {
        CONSOLE_OUTPUT("UNKNOWN AT : %04X\n",address);
        for (a=0;a<numBytes+1;a++)
        {
            CONSOLE_OUTPUT("%02X ",PeekByteZ80(address+a));
        }
        CONSOLE_OUTPUT("\n");
        if (!disable_exit)
            exit(-1);
    }

    if (registers)
    {
        DUMP_REGISTERSZ80();
    }
    sprintf(tmp,"%05X\t",address&0xFFFFF);

    for (a=0;a<numBytes+1;a++)
    {
        sprintf(tBuffer,"%02X\t",PeekByteZ80(address+a));
        strcat(tmp,tBuffer);
    }
    for (a=0;a<5-(numBytes+1);a++)
    {
        strcat(tmp,"\t");
    }
    sprintf(tBuffer,"%s\n",retVal);
    strcat(tmp,tBuffer);

    return numBytes+1;
}

int DisassembleZ80(unsigned int address,int registers)
{
    int res;
    char tmp[2048];
    res=DoDisassembleZ80(address,registers,tmp);
    CONSOLE_OUTPUT(tmp);
    return res;
}

int FETCH_DISASSEMBLEZ80(unsigned int address,char* tmp)
{
    return DoDisassembleZ80(address,0,tmp);
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
    if (!disable_exit)
        exit(-1);
    return 0;
}

uint32_t MSU_missing(uint32_t opcode)
{
    int a;
    CONSOLE_OUTPUT("IP : %08X\n",MSU_GETPHYSICAL_EIP());
    CONSOLE_OUTPUT("Next 7 Bytes : ");
    for (a=0;a<7;a++)
    {
        CONSOLE_OUTPUT("%02X ",PeekByte(MSU_GETPHYSICAL_EIP()+a));
    }
    CONSOLE_OUTPUT("\nNext 7-1 Bytes : ");
    for (a=0;a<7;a++)
    {
        CONSOLE_OUTPUT("%02X ",PeekByte(MSU_GETPHYSICAL_EIP()+a-1));
    }
    CONSOLE_OUTPUT("\nNext 7-2 Bytes : ");
    for (a=0;a<7;a++)
    {
        CONSOLE_OUTPUT("%02X ",PeekByte(MSU_GETPHYSICAL_EIP()+a-2));
    }
    CONSOLE_OUTPUT("\n");
    if (!disable_exit)
        exit(-1);
    return 0;
}

uint32_t MSU_unimplemented(uint32_t opcode)
{
    pause=1;
    return MSU_missing(opcode);
}

uint32_t Z80_missing(uint32_t opcode)
{
    CONSOLE_OUTPUT("REACHED Z80 missing");
    if (!disable_exit)
        exit(-1);
    return 0;
}

uint32_t DSP_missing(uint32_t opcode)
{
    CONSOLE_OUTPUT("REACHED DSP missing");
    if (!disable_exit)
        exit(-1);
    return 0;

}

uint32_t FL1DSP_missing(uint32_t opcode)
{
    CONSOLE_OUTPUT("REACHED FL1DSP missing");
    if (!disable_exit)
        exit(-1);
    return 0;
}

uint32_t FL1BLT_missing(uint32_t opcode)
{
    CONSOLE_OUTPUT("REACHED FL1Blitter missing");
    if (!disable_exit)
        exit(-1);
    return 0;

}

