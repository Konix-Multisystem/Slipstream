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

// Flare 1 keyboard interface

#define KEYBUF_LEN	(256)
static unsigned char keyBuffer[KEYBUF_LEN] = { 0 };

uint8_t keyBufferRead = 0;
uint8_t keyBufferWrite = 0;

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

		if (((uint8_t)(keyBufferWrite + 1)) != keyBufferRead)
		{
			keyBuffer[keyBufferWrite++] = reversed;
		}
	}
}

uint8_t FL1_KBD_Control = 0;
uint8_t FL1_KBD_ControlPrev = 0;

int FL1_KBD_InterruptPending()
{
	if (FL1_KBD_Control & 4)
	{
		return keyBufferRead != keyBufferWrite;
	}
	return 0;
}

void FL1_KBD_WriteControl(uint8_t byte)
{
	// bit 0 - interface reset
	// bit 1 - clock drive (active low)
	// bit 2 - interrupt enable
	FL1_KBD_ControlPrev = FL1_KBD_Control;
	FL1_KBD_Control = byte;
	if ((byte & (~2)) == 0)
	{
		// Keyboard reset
		keyBufferRead = 0;
		keyBufferWrite = 0;
	}
	if ((byte & 3)==3)
	{
		// interface reset
		if (keyBufferRead != keyBufferWrite)
		{
			// acknowledge key
			keyBufferRead++;
		}
	}
}

uint8_t FL1_KBD_ReadStatus()
{
	// bit 0 - keyboard data valid (interrupt active)
	// bit 3 - comparator 0
	// bit 4 - comparator 1
	// bit 5 - comparator 2
	// bit 6 - interrupt enable
	
	if (((FL1_KBD_ControlPrev^FL1_KBD_Control) & 0x2) == 2)
	{
		return 1;	// reset is instant
	}

	if (keyBufferRead != keyBufferWrite)
	{
		return 1 + ((FL1_KBD_Control & 0x4) << 4);
	}

	return 0 + ((FL1_KBD_Control & 0x4) << 4);
}

uint8_t FL1_KBD_ReadData()
{
	return keyBuffer[keyBufferRead];
}

// End Fl1 keyboard interface

// FL1 UART (MC6850)

/// Control - ittwwwcc
///				i -	  0 - disabled
///					  1	- generate interrupt for recieve register full, overrun, DCD low to high transition
///				t -  00	- (set RTS 0), transmit buffer empty interrupt disabled
///				     01	- (set RTS 0), transmit buffer empty interrupt enabled
///					 10	- (set RTS 1), transmit buffer empty interrupt disabled
///					 11	- (set RTS 0), transmit break on output, transmit buffer empty interrupt disabled
///				w - 000 - 7 Bits+Even Parity+2 Stop Bits
///					001	- 7 Bits+Odd Parity+2 Stop Bits
///					010	- 7 Bits+Even Parity+1 Stop Bit
///					011	- 7 Bits+Odd Parity+1 Stop Bit
///					100	- 8 Bits+2 Stop Bits
///					101	- 8 Bits+1 Stop Bit
///					110	- 8 Bits+Even Parity+1 Stop Bit
///					111	- 8 Bits+Odd Parity+1 Stop Bit
///				c -	 00 - Clock Divide 1
///					 01 - Clock Divide 16
///				     10 - Clock Divide 64 
///					 11 - Master Reset

void FL1_UART_MC6850_WriteControl(int uartNum, uint8_t byte)
{
#if ENABLE_DEBUG
	if ((byte & 3) == 3)
	{
		CONSOLE_OUTPUT("UART %d RESET\n", uartNum);
	}
	else
	{
		int divisor[3] = { 1,16,64 };
		CONSOLE_OUTPUT("UART %d Divider : %d\n", uartNum, divisor[byte & 3]);
	}

	if ((byte & 0x60) == 0x20)
	{
		CONSOLE_OUTPUT("UART %d TBE Interrupt enabled\n", uartNum);
	}

	if ((byte & 0x80) == 0x80)
	{
		CONSOLE_OUTPUT("UART %d RRF Interrupt enabled\n", uartNum);
	}
#endif
}

/// Status - ipofcdtr
///				i	-	1 interrupt pending (cleared by read/write to read/write data)
///				p	-	1 parity error
///				o	-	1 overrun error (recieve or transmit char lost)
///				f	-	1 framing error
///				c	-	0 clear to send, 1 - inhibits transmit buffer empty
///				d	-	0 data carrier detected - 1 - data carrier lost (when high, forces r high)
///				t	-	1 tramsit data register empty - 0 - register full (cleared on data being clocked out) 
///				r	-	1 recieve data register full - 0 -recieve data register empty (cleared on read data read)


extern uint8_t Z80_IM;

uint8_t FL1_UART_MC6850_ReadStatus(int uartNum)
{
	if (uartNum == 0)	// Serial Port (since its mapped to Baud Rate
	{
		// On boot, only the serial interface will recieve input so we simulate a character to bypass the issue  (its caused by a bug that sets interrupts enabled before the bios 
		//has copied the vectors, but the keyboard is only polled by interrupt service routine if IE has executed, fortunately, the bios will set IM 1 a little after the initial splash
		//so we can check the interrupt mode of the processor as a hack, and once its 1 we can stop forcing keys through the serial port
		if (Z80_IM == 0)
			return 0x2 + (FL1_KBD_ReadStatus() & 1);
	}

	return 0x02;
}

void FL1_UART_MC6850_WriteData(int uartNum, uint8_t byte)
{
#if ENABLE_DEBUG
	CONSOLE_OUTPUT("UART %d WriteData : %02X\n", uartNum, byte);
#endif
}

uint8_t FL1_UART_MC6850_ReadData(int uartNum)
{
	if (uartNum == 0)	// Serial Port (since its mapped to Baud Rate
	{
		// On boot, only the serial interface will recieve input so we simulate a character to bypass the issue  (its caused by a bug that sets interrupts enabled before the bios 
		//has copied the vectors, but the keyboard is only polled by interrupt service routine if IE has executed, fortunately, the bios will set IM 1 a little after the initial splash
		//so we can check the interrupt mode of the processor as a hack, and once its 1 we can stop forcing keys through the serial port
		if (Z80_IM == 0)
		{
			uint8_t data = FL1_KBD_ReadData();
			keyBufferRead = 0;
			keyBufferWrite = 0;
			return data;
		}
	}
#if ENABLE_DEBUG
	CONSOLE_OUTPUT("UART %d ReadData : AA\n", uartNum);
#endif
	return 0xAA;
}


// End FL1 UART

void termHandler( GLFWwindow* window, int key, int scan, int action, int mod );
void termCharHandler( GLFWwindow* window, unsigned int key);

extern int MAIN_WINDOW;
extern int TERMINAL_WINDOW;

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

