/*
 * Slipstream emulator
 *
 * Assumes PAL (was european after all) at present
 */

#define SLIPSTREAM_VERSION	"1.02"

#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "system.h"
#include "logfile.h"
#include "video.h"
#include "audio.h"
#include "keys.h"
#include "asic.h"
#include "dsp.h"
#include "memory.h"
#include "debugger.h"

int MAIN_WINDOW = -1;
int TERMINAL_WINDOW = -1;

ESlipstreamSystem curSystem=ESS_MSU;
int numClocks;
int masterClock=0;
int pause=0;
int single=0;
int framestep = 0;
int cyclestep = 0;
int noBorders = 0;
int curAddress=-1;
extern uint8_t DSP_CPU_HOLD;		// For now, DSP will hold CPU during relevant DMAs like this

int useJoystick=1;
int use6MhzP88Cpu=1;
int emulateDSP=1;
int useFullscreen = 0;

char lastRomLoaded[1024];

extern uint8_t FL1_VECTOR;

uint8_t dskABuffer[720*1024];
uint8_t dskBBuffer[720*1024];

const char* dskAPath=NULL;
const char* dskBPath=NULL;

#if MEMORY_MAPPED_DEBUGGER
void InitMemoryMappedDebugger();
int UpdateMemoryMappedDebuggerViews();
#endif

int HandleLoadSection(FILE* inFile)
{
    uint16_t	segment,offset;
    uint16_t	size;
    int		a=0;
    uint8_t		byte;

    if (2!=fread(&segment,1,2,inFile))
    {
        CONSOLE_OUTPUT("Failed to read segment for LoadSection\n");
        exit(1);
    }
    if (2!=fread(&offset,1,2,inFile))
    {
        CONSOLE_OUTPUT("Failed to read offset for LoadSection\n");
        exit(1);
    }
    fseek(inFile,2,SEEK_CUR);		// skip unknown
    if (2!=fread(&size,1,2,inFile))
    {
        CONSOLE_OUTPUT("Failed to read size for LoadSection\n");
        exit(1);
    }

    CONSOLE_OUTPUT("Found Section Load Memory : %04X:%04X   (%08X bytes)\n",segment,offset,size);

    for (a=0;a<size;a++)
    {
        if (1!=fread(&byte,1,1,inFile))
        {
            CONSOLE_OUTPUT("Failed to read data from LoadSection\n");
            exit(1);
        }
        SetByte(a+SEGTOPHYS(segment,offset),byte);
    }

    return 8+size;
}

int HandleExecuteSection(FILE* inFile)
{
    uint16_t	segment,offset;

    if (2!=fread(&segment,1,2,inFile))
    {
        CONSOLE_OUTPUT("Failed to read segment for ExecuteSection\n");
        exit(1);
    }
    if (2!=fread(&offset,1,2,inFile))
    {
        CONSOLE_OUTPUT("Failed to read offset for ExecuteSection\n");
        exit(1);
    }

    MSU_EIP=offset;
    MSU_CS=segment;
    MSU_SegBase[0] = segment * 16;
    CS=segment;
    IP=offset;
    Z80_PC=offset;
    Z80_SP = 0x3FC;	// PDS sets the stack here
    ASIC_WriteFL1(0, 0x10,0);
    ASIC_WriteFL1(1, 0x11,0);
    ASIC_WriteFL1(2, 0x12,0);
    ASIC_WriteFL1(3, 0x13,0);
    //ASIC_WriteFL1(7, 0xFF, 0);
    //ASIC_WriteFL1(8, 0x04, 0);
    //ASIC_WriteFL1(10, 0x40, 0);


    CONSOLE_OUTPUT("Found Section Execute : %04X:%04X\n",segment,offset);

    return 4;
}

