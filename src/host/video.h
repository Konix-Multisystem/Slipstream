/*

	Video wrapper

*/

#ifndef _VIDEO__H
#define _VIDEO__H

#define MAX_WINDOWS		(8)
#define MAIN_WINDOW		0
#define TERMINAL_WINDOW		1

extern unsigned char *videoMemory[MAX_WINDOWS];
extern int windowWidth[MAX_WINDOWS];
extern int windowHeight[MAX_WINDOWS];

void VideoInitialise();
void VideoCreate(int width,int height,const char* name,int fullscreen);
void VideoUpdate();
void VideoKill();
void VideoWait();

#endif//_VIDEO__H
