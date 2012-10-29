/*

 Audio wrapper for open AL

*/

#ifndef _AUDIO__H
#define _AUDIO__H

#if DISABLE_AUDIO
#define AudioKill()
#define AudioInitialise(x)
#define AudioUpdate(x)
#define _AudioAddData(x,y)
#else
void AudioKill();
void AudioInitialise(unsigned int tick);
void AudioUpdate(int numClocks);
void _AudioAddData(int channel,int16_t dacValue);
#endif
#endif//_AUDIO__H

