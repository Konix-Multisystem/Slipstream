/*
 * Slipstream emulator
 *
 * Assumes PAL (was european after all) at present
 */

#define SLIPSTREAM_VERSION	"0.2 Preview"

#include <GL/glfw3.h>

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

#include <winsock2.h>

#include <pthread.h>
#include <unistd.h>

volatile int serverAlive=1;
int list_s;
int conn_s;
struct sockaddr_in addr;
struct hostent *host;


ssize_t ReadCommand(int sockd, void *vptr, size_t maxlen) 
{
    ssize_t n, rc;
    char    c, *buffer;

    buffer = vptr;

    for ( n = 1; n < maxlen; n++ ) {
	
	if ( (rc = recv(sockd, &c, 1,0 )) == 1 ) 
	{
	    *buffer++ = c;
	    if ( c == '\n' )
		break;
	}
	else if ( rc == 0 ) 
	{
	    if ( n == 1 )
		return 0;
	    else
		break;
	}
	else 
	{
	    if ( errno == EINTR )
		continue;
	    return -1;
	}
    }

    *buffer = 0;
    return n;
}

ssize_t WriteCommand(int sockd, const void *vptr, size_t n) 
{
    size_t      nleft;
    ssize_t     nwritten;
    const char *buffer;

    buffer = vptr;
    nleft  = n;

    while ( nleft > 0 ) 
    {
	if ( (nwritten = send(sockd, buffer, nleft,0)) <= 0 ) 
	{
	    if ( errno == EINTR )
		nwritten = 0;
	    else
		return -1;
	}
	nleft  -= nwritten;
	buffer += nwritten;
    }

    return n;
}

uint32_t GetZ80LinearAddress()
{
	uint16_t addr=Z80_PC;
	switch (addr&0xC000)
	{
		case 0x0000:
			return ASIC_BANK0+(addr&0x3FFF);

		case 0x4000:
			return ASIC_BANK1+(addr&0x3FFF);

		case 0x8000:
			return ASIC_BANK2+(addr&0x3FFF);
		
		case 0xC000:
			break;
	}
	return ASIC_BANK3+(addr&0x3FFF);
}

void SendStatus(int sock)
{
	char tmp[1024];
	char tmp2[1024];

	switch (curSystem)
	{
		case ESS_MSU:
			sprintf(tmp,"Status:MSU%05X\n",SEGTOPHYS(CS,IP)&0xFFFFF);
			FETCH_REGISTERS8086(tmp2);
			break;
		case ESS_P88:
			sprintf(tmp,"Status:P88%05X\n",SEGTOPHYS(CS,IP)&0xFFFFF);
			FETCH_REGISTERS8086(tmp2);
			break;
		case ESS_FL1:
			sprintf(tmp,"Status:FL1%05X\n",GetZ80LinearAddress()&0xFFFFF);
			FETCH_REGISTERSZ80(tmp2);
			break;
	}
	strcat(tmp,"REG\n");
	strcat(tmp,tmp2);
	strcat(tmp,"REGEND\n");
							
	WriteCommand(conn_s,tmp,strlen(tmp));
}

void* threadFunc(void* arg)
{
	if ((list_s=socket(AF_INET,SOCK_STREAM,0))>=0)
	{
		
		memset(&addr, 0, sizeof(addr));
		host = gethostbyname("localhost");
		if (host)
		{
			memcpy(&addr.sin_addr, host->h_addr_list[0], sizeof(host->h_addr_list[0]));
			addr.sin_family = AF_INET;
			addr.sin_port   = htons(45454);
			if (bind(list_s, (struct sockaddr *)&addr, sizeof(addr))!=-1)
			{
				if (listen(list_s, 1024)!=-1)
				{
					if ( (conn_s = accept(list_s, NULL, NULL) ) >= 0 )
					{
						printf("Connection\n");
						while (serverAlive)
						{
							char tmpCom[1025]="";
							ReadCommand(conn_s,tmpCom,1024);

							if (strcmp(tmpCom,"Status\n")==0)
							{
								printf("Status request\n");
								SendStatus(conn_s);
								ReadCommand(conn_s,tmpCom,1024);
							}
							if (strcmp(tmpCon,"Step\n")==0)
							{

							}

						}
						//usleep(33);
						closesocket(conn_s);
					}
					usleep(10);
				}
			}
		}
		closesocket(list_s);
	}
	else
	{
		printf("Failed to create socket!");
	}
	return NULL;
}

pthread_t pth;

void CreateRemoteServer()
{
    	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	pthread_create(&pth,NULL,threadFunc,"RemoteSocket");
}

void CloseRemoteServer()
{
	serverAlive=0;
	pthread_join(pth,NULL);

	WSACleanup();
}

ESlipstreamSystem curSystem=ESS_MSU;
int masterClock=0;

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

	CS=segment;
	IP=offset;
	Z80_PC=offset;
	ASIC_BANK0=(segment<<4);
	ASIC_BANK1=(segment<<4)+16384;
	ASIC_BANK2=(segment<<4)+32768;
	ASIC_BANK3=0x4C000;				// PAGE 19 - 

	CONSOLE_OUTPUT("Found Section Execute : %04X:%04X\n",segment,offset);

	return 4;
}

int LoadMSU(const char* fname)					// Load an MSU file which will fill some memory regions and give us our booting point
{
	unsigned int expectedSize=0;
	FILE* inFile = fopen(fname,"rb");
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
			case 0xFF:							// Not original specification - added to indicate system type is P88
				CONSOLE_OUTPUT("Found Section Konix 8088\n");
				curSystem=ESS_P88;
				break;
			case 0xF1:
				CONSOLE_OUTPUT("Found Section Flare One\n");
				curSystem=ESS_FL1;
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

	return 0;
}

