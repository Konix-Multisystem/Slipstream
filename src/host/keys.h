/*

	Keyboard wrapper

*/

#ifndef _KEYS__H
#define _KEYS__H

int KeyDown(int key);
int CheckKey(int key);
void ClearKey(int key);

void KeysIntialise();
void KeysKill();

void JoystickPoll();
float JoystickAxis(int axis);
int JoyDown(int button);

#endif//_KEYS__H
