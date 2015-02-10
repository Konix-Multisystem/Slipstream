/*
 * Slipstream emulator
 *
 * Assumes PAL (was european after all) at present
 */

#define SLIPSTREAM_VERSION	"0.4 - Flare 1 Built In"

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

ESlipstreamSystem curSystem=ESS_FL1;
int numClocks;
int masterClock=0;
int pause=0;
int single=0;

extern uint8_t DSP_CPU_HOLD;		// For now, DSP will hold CPU during relevant DMAs like this

int useJoystick=1;
int use6MhzP88Cpu=1;
int emulateDSP=1;
int useRemoteDebugger=0;

char lastRomLoaded[1024];

uint8_t dskABuffer[720*1024];
uint8_t dskBBuffer[720*1024];

#if ENABLE_REMOTE_DEBUG

#include <winsock2.h>

#include <pthread.h>
#include <unistd.h>

extern char remoteDebuggerLog[1024*1024];

pthread_t pth;
pthread_mutex_t commandSyncMutex;

typedef enum 
{
	ERC_None,
	ERC_Step,
	ERC_Run,
	ERC_Pause,
	ERC_Reset,
	ERC_Load
}ERemoteCommand;

volatile ERemoteCommand remoteCommand=ERC_None;
char param[1024];


volatile int serverAlive;
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
	    {
	    	printf("Whoops\n");
		return -1;
		}
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
	uint32_t address=0;
	int a;
	char tmp[32768];
	char tmp2[32768];

	switch (curSystem)
	{
		case ESS_MSU:
			address=SEGTOPHYS(CS,IP)&0xFFFFF;
			sprintf(tmp,"%s:MSU%05X\n",pause?"Paused":"Runnin",address);
			FETCH_REGISTERS8086(tmp2);
			break;
		case ESS_P88:
			address=SEGTOPHYS(CS,IP)&0xFFFFF;
			sprintf(tmp,"%s:P88%05X\n",pause?"Paused":"Runnin",address);
			FETCH_REGISTERS8086(tmp2);
			break;
		case ESS_P89:
			address=SEGTOPHYS(CS,IP)&0xFFFFF;
			sprintf(tmp,"%s:P88%05X\n",pause?"Paused":"Runnin",address);
			FETCH_REGISTERS8086(tmp2);
			break;
		case ESS_FL1:
			address=Z80_PC;//GetZ80LinearAddress()&0xFFFFF;		// disassembly expects address in chip space not linear space
			sprintf(tmp,"%s:FL1%05X\n",pause?"Paused":"Runnin",address);
			FETCH_REGISTERSZ80(tmp2);
			break;
	}
	strcat(tmp,"REG\n");
	strcat(tmp,tmp2);
	sprintf(tmp2,"\nBANK 0\t%08X\nBANK 1\t%08X\nBANK 2\t%08X\nBANK 3\t%08X\n",ASIC_BANK0,ASIC_BANK1,ASIC_BANK2,ASIC_BANK3);
	strcat(tmp,tmp2);
	strcat(tmp,"REGEND\n");
	strcat(tmp,"DIS\n");
	for (a=0;a<10;a++)
	{
		switch (curSystem)
		{
			case ESS_MSU:
			case ESS_P88:
			case ESS_P89:
				address+=FETCH_DISASSEMBLE8086(address,tmp2);
				break;
			case ESS_FL1:
				address+=FETCH_DISASSEMBLEZ80(address,tmp2);
				break;
		}
		strcat(tmp,tmp2);
	}
	strcat(tmp,"\n");
	// Add memory dump facility (currently done based on BC,DE,HL,SP)
	sprintf(tmp2,"[BC %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_BC,
		Z80_GetByte(Z80_BC+0),
		Z80_GetByte(Z80_BC+1),
		Z80_GetByte(Z80_BC+2),
		Z80_GetByte(Z80_BC+3),
		Z80_GetByte(Z80_BC+4),
		Z80_GetByte(Z80_BC+5),
		Z80_GetByte(Z80_BC+6),
		Z80_GetByte(Z80_BC+7),
		Z80_GetByte(Z80_BC+8),
		Z80_GetByte(Z80_BC+9),
		Z80_GetByte(Z80_BC+10),
		Z80_GetByte(Z80_BC+11),
		Z80_GetByte(Z80_BC+12),
		Z80_GetByte(Z80_BC+13),
		Z80_GetByte(Z80_BC+14),
		Z80_GetByte(Z80_BC+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[BC %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_BC+16,
		Z80_GetByte(Z80_BC+16+0),
		Z80_GetByte(Z80_BC+16+1),
		Z80_GetByte(Z80_BC+16+2),
		Z80_GetByte(Z80_BC+16+3),
		Z80_GetByte(Z80_BC+16+4),
		Z80_GetByte(Z80_BC+16+5),
		Z80_GetByte(Z80_BC+16+6),
		Z80_GetByte(Z80_BC+16+7),
		Z80_GetByte(Z80_BC+16+8),
		Z80_GetByte(Z80_BC+16+9),
		Z80_GetByte(Z80_BC+16+10),
		Z80_GetByte(Z80_BC+16+11),
		Z80_GetByte(Z80_BC+16+12),
		Z80_GetByte(Z80_BC+16+13),
		Z80_GetByte(Z80_BC+16+14),
		Z80_GetByte(Z80_BC+16+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[DE %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_DE,
		Z80_GetByte(Z80_DE+0),
		Z80_GetByte(Z80_DE+1),
		Z80_GetByte(Z80_DE+2),
		Z80_GetByte(Z80_DE+3),
		Z80_GetByte(Z80_DE+4),
		Z80_GetByte(Z80_DE+5),
		Z80_GetByte(Z80_DE+6),
		Z80_GetByte(Z80_DE+7),
		Z80_GetByte(Z80_DE+8),
		Z80_GetByte(Z80_DE+9),
		Z80_GetByte(Z80_DE+10),
		Z80_GetByte(Z80_DE+11),
		Z80_GetByte(Z80_DE+12),
		Z80_GetByte(Z80_DE+13),
		Z80_GetByte(Z80_DE+14),
		Z80_GetByte(Z80_DE+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[DE %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_DE+16,
		Z80_GetByte(Z80_DE+16+0),
		Z80_GetByte(Z80_DE+16+1),
		Z80_GetByte(Z80_DE+16+2),
		Z80_GetByte(Z80_DE+16+3),
		Z80_GetByte(Z80_DE+16+4),
		Z80_GetByte(Z80_DE+16+5),
		Z80_GetByte(Z80_DE+16+6),
		Z80_GetByte(Z80_DE+16+7),
		Z80_GetByte(Z80_DE+16+8),
		Z80_GetByte(Z80_DE+16+9),
		Z80_GetByte(Z80_DE+16+10),
		Z80_GetByte(Z80_DE+16+11),
		Z80_GetByte(Z80_DE+16+12),
		Z80_GetByte(Z80_DE+16+13),
		Z80_GetByte(Z80_DE+16+14),
		Z80_GetByte(Z80_DE+16+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[HL %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_HL,
		Z80_GetByte(Z80_HL+0),
		Z80_GetByte(Z80_HL+1),
		Z80_GetByte(Z80_HL+2),
		Z80_GetByte(Z80_HL+3),
		Z80_GetByte(Z80_HL+4),
		Z80_GetByte(Z80_HL+5),
		Z80_GetByte(Z80_HL+6),
		Z80_GetByte(Z80_HL+7),
		Z80_GetByte(Z80_HL+8),
		Z80_GetByte(Z80_HL+9),
		Z80_GetByte(Z80_HL+10),
		Z80_GetByte(Z80_HL+11),
		Z80_GetByte(Z80_HL+12),
		Z80_GetByte(Z80_HL+13),
		Z80_GetByte(Z80_HL+14),
		Z80_GetByte(Z80_HL+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[HL %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_HL+16,
		Z80_GetByte(Z80_HL+16+0),
		Z80_GetByte(Z80_HL+16+1),
		Z80_GetByte(Z80_HL+16+2),
		Z80_GetByte(Z80_HL+16+3),
		Z80_GetByte(Z80_HL+16+4),
		Z80_GetByte(Z80_HL+16+5),
		Z80_GetByte(Z80_HL+16+6),
		Z80_GetByte(Z80_HL+16+7),
		Z80_GetByte(Z80_HL+16+8),
		Z80_GetByte(Z80_HL+16+9),
		Z80_GetByte(Z80_HL+16+10),
		Z80_GetByte(Z80_HL+16+11),
		Z80_GetByte(Z80_HL+16+12),
		Z80_GetByte(Z80_HL+16+13),
		Z80_GetByte(Z80_HL+16+14),
		Z80_GetByte(Z80_HL+16+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[SP %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_SP,
		Z80_GetByte(Z80_SP+0),
		Z80_GetByte(Z80_SP+1),
		Z80_GetByte(Z80_SP+2),
		Z80_GetByte(Z80_SP+3),
		Z80_GetByte(Z80_SP+4),
		Z80_GetByte(Z80_SP+5),
		Z80_GetByte(Z80_SP+6),
		Z80_GetByte(Z80_SP+7),
		Z80_GetByte(Z80_SP+8),
		Z80_GetByte(Z80_SP+9),
		Z80_GetByte(Z80_SP+10),
		Z80_GetByte(Z80_SP+11),
		Z80_GetByte(Z80_SP+12),
		Z80_GetByte(Z80_SP+13),
		Z80_GetByte(Z80_SP+14),
		Z80_GetByte(Z80_SP+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[SP %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_SP+16,
		Z80_GetByte(Z80_SP+16+0),
		Z80_GetByte(Z80_SP+16+1),
		Z80_GetByte(Z80_SP+16+2),
		Z80_GetByte(Z80_SP+16+3),
		Z80_GetByte(Z80_SP+16+4),
		Z80_GetByte(Z80_SP+16+5),
		Z80_GetByte(Z80_SP+16+6),
		Z80_GetByte(Z80_SP+16+7),
		Z80_GetByte(Z80_SP+16+8),
		Z80_GetByte(Z80_SP+16+9),
		Z80_GetByte(Z80_SP+16+10),
		Z80_GetByte(Z80_SP+16+11),
		Z80_GetByte(Z80_SP+16+12),
		Z80_GetByte(Z80_SP+16+13),
		Z80_GetByte(Z80_SP+16+14),
		Z80_GetByte(Z80_SP+16+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[IX %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_IX,
		Z80_GetByte(Z80_IX+0),
		Z80_GetByte(Z80_IX+1),
		Z80_GetByte(Z80_IX+2),
		Z80_GetByte(Z80_IX+3),
		Z80_GetByte(Z80_IX+4),
		Z80_GetByte(Z80_IX+5),
		Z80_GetByte(Z80_IX+6),
		Z80_GetByte(Z80_IX+7),
		Z80_GetByte(Z80_IX+8),
		Z80_GetByte(Z80_IX+9),
		Z80_GetByte(Z80_IX+10),
		Z80_GetByte(Z80_IX+11),
		Z80_GetByte(Z80_IX+12),
		Z80_GetByte(Z80_IX+13),
		Z80_GetByte(Z80_IX+14),
		Z80_GetByte(Z80_IX+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[IY %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",Z80_IY,
		Z80_GetByte(Z80_IY+0),
		Z80_GetByte(Z80_IY+1),
		Z80_GetByte(Z80_IY+2),
		Z80_GetByte(Z80_IY+3),
		Z80_GetByte(Z80_IY+4),
		Z80_GetByte(Z80_IY+5),
		Z80_GetByte(Z80_IY+6),
		Z80_GetByte(Z80_IY+7),
		Z80_GetByte(Z80_IY+8),
		Z80_GetByte(Z80_IY+9),
		Z80_GetByte(Z80_IY+10),
		Z80_GetByte(Z80_IY+11),
		Z80_GetByte(Z80_IY+12),
		Z80_GetByte(Z80_IY+13),
		Z80_GetByte(Z80_IY+14),
		Z80_GetByte(Z80_IY+15));
	strcat(tmp,tmp2);
	sprintf(tmp2,"[E278 %04X]\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",0xe278,
		Z80_GetByte(0xE278+0),
		Z80_GetByte(0xE278+1),
		Z80_GetByte(0xE278+2),
		Z80_GetByte(0xE278+3),
		Z80_GetByte(0xE278+4),
		Z80_GetByte(0xE278+5),
		Z80_GetByte(0xE278+6),
		Z80_GetByte(0xE278+7),
		Z80_GetByte(0xE278+8),
		Z80_GetByte(0xE278+9),
		Z80_GetByte(0xE278+10),
		Z80_GetByte(0xE278+11),
		Z80_GetByte(0xE278+12),
		Z80_GetByte(0xE278+13),
		Z80_GetByte(0xE278+14),
		Z80_GetByte(0xE278+15));
	strcat(tmp,tmp2);

	strcat(tmp,"DISEND\n");
	if (remoteDebuggerLog[0]!=0)
	{
		strcat(tmp,"LOG\n");
	}
							
	sprintf(tmp2,"%08X",strlen(tmp));
	WriteCommand(conn_s,tmp2,8);
	WriteCommand(conn_s,tmp,strlen(tmp));

	if (remoteDebuggerLog[0]!=0)
	{
		sprintf(tmp2,"%08X",strlen(remoteDebuggerLog));
		WriteCommand(conn_s,tmp2,8);
		WriteCommand(conn_s,remoteDebuggerLog,strlen(remoteDebuggerLog));
		remoteDebuggerLog[0]=0;
	}

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
					while (serverAlive)
					{
						if ( (conn_s = accept(list_s, NULL, NULL) ) >= 0 )
						{
							printf("Connection\n");
							while (serverAlive)
							{
								if (remoteCommand==ERC_None)
								{
									char tmpCom[1025]="";
									if (ReadCommand(conn_s,tmpCom,1024)==0)
										break;

									pthread_mutex_lock(&commandSyncMutex);

									if (strcmp(tmpCom,"Status\n")==0)
									{
										SendStatus(conn_s);
									}
									if (strcmp(tmpCom,"Step\n")==0)
									{
										remoteCommand=ERC_Step;
									}
									if (strcmp(tmpCom,"Run\n")==0)
									{
										remoteCommand=ERC_Run;
									}
									if (strcmp(tmpCom,"Pause\n")==0)
									{
										remoteCommand=ERC_Pause;
									}
									if (strncmp(tmpCom,"Load",4)==0)
									{
										strcpy(param,tmpCom+5);
										param[strlen(param)-1]=0;
										remoteCommand=ERC_Load;
									}
									if (strcmp(tmpCom,"Reset\n")==0)
									{
										remoteCommand=ERC_Reset;
									}

									pthread_mutex_unlock(&commandSyncMutex);

								}
								else
								{
									usleep(100);
								}

							}
							printf("dropped\n");
							//usleep(33);
							closesocket(conn_s);
						}
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


void CreateRemoteServer()
{
    	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	serverAlive=1;
	pthread_mutex_init(&commandSyncMutex,NULL);
	pthread_create(&pth,NULL,threadFunc,"RemoteSocket");
}

void CloseRemoteServer()
{
	serverAlive=0;
	pthread_join(pth,NULL);
	pthread_mutex_destroy(&commandSyncMutex);
	WSACleanup();
}
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

int FL1LoadDisk(uint8_t* buffer,const char* fname)					// Load an MSU file which will fill some memory regions and give us our booting point
{
	unsigned int expectedSize=0;
	unsigned int address=0;
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
		buffer[address]=data;
		address++;
		expectedSize--;
	}

	fclose(inFile);

	return 0;
}

int LoadRom(const char* fname,uint32_t address)					// Load an MSU file which will fill some memory regions and give us our booting point
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

	if (5632*2*80!=fread(DISK_IMAGE,1,5632*2*80,inFile))
	{
		ret=1;
	}

	fclose(inFile);

	return ret;
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

void DoCPU8086()
{
#if ENABLE_DEBUG
		if (SEGTOPHYS(CS,IP)==0)//0x80120)//(0x80ECF))
		{
//			doDebug=1;
//			debugWatchWrites=1;
//			debugWatchReads=1;
//			doShowBlits=1;
//			numClocks=1;
		}
#endif
#if ENABLE_DEBUG
	if (doDebug)
	{
		Disassemble80386(SEGTOPHYS(CS,IP),1);
	}
#endif
	STEP();
}

void DoCPU80386sx()
{
#if ENABLE_DEBUG
/*	if ((MSU_GETPHYSICAL_EIP()&0xFFFFF)==0xF0100)
	{
		doDebug=1;
		debugWatchWrites=1;
		debugWatchReads=1;
		doShowPortStuff=1;
	}*/
	if (doDebug)
	{
		Disassemble80386(MSU_GETPHYSICAL_EIP(),1);
	}
#endif
	MSU_STEP();
}


void DoCPUZ80()
{
#if ENABLE_REMOTE_DEBUG

//	uint32_t LinearAddress = GetZ80LinearAddress()&0xFFFFF;


//	if (((GetZ80LinearAddress()&0xFFFFF)==(/*0x8E*//*0x24b8*/0x94) || (GetZ80LinearAddress()&0xFFFFF)==(0x38)) && !(Z80_HALTED&1))

	if ((Z80_PC==0x3/*0xe7e5*/ /*|| Z80_PC==0x35af*/) && !(Z80_HALTED&1))
	{
		GoDebug();
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

/*	if (Z80_GetByte(Z80_PC)==0xDB)
	{
		pause=1;
	}*/
}

int CPU_STEP(int doDebug)
{
	if (!DSP_CPU_HOLD)
	{
		switch (curSystem)
		{
			case ESS_MSU:
				DoCPU80386sx();
				return MSU_CYCLES;			// Assuming clock speed same as hardware chips
			case ESS_P88:
			case ESS_P89:
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
	CONSOLE_OUTPUT("slipstream [opts] program.msu/program.p88/program.fl1\n");
	CONSOLE_OUTPUT("-r [Startup in remote debugger mode]\n");
	CONSOLE_OUTPUT("-f [disable P88 frequency divider]\n");
	CONSOLE_OUTPUT("-b address file.bin [Load binary to ram]\n");
	CONSOLE_OUTPUT("-n [disable DSP emulation]\n");
	CONSOLE_OUTPUT("-K boot production konix bios\n");
	CONSOLE_OUTPUT("-M load MSU bios\n");
	CONSOLE_OUTPUT("-D [floppy] load [floppy]\n");
	CONSOLE_OUTPUT("-z filename [load a file as FL1 binary]\n");
	CONSOLE_OUTPUT("-j [disable joystick]\n");
	CONSOLE_OUTPUT("-1 [disk] boot in flare 1 bios mode and mount disk to floppy drive\n");
	CONSOLE_OUTPUT("\nFor example to load the PROPLAY.MSU :\n");
	CONSOLE_OUTPUT("slipstream -b 90000 RCBONUS.MOD PROPLAY.MSU\n");
	exit(1);
}

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
			if (strcmp(argv[a],"-r")==0)
			{
				useRemoteDebugger=1;
				continue;
			}
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
			if (strcmp(argv[a],"-H")==0)
			{
				curSystem=ESS_P88;
				LoadBinary("heads/slipstrm.hi",0x8B000);
				LoadBinary("heads/data.9",0x90000);
				LoadBinary("heads/data.9e",0x9E000);
				LoadBinary("heads/data.a",0xA0000);
				LoadBinary("heads/data.aa",0xAA000);
				LoadBinary("heads/laugh.ss",0xB0000);
				LoadBinary("heads/head.com",0x80000);

				CS=0x8000;
				IP=0;
				return;
			}
			if (strcmp(argv[a],"-K")==0)
			{
				curSystem=ESS_P89;
				LoadRom("roms/konixBios.bin",0);
				continue;
			}
			if (strcmp(argv[a],"-M")==0)
			{
				curSystem=ESS_MSU;
				LoadRom("roms/MSUBios.bin",0);
				return;
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
			if (strcmp(argv[a],"-1")==0)
			{
				FL1LoadDisk(dskABuffer,"SYSTEM.DSK");
				FL1LoadDisk(dskBBuffer,"BLANK.DSK");

				LoadBinary("FL1_ROM0_0000.rom",0);
				LoadBinary("FL1_ROM1_CC00.rom",4*16384);
				curSystem=ESS_FL1;
				return;
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
	numClocks=0;
	memset(videoMemory[MAIN_WINDOW],0,WIDTH*HEIGHT*sizeof(unsigned int));
	memset(videoMemory[TERMINAL_WINDOW],0,640*480*sizeof(unsigned int));

	CPU_RESET();
	DSP_RESET();
	FL1DSP_RESET();

	MEMORY_INIT();
	ASIC_INIT();

	PALETTE_INIT();
	DSP_RAM_INIT();
}

void DebugDrawOffScreen();
uint32_t FL1BLT_Step(uint8_t hold);

int main(int argc,char**argv)
{
	videoMemory[MAIN_WINDOW] = (unsigned char*)malloc(WIDTH*HEIGHT*sizeof(unsigned int));
	videoMemory[TERMINAL_WINDOW] = (unsigned char*)malloc(640*480*sizeof(unsigned int));

	ResetHardware();
		
#if ENABLE_REMOTE_DEBUG
	CreateRemoteServer();
#endif

	ParseCommandLine(argc,argv);
	

// Poke Rom To  Skip floppy security  test results of no floppy
	if (curSystem==ESS_P89)
	{
/*	ROM[0xAE]=0x90;
	ROM[0xAF]=0x90;
	ROM[0xB0]=0x90;*/
	ROM[0xB1]=0xEB;
/*	ROM[0xB2]=0x90;*/
	}

	VECTORS_INIT();				// Workarounds for problematic roms that rely on a bios (we don't have) to have initialised memory state

#if ENABLE_REMOTE_DEBUG
	if (useRemoteDebugger)
	{
//		printf("Running in headless mode - CTRL-C to quit\n");
		pause=1;
	}
#endif

	{
		VideoInitialise();
		VideoCreate(WIDTH,HEIGHT,"Slipstream - V" SLIPSTREAM_VERSION);
#if TERMINAL
		VideoCreate(640,480,"Terminal Emulation");
#endif
		KeysIntialise(useJoystick);
		AudioInitialise(WIDTH*HEIGHT);
	}
	//////////////////
	
//	doDebugTrapWriteAt=0x088DAA;
//	debugWatchWrites=1;
//	doDebug=1;
	//	pause=1;
	/*	extern int doShowDMA;
		doDebug=1;*/
//		debugWatchWrites=1;
//		debugWatchReads=1;
//		doShowPortStuff=1;
//		doDSPDisassemble=1;
//		doDebug=1;
/*		doShowDMA=1;
		doShowBlits=1;*/
	while (1==1)
	{
		uint32_t ttBltDebug;
		if (!pause)
		{
			numClocks+=CPU_STEP(doDebug);
/*			ttBltDebug=FL1BLT_Step(0);
			if (ttBltDebug==0)
			{
				numClocks+=CPU_STEP(doDebug);
			}
			else
			{
				if (ttBltDebug==2)
				{
//					pause=1;
				}
				numClocks+=1;
			}*/
			switch (curSystem)
			{
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
			numClocks=0;
			if (single)
			{
				pause=1;
				single=0;
			}
		}
		if (masterClock>=WIDTH*HEIGHT || pause)
		{
			if (curSystem==ESS_P89)
			{
				void ShowEddyDebug();		// Show P89 Floppy Controller Information
				ShowEddyDebug();
			}

			if (masterClock>=WIDTH*HEIGHT)
			{
				masterClock-=WIDTH*HEIGHT;
			}
#if ENABLE_REMOTE_DEBUG
			if (useRemoteDebugger)
			{
				if (remoteCommand!=ERC_None)
				{
					char tParam[1024];
					ERemoteCommand remCom;

					pthread_mutex_lock(&commandSyncMutex);

					remCom=remoteCommand;
					strcpy(tParam,param);
					remoteCommand=ERC_None;

					pthread_mutex_unlock(&commandSyncMutex);

					// Check for a command

					switch (remCom)
					{
					default:
						break;
					case ERC_Run:
						pause=0;
						break;
					case ERC_Pause:
						pause=1;
						break;
					case ERC_Step:
						pause=0;
						single=1;
						break;
					case ERC_Reset:
						ResetHardware();
						LoadMSU(lastRomLoaded);
						VECTORS_INIT();
						pause=1;
						single=0;
						break;
					case ERC_Load:
						ResetHardware();
						printf("Loading : %s\n",tParam);
						LoadMSU(tParam);
						printf("Loaded : %s\n",tParam);
						VECTORS_INIT();
						pause=1;
						single=0;
						break;
					}
				}
			}
#endif
			{
				//SetByte(0x010022,5);		// Infinite lives LN2
				TickKeyboard();
				if (JoystickPresent())
				{
					JoystickPoll();
				}
				VideoUpdate();

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
				if (CheckKey(GLFW_KEY_ESCAPE))
				{
					ClearKey(GLFW_KEY_ESCAPE);
					break;
				}
				if (pause)//CheckKey(GLFW_KEY_F11))
				{
					DebugDrawOffScreen();
				}
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
				VideoWait();
#endif
			}
		}
	}

#if ENABLE_REMOTE_DEBUG
	CloseRemoteServer();
#endif

	KeysKill();
	AudioKill();
	VideoKill();


	return 0;
}


