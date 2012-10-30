/*

	Video Wrapper

*/

#include <GL/glfw3.h>
#include <GL/glext.h>

#include <malloc.h>
#include <string.h>

#include "video.h"

unsigned char *videoMemory[MAX_WINDOWS];
GLFWwindow windows[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];
int windowWidth[MAX_WINDOWS];
int windowHeight[MAX_WINDOWS];

double	atStart,now,remain;

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
	videoMemory[windowNum] = (unsigned char*)malloc(w*h*sizeof(unsigned int));
	memset(videoMemory[windowNum],0,w*h*sizeof(unsigned int));

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

void VideoInitialise(int width,int height,const char* name)
{
	/// Initialize GLFW 
	glfwInit(); 

	windowWidth[MAIN_WINDOW]=width;
	windowHeight[MAIN_WINDOW]=height;

	// Open invaders OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwCreateWindow( width, height, GLFW_WINDOWED,name,NULL)) ) 
	{ 
		glfwTerminate(); 
		exit(1);
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],300,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,width,height);

	glfwSwapInterval(0);			// Disable VSYNC

	atStart=glfwGetTime();
}

void VideoKill()
{
}

void VideoUpdate()
{
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	ShowScreen(MAIN_WINDOW,windowWidth[MAIN_WINDOW],windowHeight[MAIN_WINDOW]);
	glfwSwapBuffers(windows[MAIN_WINDOW]);
				
	glfwPollEvents();
}

void VideoWait()
{
	now=glfwGetTime();

	remain = now-atStart;

	while ((remain<0.02f))
	{
		now=glfwGetTime();

		remain = now-atStart;
	}
	atStart=glfwGetTime();
}