int LoadMSU(const char* fname)					// Load an MSU file which will fill some memory regions and give us our booting point
{
    unsigned int expectedSize=0;
    strcpy(lastRomLoaded,fname);
    FILE* inFile = fopen(fname,"rb");
    if (inFile==NULL)
    {
        CONSOLE_OUTPUT("Failed to open %s\n",fname);
        return 1;
    }
    fseek(inFile,0,SEEK_END);
    expectedSize=ftell(inFile);
    fseek(inFile,0,SEEK_SET);

    while (expectedSize)
    {
        unsigned char sectionType;

        // Read a byte
        if (1!=fread(&sectionType,1,1,inFile))
        {
            CONSOLE_OUTPUT("Failed to read section header\n");
            return 1;
        }
        expectedSize--;

        switch (sectionType)
        {
            case 0xFF:						// Not original specification - added to indicate system type is P88
                CONSOLE_OUTPUT("Found Section Konix 8088\n");
                curSystem=ESS_P88;
                break;
            case 0xB3:
                // unknown
                break;
            case 0xF1:						// Not original specification - added to indicate system type is FL1
                CONSOLE_OUTPUT("Found Section Flare One\n");
                curSystem=ESS_FL1;
                // FL1 binaries are generally intended to be run from within an initialised system (they were downloaded via PDS)
                Z80_IM = 1;
                // PDS rom sets up a standard vector
                SetByte(0x40038, 0xF3);
                SetByte(0x40039, 0xF5);
                SetByte(0x4003A, 0xDB);
                SetByte(0x4003B, 0x07);
                SetByte(0x4003C, 0xF1);
                SetByte(0x4003D, 0xFB);
                SetByte(0x4003E, 0xED);
                SetByte(0x4003F, 0x4D);
                // TRANSFORMERS FIX
                SetByte(0x40001, 0xFF);	// Due to a mistake in the original sources, the transformers demo does not reserve space for a byte to indicate if the 
                                        //screen swapping is enabled, so it ends up uses the SWAPPING equate instead (which is 1). So the code polls address 1
                                        //to see if screen swapping is enabled, if PDS has been used to download the code, then the pds ROM occupies address 1
                                        //causing address 1 to return a non zero value. The FL1 image does not set a value to this address, and thus it reads
                                        //0, and thinks screen swapping is disabled, causing flickering on the roads
                break;
            case 0xC8:
                expectedSize-=HandleLoadSection(inFile);
                break;
            case 0xCA:
                expectedSize-=HandleExecuteSection(inFile);
                break;
            default:
                CONSOLE_OUTPUT("Unknown section type @%ld : %02X\n",ftell(inFile)-1,sectionType);
                return 1;
        }
    }

    fclose(inFile);

    //pause = 1;
    return 0;
}

int FL1LoadDisk(uint8_t* buffer,const char* fname)					// Load an MSU file which will fill some memory regions and give us our booting point
{
    unsigned int expectedSize=0;
    unsigned int address=0;
    FILE* inFile = fopen(fname,"rb");
    if (inFile == NULL)
    {
        CONSOLE_OUTPUT("Failed to read from %s\n", fname);
        return 1;
    }
    fseek(inFile,0,SEEK_END);
    expectedSize=ftell(inFile);
    fseek(inFile,0,SEEK_SET);

    while (expectedSize)
    {
        uint8_t data;

        // Read a byte
        if (1!=fread(&data,1,1,inFile))
        {
            CONSOLE_OUTPUT("Failed to read from %s\n",fname);
            return 1;
        }
        buffer[address]=data;
        address++;
        expectedSize--;
    }

    fclose(inFile);

    return 0;
}

int FL1SaveDisk(uint8_t* buffer,const char* fname, unsigned int expectedSize)
{
    unsigned int address=0;
    FILE* outFile = fopen(fname,"wb");
    if (outFile == NULL)
    {
        CONSOLE_OUTPUT("Failed to write to %s\n", fname);
        return 1;
    }

    while (expectedSize)
    {
        uint8_t data=buffer[address];

        // Read a byte
        if (1!=fwrite(&data,1,1,outFile))
        {
            CONSOLE_OUTPUT("Failed to write to %s\n",fname);
            return 1;
        }
        address++;
        expectedSize--;
    }

    fclose(outFile);

    return 0;
}


int LoadRom(const char* fname,uint32_t address)					// Load an MSU file which will fill some memory regions and give us our booting point
{
    unsigned int expectedSize=0;
    FILE* inFile = fopen(fname,"rb");
    if (inFile == NULL)
    {
        CONSOLE_OUTPUT("Failed to read from %s\n", fname);
        return 1;
    }
    fseek(inFile,0,SEEK_END);
    expectedSize=ftell(inFile);
    fseek(inFile,0,SEEK_SET);

    while (expectedSize)
    {
        uint8_t data;

        // Read a byte
        if (1!=fread(&data,1,1,inFile))
        {
            CONSOLE_OUTPUT("Failed to read from %s\n",fname);
            return 1;
        }
        ROM[address]=data;
        address++;
        expectedSize--;
    }

    fclose(inFile);

    return 0;
}

uint8_t DISK_IMAGE[5632*2*80];

int LoadDisk(const char* fname)				// Load P89 Disk -- Pure Sector Dump
{
    int ret=0;
    FILE* inFile = fopen(fname,"rb");
    if (inFile == NULL)
    {
        CONSOLE_OUTPUT("Failed to read from %s\n", fname);
        return 1;
    }

    if (5632*2*80!=fread(DISK_IMAGE,1,5632*2*80,inFile))
    {
        ret=1;
    }

    fclose(inFile);

    return ret;
}

