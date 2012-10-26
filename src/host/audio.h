/*

 Audio wrapper for open AL

*/

#ifndef _AUDIO__H
#define _AUDIO__H

void AudioKill();
void AudioInitialise(unsigned int tick);
void AudioUpdate(int numClocks);
void _AudioAddData(int channel,int16_t dacValue);

#endif//_AUDIO__H

