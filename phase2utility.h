#ifndef _PHASE2UTILITY_H
#define _PHASE2UTILITY_H

#include "message.h"

void check_kernel_mode(char*);
void disableInterrupts();
void enableInterrupts();
int mboxIDToIndex(int);
int getNextMboxID();
slotPtr findEmptyMailSlot();
void addMailSlot(mailboxPtr, slotPtr);
void initProc(int, void *, int);
void clearProc(int);
int pidToSlot(int pid);

#endif