int LoadBinaryMaxSizeOffset(const char* fname,uint32_t address,uint32_t len,uint32_t offs)					// Load an MSU file which will fill some memory regions and give us our booting point
{
    unsigned int expectedSize=0;
    FILE* inFile = fopen(fname,"rb");
    if (inFile == NULL)
    {
        CONSOLE_OUTPUT("Failed to read from %s\n", fname);
        return 1;
    }
    fseek(inFile,0,SEEK_END);
    expectedSize=ftell(inFile);
    fseek(inFile,offs,SEEK_SET);

    expectedSize-=offs;
    if (expectedSize>len)
    {
        expectedSize=len;
    }

    while (expectedSize)
    {
        uint8_t data;

        // Read a byte
        if (1!=fread(&data,1,1,inFile))
        {
            CONSOLE_OUTPUT("Failed to read from %s\n",fname);
            return 1;
        }
        SetByte(address,data);
        address++;
        expectedSize--;
    }

    fclose(inFile);

    return 0;
}

uint8_t GENLockTestingImage[256 * 256 * 3];

void GenerateGenLock()
{
    for (int y = 0; y < 256; y++)
    {
        for (int x = 0; x < 256; x++)
        {
            uint8_t s, p;
            s = rand();
            GENLockTestingImage[y * 256 * 3 + x * 3 + 0] = s;
            GENLockTestingImage[y * 256 * 3 + x * 3 + 1] = s;
            GENLockTestingImage[y * 256 * 3 + x * 3 + 2] = s;
        }
    }
}

int LoadRaw256x256x3TrueColorImage(const char* fname)
{
    FILE* inFile = fopen(fname, "rb");
    if (inFile == NULL)
    {
        CONSOLE_OUTPUT("Failed to read from %s\n", fname);
        return 1;
    }
    fseek(inFile, 0, SEEK_END);
    int expectedSize = ftell(inFile);
    fseek(inFile, 0, SEEK_SET);

    if (expectedSize!=256*256*3)
    {
        CONSOLE_OUTPUT("Expected file '%s' to be exactly 256x256x3 bytes large\n", fname);
        return 1;
    }

    if (expectedSize!=fread(GENLockTestingImage,1,expectedSize, inFile))
    {
        CONSOLE_OUTPUT("Failed to read from %s\n", fname);
        return 1;
    }
}

int LoadBinary(const char* fname,uint32_t address)					// Load an MSU file which will fill some memory regions and give us our booting point
{
    unsigned int expectedSize=0;
    FILE* inFile = fopen(fname,"rb");
    if (inFile == NULL)
    {
        CONSOLE_OUTPUT("Failed to read from %s\n", fname);
        return 1;
    }
    fseek(inFile,0,SEEK_END);
    expectedSize=ftell(inFile);
    fseek(inFile,0,SEEK_SET);

    while (expectedSize)
    {
        uint8_t data;

        // Read a byte
        if (1!=fread(&data,1,1,inFile))
        {
            CONSOLE_OUTPUT("Failed to read from %s\n",fname);
            return 1;
        }
        SetByte(address,data);
        address++;
        expectedSize--;
    }

    fclose(inFile);

    return 0;
}

void DSP_RESET(void);
void FL1DSP_RESET(void);
void STEP(void);
void RESET(void);
void MSU_STEP(void);
void MSU_RESET(void);
void Z80_RESET(void);
void Z80_STEP(void);

void CPU_RESET()
{
    RESET();
    MSU_RESET();
    Z80_RESET();
}

extern uint16_t numPadState;
extern uint16_t joyPadState;

void ForceUpperCase(char* tmp)
{
    while (*tmp!=0)
    {
        if (*tmp>='a' && *tmp<='z')
            *tmp-=0x20;
        tmp++;
    }
}

void DoCPU8086()
{
    STEP();
}

