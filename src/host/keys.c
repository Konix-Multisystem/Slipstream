/*

	Keyboard wrapper

*/

#include <stdio.h>

#include <GLFW/glfw3.h>

#include "logfile.h"

extern GLFWwindow *windows[MAX_WINDOWS];

unsigned char keyArray[512*3];
int joystickDetected=0;
float joystickAxis[8];
unsigned char joystickButtons[16];

int KeyDown(int key)
{
	return keyArray[key*3+1]==GLFW_PRESS;
}

int CheckKey(int key)
{
	return keyArray[key*3+2];
}

void ClearKey(int key)
{
	keyArray[key*3+2]=0;
}

void kbHandler( GLFWwindow* window, int key, int scan, int action, int mod )		/* At present ignores which window, will add per window keys later */
{
	keyArray[key*3 + 0]=keyArray[key*3+1];
	keyArray[key*3 + 1]=action;
	keyArray[key*3 + 2]|=(keyArray[key*3+0]==GLFW_RELEASE)&&(keyArray[key*3+1]==GLFW_PRESS);
}

void KeysIntialise(int joystick)
{
	glfwSetKeyCallback(windows[MAIN_WINDOW],kbHandler);

	if (joystick)
	{
		joystickDetected=glfwJoystickPresent(GLFW_JOYSTICK_1);
	}
	if (!joystickDetected)
	{
		CONSOLE_OUTPUT("Unable to locate Joystick, using keyboard controls\n - Note Analogue functionality will not work!\n");
	}
	else
	{
/*
		CONSOLE_OUTPUT("Joystick has %d axis and %d buttons\n - Currently button/axis mappings are based on 360 controller -\nApologies if your joystick does not work correctly!\n",
			glfwGetJoystickAxes(GLFW_JOYSTICK_1,GLFW_AXES),glfwGetJoystickParam(GLFW_JOYSTICK_1,GLFW_BUTTONS));*/
	}
}


void JoystickPoll()
{
/*
	glfwGetJoystickAxes(GLFW_JOYSTICK_1,joystickAxis,8);
	glfwGetJoystickButtons(GLFW_JOYSTICK_1, joystickButtons, 16);
*/
}

float JoystickAxis(int axis)
{
	return joystickAxis[axis];
}

int JoyDown(int button)
{
	return joystickButtons[button];
}

int JoystickPresent()
{
	return joystickDetected;
}

void KeysKill()
{

}

