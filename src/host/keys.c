/*

	Keyboard wrapper

*/

#include <stdio.h>

#include <GLFW/glfw3.h>

#include "video.h"
#include "logfile.h"

extern GLFWwindow *windows[MAX_WINDOWS];

unsigned char keyArray[512*3];
int joystickDetected=0;
const float *joystickAxis=NULL;
int joystickAxisCnt = 0;
const unsigned char *joystickButtons=NULL;
int joystickButtonCnt = 0;

#define KEYBUF_LEN	(20)
unsigned char keyBuffer[KEYBUF_LEN]={0x0,0x00,0x15,0x15,0x00};

int KeyDown(int key)
{
	return keyArray[key*3+1]!=GLFW_RELEASE;
}

int CheckKey(int key)
{
	return keyArray[key*3+2];
}

void ClearKey(int key)
{
	keyArray[key*3+2]=0;
}


int fl1_scan_codes[]={
	GLFW_KEY_ESCAPE,'1','2','3','4','5','6','7','8','9','0','-','=',GLFW_KEY_BACKSPACE,
	GLFW_KEY_TAB,'Q','W','E','R','T','Y','U','I','O','P','[',']',GLFW_KEY_ENTER,
	GLFW_KEY_LEFT_CONTROL,'A','S','D','F','G','H','J','K','L',';',0x27,0x60,
	GLFW_KEY_LEFT_SHIFT,'\\','Z','X','C','V','B','N','M',',','.','/',GLFW_KEY_RIGHT_SHIFT,GLFW_KEY_KP_MULTIPLY,			//NOTE KP MULTIPLY FILLS IN FOR MISSING KEY AFTER RSHIFT
	GLFW_KEY_LEFT_ALT,' ',GLFW_KEY_CAPS_LOCK,
	GLFW_KEY_F1,GLFW_KEY_F2,GLFW_KEY_F3,GLFW_KEY_F4,GLFW_KEY_F5,GLFW_KEY_F6,GLFW_KEY_F7,GLFW_KEY_F8,GLFW_KEY_F9,GLFW_KEY_F10,
	GLFW_KEY_NUM_LOCK,GLFW_KEY_KP_ENTER,
	GLFW_KEY_KP_7,GLFW_KEY_KP_8,GLFW_KEY_KP_9,GLFW_KEY_KP_SUBTRACT,
	GLFW_KEY_KP_4,GLFW_KEY_KP_5,GLFW_KEY_KP_6,GLFW_KEY_KP_ADD,
	GLFW_KEY_KP_1,GLFW_KEY_KP_2,GLFW_KEY_KP_3,
	GLFW_KEY_KP_0,GLFW_KEY_KP_DECIMAL,0};

unsigned char FindCode(int key)
{
	int* ptr=fl1_scan_codes;
	int counter=1;
	while (*ptr!=0)
	{
		if (*ptr==key)
			return counter;
		ptr++;
		counter++;
	}
	return 0;
}

unsigned int reverse(register unsigned int x)
{
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
    return((x >> 16) | (x << 16));
}


void kbHandler( GLFWwindow* window, int key, int scan, int action, int mod )		/* At present ignores which window, will add per window keys later */
{
	keyArray[key*3 + 0]=keyArray[key*3+1];
	keyArray[key*3 + 1]=action;
	keyArray[key*3 + 2]|=(keyArray[key*3+0]==GLFW_RELEASE)&&(keyArray[key*3+1]!=GLFW_RELEASE);

	unsigned char scanCodeNum=FindCode(key);

	if (scanCodeNum!=0)
	{
		// Scan Codes are reversed, lowest bit indicates down/up
		unsigned char reversed=reverse(scanCodeNum)>>24;

		if (action==GLFW_RELEASE)
		{
			reversed|=1;
		}

		keyBuffer[0]=reversed;
	}
}

unsigned char IsKeyAvailable()
{
	if (keyBuffer[0])
	{
		return 1;
	}
	return 0;
}

unsigned char NextKeyCode()
{
	if (keyBuffer[0])
	{
//		int a;
		unsigned char ret=keyBuffer[0];
/*		for (a=1;a<KEYBUF_LEN;a++)
		{
			keyBuffer[a-1]=keyBuffer[a];
		}
*/
		return ret;
	}
	return 0;
}

void ClearKBKey()
{
	keyBuffer[0]=0;
}
void termHandler( GLFWwindow* window, int key, int scan, int action, int mod );
void termCharHandler( GLFWwindow* window, unsigned int key);

void KeysIntialise(int joystick)
{
	glfwSetKeyCallback(windows[MAIN_WINDOW],kbHandler);
#if TERMINAL
	glfwSetCharCallback(windows[TERMINAL_WINDOW],termCharHandler);
	glfwSetKeyCallback(windows[TERMINAL_WINDOW],termHandler);
#endif
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
		int axis_count;
		int button_count;
		glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axis_count);
		glfwGetJoystickButtons(GLFW_JOYSTICK_1, &button_count);
		CONSOLE_OUTPUT("Joystick has %d axis and %d buttons\n - Currently button/axis mappings are based on 360 controller -\nApologies if your joystick does not work correctly!\n",
			axis_count,button_count);
	}
}


void JoystickPoll()
{
	joystickAxis = glfwGetJoystickAxes(GLFW_JOYSTICK_1,&joystickAxisCnt);
	joystickButtons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &joystickButtonCnt);
}

float JoystickAxis(int axis)
{
	if (axis<joystickAxisCnt)
		return joystickAxis[axis];
	return 0.0f;
}

int JoyDown(int button)
{
	if (button<joystickButtonCnt)
		return joystickButtons[button];
	return 0;
}

int JoystickPresent()
{
	return joystickDetected;
}

void KeysKill()
{

}