void DoCPU80386sx()
{
#if 0
    //	if ((MSU_GETPHYSICAL_EIP()&0xFFFFFF)==0xFF00ED)
    //	if ((MSU_GETPHYSICAL_EIP()&0xFFFFFF)==0xFF01ee)
    //	if ((MSU_GETPHYSICAL_EIP()&0xFFFFFF)==0xFF6ccd)
    /*	if ((MSU_GETPHYSICAL_EIP()&0xFFFFFF)==0xFF0781)
        {
        doDebug=1;
        debugWatchWrites=1;
        debugWatchReads=1;
        doShowPortStuff=1;
        doShowBlits = 1;
        }*/
    //	{
    //		FILE *dump = fopen("e:\\newwork\\msuUpper.bin", "wb");
    //		fwrite(ROM + 65536, 1, 65536, dump);
    //		fclose(dump);
    //	}

    if (doDebug)
    {
        Disassemble80386(MSU_GETPHYSICAL_EIP(),1);
        int vov = 0;
    }
#endif
    if (MSU_GETPHYSICAL_EIP()==0xE001C)
    {
        CONSOLE_OUTPUT("port_init called\n");
    }
    if (MSU_GETPHYSICAL_EIP()==0xE0010)
    {
        static uint32_t dirAddr;
        static int lastEntry;
        static uint32_t fileOffset;
        CONSOLE_OUTPUT("read_cd called\n");
        //	bh -minute
        //	bl -second
        //	al -block

        if ((MSU_EBX&0xFFFF)==0 && (MSU_EAX&0XFF)==0 && (MSU_ECX&0xFFFF)==0x1400)
        {
            CONSOLE_OUTPUT("Read Root Directory To ES:DI : %05X (Length in CX)\n",SEGTOPHYS(MSU_ES,MSU_EDI&0xFFFF));
            CONSOLE_OUTPUT("Minute %02X, Second %02X, Block %02X, Length %04X\n",(MSU_EBX>>8)&0xFF,MSU_EBX&255,MSU_EAX&255,MSU_ECX&0xFFFF);
            LoadBinaryMaxSizeOffset("ROBOCOD/CRUNCH/CD_DIR.BIN",SEGTOPHYS(MSU_ES,MSU_EDI&0xFFFF),MSU_ECX&0xFFFF,0);
            dirAddr=SEGTOPHYS(MSU_ES,MSU_EDI&0xFFFF);
            //							doDebug=1;
            //							debugWatchReads=1;
            //							debugWatchWrites=1;
            //doDebug=1;
        }
        else
        {
            char tmp[200];
            int a;
            CONSOLE_OUTPUT("Read Something from Directory To ES:DI : %05X\n",SEGTOPHYS(MSU_ES,MSU_EDI&0xFFFF));
            CONSOLE_OUTPUT("Minute %02X, Second %02X, Block %02X, Length %04X\n",(MSU_EBX>>8)&0xFF,MSU_EBX&255,MSU_EAX&255,MSU_ECX&0xFFFF);

            // Find filename from directory table -- note if name not in table, its a continuation from the last load offset by 25 blocks -- HACK!
            for (a=0;a<211;a++)
            {
                if (GetByte(dirAddr+a*20+13)==((MSU_EBX>>8)&255))
                {
                    if (GetByte(dirAddr+a*20+14)==(MSU_EBX&255))
                    {
                        if (GetByte(dirAddr+a*20+15)==(MSU_EAX&255))
                        {
                            //CONSOLE_OUTPUT("Filename : %s\n",&RAM[dirAddr+a*20]);
                            lastEntry=a;
                            fileOffset=0;
                            break;
                        }
                    }
                }
            }
            if (a==211)
            {
                fileOffset+=25*2048;
                CONSOLE_OUTPUT("CONTINUATION : %s + %08X\n",&RAM[dirAddr+lastEntry*20],fileOffset);
            }
            sprintf(tmp,"ROBOCOD/CRUNCH/%s",&RAM[dirAddr+lastEntry*20]);
            ForceUpperCase(tmp);
            CONSOLE_OUTPUT("Loading : %s\n",tmp);
            LoadBinaryMaxSizeOffset(tmp,SEGTOPHYS(MSU_ES,MSU_EDI&0xFFFF),MSU_ECX&0xFFFF,fileOffset);

            if (strcmp("GENERAL.ITM",(const char*)&RAM[dirAddr+lastEntry*20])==0)
            {
                //doDebug=1;
            }

        }
    }
    if (MSU_GETPHYSICAL_EIP()==0xE0024)
    {
        char tmp[200];
        int a;
        int fileOffset;
        CONSOLE_OUTPUT("Read Something from Directory To ES:DI : %05X\n",SEGTOPHYS(MSU_ES,MSU_EDI&0xFFFF));
        CONSOLE_OUTPUT("Minute %02X, Second %02X, Block %02X, Length %04X\n",(MSU_EBX>>8)&0xFF,MSU_EBX&255,MSU_EAX&255,MSU_ECX&0xFFFF);

        int minutes = (MSU_EBX>>8)&0xFF;
        int seconds = MSU_EBX&255;
        int block = MSU_EAX&255;
        int length = MSU_ECX&0xFFFF;

        fileOffset = (seconds * 75 + block)*2048;

        // Find filename from directory table -- note if name not in table, its a continuation from the last load offset by 25 blocks -- HACK!
        sprintf(tmp,"KONIX-BUILDS/MINUTE.0%d",minutes);
//        ForceUpperCase(tmp);
        CONSOLE_OUTPUT("Loading : %s\n",tmp);
        LoadBinaryMaxSizeOffset(tmp,SEGTOPHYS(MSU_ES,MSU_EDI&0xFFFF),length,fileOffset);


    }
    if (MSU_GETPHYSICAL_EIP()==0xE0004)
    {
        MSU_EAX = (MSU_EAX & 0xFFFF0000 ) | ((joyPadState^0xFFFF)&0xFFFF);// ^ 0xFFFF;
                                                                          //CONSOLE_OUTPUT("read_kmssjoy called\n");
    }
    if (MSU_GETPHYSICAL_EIP()==0xE000C)
    {
        MSU_EAX = (MSU_EAX & 0xFFFF0000) | (numPadState/* ^ 0xFFFF*/);
        //CONSOLE_OUTPUT("read_keypad called\n");
    }
    if (MSU_GETPHYSICAL_EIP()==0x0D000)
    {
        CONSOLE_OUTPUT("neildos called\n");
    }

    MSU_STEP();
}


