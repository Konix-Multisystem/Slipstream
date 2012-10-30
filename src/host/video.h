/*

	Video wrapper

*/

#ifndef _VIDEO__H
#define _VIDEO__H

#define MAX_WINDOWS		(8)
#define MAIN_WINDOW		0

extern unsigned char *videoMemory[MAX_WINDOWS];

void VideoInitialise(int width,int height,const char* name);
void VideoUpdate();
void VideoKill();
void VideoWait();

#endif//_VIDEO__H
