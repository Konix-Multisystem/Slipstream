/*

 Wrapper for printf functionality + output to file

*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "logfile.h"

#include <pthread.h>

#define LOGFILENAME	"OUT.LOG"

FILE* logFile=NULL;

#if ENABLE_REMOTE_DEBUG
extern int useRemoteDebugger;

char remoteDebuggerLog[1024*1024]={0};

extern pthread_mutex_t commandSyncMutex;
#endif

void CONSOLE_OUTPUT(char* fmt,...)
{
	va_list args;

#if ENABLE_REMOTE_DEBUG
	if (useRemoteDebugger)
	{
		char tmpForLog[32768];
		
		va_start(args,fmt);

		vsprintf(tmpForLog,fmt,args);
		
		va_end(args);

		pthread_mutex_lock(&commandSyncMutex);

		if (strlen(tmpForLog)<(sizeof(remoteDebuggerLog)-strlen(remoteDebuggerLog)))
		{
			strcat(remoteDebuggerLog,tmpForLog);
		}

		pthread_mutex_unlock(&commandSyncMutex);
	}
	else
#endif
	{
		if (logFile==NULL)
		{
			logFile=fopen(LOGFILENAME,"w");
		}

		va_start(args,fmt);
		vprintf(fmt,args);
		vfprintf(logFile,fmt,args);
		va_end(args);
	}
}

