/*

	Video Wrapper

*/

#include <GLFW/glfw3.h>
#include <GL/glext.h>

#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "logfile.h"
#include "audio.h"
#include "video.h"

unsigned char *videoMemory[MAX_WINDOWS];
GLFWwindow *windows[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];
int windowWidth[MAX_WINDOWS];
int windowHeight[MAX_WINDOWS];
const char* fpsWindowName;
double	atStart,now,remain;
float initialRatio;

int maxWindow=-1;

void ShowScreen(int windowNum,int w,int h)
{
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);
	
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f,1.0f);

	glTexCoord2f(0.0f, h);
	glVertex2f(-1.0f, -1.0f);

	glTexCoord2f(w, h);
	glVertex2f(1.0f, -1.0f);

	glTexCoord2f(w, 0.0f);
	glVertex2f(1.0f, 1.0f);

	glEnd();
	
	glFlush();
}

void setupGL(int windowNum,int w, int h) 
{
	videoTexture[windowNum] = windowNum+1;

	//Tell OpenGL how to convert from coordinates to pixel values
	glViewport(0, 0, w, h);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); 

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);

	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_REPEAT);
//	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA, w,
			h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);

	glDisable(GL_DEPTH_TEST);
}

void VideoSizeHandler(GLFWwindow *window,int xs,int ys)
{
	int offsx=0,offsy=0;
	glfwMakeContextCurrent(window);
	if (xs>ys)
	{
		offsx=xs;
		xs=ys*initialRatio;
		offsx=(offsx-xs)/2;
	}
	else
	{
		offsy=ys;
		ys=xs*initialRatio;
		offsy=(offsy-ys)/2;
	}
	glViewport(offsx, offsy, xs, ys);
}

void VideoCloseHandler(GLFWwindow *window)
{
	exit(0);
}

void VideoInitialise()
{
	/// Initialize GLFW 
	glfwInit(); 

	maxWindow=0;
	
	glfwSwapInterval(0);			// Disable VSYNC
	
	atStart=glfwGetTime();
}

void VideoCreate(int width,int height,const char* name)
{
	windowWidth[maxWindow]=width;
	windowHeight[maxWindow]=height;

	// Open invaders OpenGL window 
	if (maxWindow==0)
	{
		fpsWindowName=name;
	}
	if( !(windows[maxWindow]=glfwCreateWindow( width, maxWindow==0?height*2:height, name,NULL,NULL)) ) 
	{ 
/*		int glError = glfwGetError();
		CONSOLE_OUTPUT("GLFW Create Window - %s - %d",glfwErrorString(glError),glError);*/
		glfwTerminate(); 
		exit(1);
	} 

	glfwSetWindowPos(windows[maxWindow],300,300);
	
	glfwMakeContextCurrent(windows[maxWindow]);
	setupGL(maxWindow,width,height);

	if (maxWindow==0)
	{
		glfwSetWindowSizeCallback(windows[maxWindow],VideoSizeHandler);
		glfwSetWindowCloseCallback(windows[maxWindow],VideoCloseHandler);
		glViewport(0, 0, width, height*2);
	}
	else
	{
		glViewport(0, 0, width, height);
	}

	initialRatio=width/(height*2.0f);

	maxWindow++;
}

void VideoKill()
{
}

void VideoUpdate()
{
	int a;
	for (a=0;a<maxWindow;a++)
	{
		glfwMakeContextCurrent(windows[a]);
		ShowScreen(a,windowWidth[a],windowHeight[a]);
		glfwSwapBuffers(windows[a]);
	}			
	glfwPollEvents();
}

float totalTime=0.f;
int totalCnt=0;

void VideoWait()
{
	static char fpsBuffer[128];

	now=glfwGetTime();

	remain = now-atStart;

	totalTime+=remain;
	totalCnt+=1;

	if (totalCnt==50)
	{
		sprintf(fpsBuffer,"%s - Average FPS %f\n",fpsWindowName,1.f/(remain));
		glfwSetWindowTitle(windows[MAIN_WINDOW],fpsBuffer);
		totalTime=0.f;
		totalCnt=0;
	}

	while ((remain<0.02f))
	{
		now=glfwGetTime();

		AudioUpdate(0);

		remain = now-atStart;
	}

	atStart=glfwGetTime();
}

