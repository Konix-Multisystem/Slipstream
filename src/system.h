#ifndef _SYSTEM__H
#define _SYSTEM__H

typedef enum 
{
	ESS_CP1,			// CP1 dev board (SS4)
	ESS_MSU,			// MSU era (appears to be 286 code running on 89 hardware!)
	ESS_P89,			// Close/Final Production Konix 8086 era
	ESS_P88,			// Early Konix 8088 era
	ESS_FL1				// Flare One Era
}ESlipstreamSystem;

extern ESlipstreamSystem curSystem;

#endif//_SYSTEM__H

