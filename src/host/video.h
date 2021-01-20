/*

	Video wrapper

*/

#ifndef _VIDEO__H
#define _VIDEO__H

#define MAX_WINDOWS			(8)

extern unsigned char *videoMemory[MAX_WINDOWS];
extern int windowWidth[MAX_WINDOWS];
extern int windowHeight[MAX_WINDOWS];

void VideoInitialise();
int VideoCreate(int width, int height, int widthScale, int heightScale, int resizeable, int canCloseToQuit, const char* name, int fullscreen);
void VideoUpdate(int noBorders);
void VideoKill();
void VideoWait(float freq);

#endif//_VIDEO__H