int LoadBinary(const char* fname,uint32_t address)					// Load an MSU file which will fill some memory regions and give us our booting point
{
	unsigned int expectedSize=0;
	FILE* inFile = fopen(fname,"rb");
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
void STEP(void);
void RESET(void);
void Z80_RESET(void);
void Z80_STEP(void);

extern uint8_t DSP_CPU_HOLD;		// For now, DSP will hold CPU during relevant DMAs like this

int use6MhzP88Cpu=1;
int emulateDSP=1;

void CPU_RESET()
{
	RESET();
	Z80_RESET();
}

void DoCPU8086()
{
#if ENABLE_DEBUG
		if (SEGTOPHYS(CS,IP)==0)//0x80120)//(0x80ECF))
		{
			doDebug=1;
			debugWatchWrites=1;
			debugWatchReads=1;
			doShowBlits=1;
//			numClocks=1;
		}
#endif
#if ENABLE_DEBUG
	if (doDebug)
	{
		Disassemble8086(SEGTOPHYS(CS,IP),1);
	}
#endif
	STEP();
}

void DoCPUZ80()
{
#if ENABLE_DEBUG
	if (Z80_PC==0)//0x1205)
	{
		doDebug=1;
		//debugWatchWrites=1;
		//debugWatchReads=1;
		doShowPortStuff=1;
		//doShowBlits=1;
		//			numClocks=1;
	}
#endif
#if ENABLE_DEBUG
	if (doDebug)
	{
		DisassembleZ80(Z80_PC,1);
	}
#endif
/*	if (Z80_PC==0x488)
		{
		uint8_t c1,c2,c3;

		c1=GetByte(1024+0xA5);
		c2=GetByte(1024+0xA6);
		c3=GetByte(1024+0xA7);

		printf("Bytes : %02X(%c) %02X(%c) %02X\n",c1,c1,c2,c2,c3);
		}
*/
	Z80_STEP();
}

int CPU_STEP(int doDebug)
{
	if (!DSP_CPU_HOLD)
	{
		switch (curSystem)
		{
			case ESS_MSU:
				DoCPU8086();
				return CYCLES;			// Assuming clock speed same as hardware chips
			case ESS_P88:
				DoCPU8086();
				if (use6MhzP88Cpu)
					return CYCLES*2;		// 6Mhz
				else
					return CYCLES;
			case ESS_FL1:
				DoCPUZ80();
				return Z80_CYCLES;
		}
	}
		
	return 1;		// CPU HELD, MASTER CLOCKS continue
}
	

void Usage()
{
	CONSOLE_OUTPUT("slipstream [opts] program.msu/program.p88\n");
	CONSOLE_OUTPUT("-f [disable P88 frequency divider]\n");
	CONSOLE_OUTPUT("-b address file.bin [Load binary to ram]\n");
	CONSOLE_OUTPUT("-n [disable DSP emulation]\n");
	CONSOLE_OUTPUT("-z filename [load a file as FL1 binary]\n");
	CONSOLE_OUTPUT("\nFor example to load the PROPLAY.MSU :\n");
	CONSOLE_OUTPUT("slipstream -b 90000 RCBONUS.MOD PROPLAY.MSU\n");
	exit(1);
}

void ParseCommandLine(int argc,char** argv)
{
	int a;

	if (argc<2)
	{
		return Usage();
	}

	for (a=1;a<argc;a++)
	{
		if (argv[a][0]=='-')
		{
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
					return Usage();
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
					return Usage();
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

int main(int argc,char**argv)
{
	int numClocks;

	CreateRemoteServer();

	CPU_RESET();
	DSP_RESET();

	PALETTE_INIT();
	DSP_RAM_INIT();

	ParseCommandLine(argc,argv);

	VECTORS_INIT();

	VideoInitialise(WIDTH,HEIGHT,"Slipstream - V" SLIPSTREAM_VERSION);
	KeysIntialise();
	AudioInitialise(WIDTH*HEIGHT);

	//////////////////
	
//	doDebugTrapWriteAt=0x088DAA;
//	debugWatchWrites=1;
//	doDebug=1;

	int pause=1;
	int single=0;

	while (1==1)
	{
		if (!pause)
		{
			numClocks=CPU_STEP(doDebug);
			switch (curSystem)
			{
				case ESS_MSU:
					TickAsicMSU(numClocks);
					break;
				case ESS_P88:
					TickAsicP88(numClocks);
					break;
				case ESS_FL1:
					TickAsicFL1(numClocks);
					break;
			}
			masterClock+=numClocks;

			AudioUpdate(numClocks);
			if (single)
			{
				pause=1;
				single=0;
			}
		}
		if (masterClock>=WIDTH*HEIGHT || pause)
		{	
			if (masterClock>=WIDTH*HEIGHT)
			{
				masterClock-=WIDTH*HEIGHT;
			}

			TickKeyboard();
			if (JoystickPresent())
			{
				JoystickPoll();
			}
			VideoUpdate();

			if (CheckKey(GLFW_KEY_ESC))
			{
				break;
			}
#if !ENABLE_DEBUG
			VideoWait();
#endif
		}
	}

	KeysKill();
	AudioKill();
	VideoKill();

	CloseRemoteServer();

	return 0;
}