void DoCPUZ80()
{
    Z80_STEP();
}
uint32_t FL1BLT_Step(uint8_t hold);

int CPU_STEP(int doDebug)
{
    if (!DSP_CPU_HOLD)
    {
        switch (curSystem)
        {
            case ESS_MSU:
                {
                    int cycles = 0;
                    for (int a=0;a<2;a++)
                    {
                        DoCPU80386sx();
                        cycles+= MSU_CYCLES;
                    }
                    return MSU_CYCLES>>1;			// MSU docs talk about 8086...  
                }
            case ESS_CP1:
                DoCPU80386sx();
                return MSU_CYCLES;			// Assuming clock speed same as hardware chips
            case ESS_P88:
            case ESS_P89:
                DoCPU8086();
                if (CYCLES<=0)
                {
                    return 1;
                }
                if (use6MhzP88Cpu)
                    return CYCLES*2;		// 6Mhz
                else
                    return CYCLES;
            case ESS_FL1:
                if (FL1BLT_Step(0) == 0)
                {
                    DoCPUZ80();
                    return Z80_CYCLES;
                }
                break;
        }
    }

    return 1;		// CPU HELD, MASTER CLOCKS continue
}


void Usage()
{
    CONSOLE_OUTPUT("slipstream [opts] program.msu/program.p88/program.fl1\n");
    CONSOLE_OUTPUT("-F Fullscreen\n");
    CONSOLE_OUTPUT("-O Disable Borders\n");
    CONSOLE_OUTPUT("-W Fullscreen Windowed\n");
    CONSOLE_OUTPUT("-f [disable P88 frequency divider]\n");
    CONSOLE_OUTPUT("-b address file.bin [Load binary to ram]\n");
    CONSOLE_OUTPUT("-n [disable DSP emulation]\n");
    CONSOLE_OUTPUT("-K boot production konix bios\n");
    CONSOLE_OUTPUT("-M load MSU bios\n");
    CONSOLE_OUTPUT("-C load CARD1 bios (developer bios)\n");
    CONSOLE_OUTPUT("-D [floppy] load [floppy] to drive (needs -K to boot)\n");
    CONSOLE_OUTPUT("-z filename [load a file as FL1 binary]\n");
    CONSOLE_OUTPUT("-j [disable joystick]\n");
    CONSOLE_OUTPUT("-1 [disk] boot in flare 1 bios mode and mount disk to floppy drive\n");
    CONSOLE_OUTPUT("-2 [disk]  mount disk to floppy drive b\n");
    CONSOLE_OUTPUT("-V [256x256x3] truecolour image raw format for use with flare one genlocking\n");
    CONSOLE_OUTPUT("-S [symbols] load a symbol file\n");
    CONSOLE_OUTPUT("\nFor example to load the PROPLAY.MSU :\n");
    CONSOLE_OUTPUT("slipstream -b 90000 RCBONUS.MOD PROPLAY.MSU\n");
    exit(1);
}

#if MEMORY_MAPPED_DEBUGGER
void LoadSymbolFile(char* filename);
#endif

