#ifndef _PHASE2UTILITY_H
#define _PHASE2UTILITY_H

void check_kernel_mode(char*);
void disableInterrupts();
void enableInterrupts();
int mboxIDToSlot(int);
int getNextMboxID();

#endif