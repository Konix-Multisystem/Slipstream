#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "system.h"
#include "logfile.h"
#include "font.h"
#include "host/video.h"

void DrawChar(unsigned char* buffer,unsigned int width,unsigned int x,unsigned int y, char c,unsigned char r,unsigned char g,unsigned char b)
{
    unsigned int xx,yy;
    unsigned char *fontChar=&FontData[c*6*8];

    x*=8;
    y*=8;

    for (yy=0;yy<8;yy++)
    {
        for (xx=0;xx<6;xx++)
        {
            buffer[(x+xx+1 + (y+yy)*width)*4+0]=(*fontChar)*b;
            buffer[(x+xx+1 + (y+yy)*width)*4+1]=(*fontChar)*g;
            buffer[(x+xx+1 + (y+yy)*width)*4+2]=(*fontChar)*r;
            fontChar++;
        }
    }
}

void PrintAt(unsigned char* buffer,unsigned int width,unsigned char r,unsigned char g,unsigned char b,unsigned int x,unsigned int y,const char *msg,...)
{
    static char tStringBuffer[32768];
    char *pMsg=tStringBuffer;
    va_list args;

    va_start (args, msg);
    vsprintf (tStringBuffer,msg, args);
    va_end (args);

    while (*pMsg)
    {
        DrawChar(buffer,width,x,y,*pMsg,r,g,b);
        x++;
        pMsg++;
    }
}

void TERMINAL_OUTPUT(uint8_t byte)
{
#if TERMINAL
    static int cPosX=0,cPosY=0;

    switch (byte)
    {
        case 0:
            return;
        case 13:
            cPosX=0;
            break;
        case 10:
            cPosY++;
            break;
        default:
            DrawChar(videoMemory[TERMINAL_WINDOW],windowWidth[TERMINAL_WINDOW],cPosX,cPosY,byte,255,255,255);
            cPosX++;
            if (cPosX*8 >= windowWidth[TERMINAL_WINDOW])
            {
                cPosX=0;
                cPosY++;
            }
            break;
    }

    if (cPosY*8>=windowHeight[TERMINAL_WINDOW])
    {
        // Scroll window 
        memmove(videoMemory[TERMINAL_WINDOW],videoMemory[TERMINAL_WINDOW]+windowWidth[TERMINAL_WINDOW]*8*4,4*windowWidth[TERMINAL_WINDOW]*(windowHeight[TERMINAL_WINDOW]-8));
        memset(videoMemory[TERMINAL_WINDOW]+4*windowWidth[TERMINAL_WINDOW]*(windowHeight[TERMINAL_WINDOW]-8),0,8*4*windowWidth[TERMINAL_WINDOW]);
        cPosY--;
    }
#endif
}

uint8_t keyBuffer[256];
int keyPos=0;
int lastKey=0;
int lastAction=0;

uint8_t GetTermKey()
{
    uint8_t k=0;
    if (keyPos!=0)
    {
        k=keyBuffer[0];
        memmove(&keyBuffer[0],&keyBuffer[1],255);
        keyPos--;
    }
    return k;
}

uint8_t HasTermKey()
{
    return keyPos!=0;
}

void termHandler( GLFWwindow* window, int key, int scan, int action, int mod )
{
    if (action==GLFW_PRESS && GLFW_KEY_ENTER)
    {
        /*		if (key == GLFW_KEY_BACKSPACE)
                {
                keyBuffer[keyPos++]=8;
                }*/
        if (key == GLFW_KEY_ENTER)
        {
            keyBuffer[keyPos++]=13;
        }
        /*		if (key>=' ' && key<=127)
                {
                keyBuffer[keyPos++]=key;
                }*/
    }
}
void termCharHandler( GLFWwindow* window, unsigned int key)
{
    printf("P: %c(%d)\n",key,key);
    /*		if (key == GLFW_KEY_BACKSPACE)
            {
            keyBuffer[keyPos++]=8;
            }*/
    if (key == GLFW_KEY_ENTER)
    {
        keyBuffer[keyPos++]=13;
    }
    if (key>=' ' && key<=127)
    {
        keyBuffer[keyPos++]=key;
    }
}