void ParseCommandLine(int argc,char** argv)
{
    int a;

    if (argc<2)
    {
        Usage();
        return;
    }

    for (a=1;a<argc;a++)
    {
        if (argv[a][0]=='-')
        {
            if (strcmp(argv[a],"-j")==0)
            {
                useJoystick=0;
                continue;
            }
            if (strcmp(argv[a],"-f")==0)
            {
                use6MhzP88Cpu=0;
                continue;
            }
            if (strcmp(argv[a],"-n")==0)
            {
                emulateDSP=0;
                continue;
            }
            if (strcmp(argv[a],"-K")==0)
            {
                curSystem=ESS_P89;
                LoadRom("roms/konixBios.bin",0);
                ROM[0xB1] = 0xEB;		// Disable floppy security check
                continue;
            }
            if (strcmp(argv[a], "-S") == 0)
            {
                if ((a+1)<argc)
                {
#if MEMORY_MAPPED_DEBUGGER
                    LoadSymbolFile(argv[a+1]);
#endif
                }
                else
                {
                    Usage();
                    return;
                }
                a+=1;
                continue;
            }
            if (strcmp(argv[a], "-V") == 0)
            {
                if ((a+1)<argc)
                {
                    LoadRaw256x256x3TrueColorImage(argv[a+1]);
                }
                else
                {
                    Usage();
                    return;
                }
                a+=1;
                continue;
            }
            if (strcmp(argv[a], "-F") == 0)
            {
                useFullscreen = 1;
                continue;
            }
            if (strcmp(argv[a], "-W") == 0)
            {
                useFullscreen = 2;
                continue;
            }
            if (strcmp(argv[a], "-O") == 0)
            {
                noBorders = 1;
                continue;
            }
            if (strcmp(argv[a],"-C")==0)
            {
                curSystem=ESS_CP1;
                LoadRom("roms/card1.bin",0);
                //LoadRom("roms/james5.bin",0);
                //LoadRom("roms/easy.bin",0);
                //LoadRom("roms/devsys5.bin",0);
                continue;
            }
            if (strcmp(argv[a],"-M")==0)
            {
                curSystem=ESS_CP1;
                LoadRom("roms/MSUBios.bin",0);
                continue;
            }
            if (strcmp(argv[a],"-b")==0)
            {
                if ((a+2)<argc)
                {
                    // Grab address (hex)
                    uint32_t address;
                    sscanf(argv[a+1],"%x",&address);
                    CONSOLE_OUTPUT("Loading Binary %s @ %05X\n",argv[a+2],address);
                    LoadBinary(argv[a+2],address);
                }
                else
                {
                    Usage();
                    return;
                }
                a+=2;
                continue;
            }
            if (strcmp(argv[a],"-z")==0)
            {
                if ((a+1)<argc)
                {
                    LoadBinary(argv[a+1],0x40000+1024);
                    Z80_PC=1024;
                    curSystem=ESS_FL1;

                    return;
                }
                else
                {
                    Usage();
                    return;
                }
                a+=1;
                continue;
            }
            if (strcmp(argv[a],"-D")==0)
            {
                if ((a+1)<argc)
                {
                    LoadDisk(argv[a+1]);
                }
                else
                {
                    Usage();
                    return;
                }
                a+=1;
                continue;
            }
            if (strcmp(argv[a],"-1")==0)
            {
                if ((a+1)<argc)
                {
                    dskAPath=argv[a+1];
                    FL1LoadDisk(dskABuffer,dskAPath);
                    LoadBinary("roms/FL1_ROM0_0000.rom",0);
                    LoadBinary("roms/FL1_ROM1_CC00.rom",4*16384);
                    curSystem=ESS_FL1;
                }
                else
                {
                    Usage();
                    return;
                }
                a+=1;
                continue;
            }
            if (strcmp(argv[a],"-2")==0)
            {
                if ((a+1)<argc)
                {
                    dskBPath=argv[a+1];
                    FL1LoadDisk(dskBBuffer,dskBPath);
                }
                else
                {
                    Usage();
                    return;
                }
                a+=1;
                continue;
            }
        }
        else
        {
            LoadMSU(argv[a]);
        }
    }
}

void ResetHardware()
{
    CPU_RESET();
    DSP_RESET();
    FL1DSP_RESET();

    MEMORY_INIT();
    ASIC_INIT();

    PALETTE_INIT();
    DSP_RAM_INIT();
}

void DebugDrawOffScreen();
int GetILength80386(unsigned int address, int cpu);

int dbg_event = 0;
int bpaddress = -1;
int DBG_Cpu_Clocks = 0;

#if ENABLE_PDS
void PDS_Main();
void PDS_Start();
int PDS_Tick();
#endif

