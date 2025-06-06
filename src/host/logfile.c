/*

 Wrapper for printf functionality + output to file

*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "logfile.h"

#define LOGFILENAME	"OUT.LOG"

FILE* logFile=NULL;

void CONSOLE_OUTPUT(const char* fmt,...)
{
	va_list args;

	if (logFile == NULL)
	{
		logFile = fopen(LOGFILENAME, "w");
	}

	va_start(args, fmt);
	vfprintf(logFile, fmt, args);
	va_end(args);

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	fflush(logFile);
}

