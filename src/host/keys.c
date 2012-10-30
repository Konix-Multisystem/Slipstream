/*

	Keyboard wrapper

*/

#include <stdio.h>

#include <GL/glfw3.h>

unsigned char keyArray[512*3];
float joystickAxis[8];
char joystickButtons[16];

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

void kbHandler( GLFWwindow window, int key, int action )		/* At present ignores which window, will add per window keys later */
{
	keyArray[key*3 + 0]=keyArray[key*3+1];
	keyArray[key*3 + 1]=action;
	keyArray[key*3 + 2]|=(keyArray[key*3+0]==GLFW_RELEASE)&&(keyArray[key*3+1]==GLFW_PRESS);
}

void KeysIntialise()
{
	glfwSetKeyCallback(kbHandler);
}


void JoystickPoll()
{
	int a;

	glfwGetJoystickAxes(GLFW_JOYSTICK_1,joystickAxis,8);
	glfwGetJoystickButtons(GLFW_JOYSTICK_1, joystickButtons, 16);
}

float JoystickAxis(int axis)
{
	return joystickAxis[axis];
}

int JoyDown(int button)
{
	return joystickButtons[button];
}

void KeysKill()
{

}

