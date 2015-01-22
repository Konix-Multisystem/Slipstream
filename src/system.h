#ifndef _SYSTEM__H
#define _SYSTEM__H

typedef enum 
{
	ESS_MSU,			// MSU era
	ESS_P89,			// Close/Final Production Konix 8086 era
	ESS_P88,			// Early Konix 8088 era
	ESS_FL1				// Flare One Era
}ESlipstreamSystem;

extern ESlipstreamSystem curSystem;

#endif//_SYSTEM__H