int main(int argc,char**argv)
{
#if ENABLE_PDS
    PDS_Main();
#endif
    ResetHardware();

    GenerateGenLock();

    ParseCommandLine(argc,argv);

    // PDS HACKING
#if 0

    curSystem = ESS_P88;

    SetByte(0xFFFF0, 0xEB);
    SetByte(0xFFFF1, 0xFE);
#endif
    // PDS HACKING

    VECTORS_INIT();				// Workarounds for problematic roms that rely on a bios (we don't have) to have initialised memory state

#if MEMORY_MAPPED_DEBUGGER
    InitMemoryMappedDebugger();
#endif

    {
        VideoInitialise();
        MAIN_WINDOW = VideoCreate(WIDTH, HEIGHT, 1, 2, 1, 1, "Slipstream - V" SLIPSTREAM_VERSION, useFullscreen);
        videoMemory[MAIN_WINDOW] = (unsigned char*)malloc(WIDTH * HEIGHT * sizeof(unsigned int));
        memset(videoMemory[MAIN_WINDOW], 0, WIDTH * HEIGHT * sizeof(unsigned int));
#if TERMINAL
        TERMINAL_WINDOW = VideoCreate(640, 480, 1, 1, 0, 0, "Terminal Emulation");
        videoMemory[TERMINAL_WINDOW] = (unsigned char*)malloc(640 * 480 * sizeof(unsigned int));
        memset(videoMemory[TERMINAL_WINDOW], 0, 640 * 480 * sizeof(unsigned int));
#endif

        KeysIntialise(useJoystick);
        AudioInitialise(WIDTH*HEIGHT);
    }

#if ENABLE_PDS
    PDS_Start();
#endif
    //////////////////

    //	doDebugTrapWriteAt=0x088DAA;
    //	debugWatchWrites=1;
    //	doDebug=1;
    //	pause=1;
    /*	extern int doShowDMA;
        doDebug=1;*/
    /*		debugWatchWrites=1;
            debugWatchReads=1;
            doShowPortStuff=1;
            doDSPDisassemble=1;
            doDebug=1;*/
    /*		doShowDMA=1;
            doShowBlits=1;*/

    dbg_event= 0;
    //	bpaddress = 0xFE0DD5;
    //	bpaddress = 0xFE2223;		// Flash/PC communications (could be interesting)
    //	bpaddress = 0x725563;		// FRONT.REX goes wrong
    //	bpaddress = 0x726A9B;		// CART.REX goes wrong
    //	bpaddress = 0xFFE350;		// JAMES5.BIN 

#if MEMORY_MAPPED_DEBUGGER
    pause=1;
#endif

#if 0 
    dbg_event = 1;
    bpaddress = 0x88;
    pause = 1;
#endif

    dbg_event = 1;
    //	bpaddress = 0x80000;// 0x800b9;
    //pause = 0;

    /*
       SetByte(0xE8, 0xAD);
       SetByte(0xE9, 0xDE);
       SetByte(0xEA, 0xDE);
       SetByte(0xEB, 0xC0);
       */

    extern int g_Quit;
    while (g_Quit)
    {
        uint32_t ttBltDebug;
        if (!pause)
        {
#if ENABLE_PDS
            PDS_Tick();
#endif
#if MEMORY_MAPPED_DEBUGGER
            if (DBG_Cpu_Clocks == 0)			// Hack to allow cycle stepping
            {
                numClocks = DBG_Cpu_Clocks;
                DBG_Cpu_Clocks = CPU_STEP(doDebug);
            }
            else
                DBG_Cpu_Clocks--;

            if (single)
                numClocks += 1;
            else
                numClocks = 1;
#else
            numClocks = CPU_STEP(doDebug);
#endif

#if MEMORY_MAPPED_DEBUGGER
            if (dbg_event)
            {
                if (cyclestep)
                {
                    pause = 1;
                    cyclestep = 0;
                }
                switch (curSystem)
                {
                    case ESS_P88:
                    case ESS_P89:
                        if (SEGTOPHYS(CS,IP) == bpaddress)
                        {
                            pause = 1;
                            dbg_event = 0;
                            bpaddress=-1;
                        }
                        if (SEGTOPHYS(CS,IP) != curAddress && curAddress!=-1)
                        {
                            pause = 1;
                            dbg_event = 0;
                            curAddress=-1;
                        }
                        break;
                    case ESS_CP1:
                    case ESS_MSU:
                        if (MSU_GETPHYSICAL_EIP() == bpaddress)
                        {
                            pause = 1;
                            dbg_event = 0;
                            bpaddress=-1;
                        }
                        if (MSU_GETPHYSICAL_EIP() != curAddress && curAddress!=-1)
                        {
                            pause = 1;
                            dbg_event = 0;
                            curAddress=-1;
                        }
                        break;
                    case ESS_FL1:
                        if (getZ80LinearAddress() == bpaddress)
                        {
                            pause = 1;
                            dbg_event = 0;
                            bpaddress=-1;
                        }
                        if (getZ80LinearAddress() != curAddress && curAddress!=-1)
                        {
                            pause = 1;
                            dbg_event = 0;
                            curAddress=-1;
                        }
                        break;
                }
            }
#endif
            switch (curSystem)
            {
                case ESS_CP1:
                    TickAsicCP1(numClocks);
                    break;
                case ESS_MSU:
                    TickAsicMSU(numClocks);
                    break;
                case ESS_P88:
                    TickAsicP88(numClocks);
                    break;
                case ESS_P89:
                    TickAsicP89(numClocks);
                    break;
                case ESS_FL1:
                    TickAsicFL1(numClocks);
                    break;
            }
            masterClock+=numClocks;

            AudioUpdate(numClocks);
        }

        if (masterClock>=WIDTH*HEIGHT || pause)
        {

#if ENABLE_DEBUG
            if (curSystem==ESS_P89)
            {
                void ShowEddyDebug();		// Show P89 Floppy Controller Information
                ShowEddyDebug();
            }
            if (curSystem == ESS_FL1)
            {
                void ShowPotsDebug();
                ShowPotsDebug();
            }
#endif
            if (masterClock>=WIDTH*HEIGHT)
            {
                masterClock-=WIDTH*HEIGHT;
                if (framestep)
                {
                    framestep = 0;
                    pause = 1;
                }
            }
#if MEMORY_MAPPED_DEBUGGER
            switch (UpdateMemoryMappedDebuggerViews(pause || (masterClock<=0)))
            {
                case 0:
                    pause = 1;
                    break;
                case 1:
                    pause = 0;
                    break;
                case 2:
                    pause = 0;
                    switch (curSystem)
                    {
                        case ESS_FL1:
                            curAddress= getZ80LinearAddress();
                            break;
                        case ESS_MSU:
                        case ESS_CP1:
                            curAddress= MSU_GETPHYSICAL_EIP();
                            break;
                        case ESS_P88:
                        case ESS_P89:
                            curAddress= SEGTOPHYS(CS,IP);
                            break;

                    }
                    dbg_event = 1;
                    break;
                case 3:
                    // Get Length of current instruction and set a global bp on it (not strictly a step over, ideally we would decode the next address properly)
                    dbg_event = 1;
                    pause = 0;
                    switch (curSystem)
                    {
                        case ESS_FL1:
                            bpaddress = getZ80LinearAddress() + GetILength80386(getZ80LinearAddress(), 0);
                            break;
                        case ESS_MSU:
                        case ESS_CP1:
                            bpaddress = MSU_GETPHYSICAL_EIP() + GetILength80386(MSU_GETPHYSICAL_EIP(), 1);
                            break;
                        case ESS_P88:
                        case ESS_P89:
                            bpaddress = SEGTOPHYS(CS,IP+GetILength80386(SEGTOPHYS(CS,IP), 1));
                            break;

                    }
                    break;
                case 4:
                    pause = 0;
                    framestep = 1;
                    break;
                case 5:
                    dbg_event = 1;
                    pause = 0;
                    cyclestep = 1;
                    break;
                default:
                    break;
            }
#endif

            {
                //SetByte(0x010022,5);		// Infinite lives LN2
                TickKeyboard();
                if (JoystickPresent())
                {
                    JoystickPoll();
                }
                VideoUpdate(noBorders);

                /*	if (CheckKey(GLFW_KEY_F12))
                    {
                    ClearKey(GLFW_KEY_F12);

                    ResetHardware();
                    LoadDisk(dskABuffer,"SYSTEM.DSK");
                //LoadDisk(dskBBuffer,"BLANK.DSK");

                LoadBinary("FL1_ROM0_0000.rom",0);
                LoadBinary("FL1_ROM1_CC00.rom",4*16384);
                curSystem=ESS_FL1;
                VECTORS_INIT();
                }*/
                if (CheckKey(GLFW_KEY_F11))
                {
                    ClearKey(GLFW_KEY_F11);
                    break;
                }
                if (pause)//CheckKey(GLFW_KEY_F11))
                {
                    DebugDrawOffScreen();
                }
                /*				if (KeyDown(GLFW_KEY_F4))
                                {
                                doDebug = 1;
                                }*/
                if (KeyDown(GLFW_KEY_HOME))
                {
                    pause=0;
                }
                if (CheckKey(GLFW_KEY_END))
                {
                    pause=0;
                    ClearKey(GLFW_KEY_END);
                }


                /*		-- Find infinite lives poke LN2
                        if (CheckKey(GLFW_KEY_B))
                        {
                        static int find=5;
                        static int memlist[32768],memlist2[32768];
                        static int* mem1=memlist;
                        static int* mem2=memlist2;
                        static int* memt;
                        static int memcnt=0,memcnt2=0;
                        static int* mcnt=&memcnt;
                        static int* mcnt2=&memcnt2;
                        int a;
                        printf("---find---%d,%d\n",memcnt,memcnt2);
                        for (a=0;a<0xC0000;a++)
                        {
                        if (GetByte(a)==find)
                        {
                        if (find==5)
                        {
                        mem1[(*mcnt)++]=a;
                        printf("Address : %06X\n",a);
                        }
                        else
                        {
                        int b;
                        for (b=0;b<*mcnt;b++)
                        {
                        if (mem1[b]==a)
                        {
                        mem2[(*mcnt2)++]=a;
                        printf("Address : %06X\n",a);
                        }
                        }
                        }
                        }
                        }
                        if (find!=5)
                        {
                        memt=mem1;
                        mem1=mem2;
                        mem2=memt;
                        memt=mcnt;
                        mcnt=mcnt2;
                        mcnt2=memt;
                 *mcnt2=0;
                 }
                 find--;
                 ClearKey(GLFW_KEY_B);
                 }*/

#if !ENABLE_DEBUG
                VideoWait(curSystem==ESS_MSU? 0.02f : 0.02f);
#endif
            }
        }
    }

    /*
       FILE* ramDump = fopen("C:\\work\\F1.RAM", "wb");
       for (int a = 0; a < RAM_SIZE; a++)
       {
       fwrite(&RAM[a], 1, 1, ramDump);
       }
       fclose(ramDump);
       */

    KeysKill();
    AudioKill();
    VideoKill();

    if (dskAPath!=NULL)
    {
        FL1SaveDisk(dskABuffer, dskAPath, 720*1024);
    }
    if (dskBPath!=NULL)
    {
        FL1SaveDisk(dskBBuffer, dskBPath, 720*1024);
    }

    return 0;
}


