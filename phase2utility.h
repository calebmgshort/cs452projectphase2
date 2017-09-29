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
void clearProc(int);
int pidToSlot(int);
int getDeviceMboxID(int, int);
mailboxPtr getMailbox(int);
int blockCurrent(mailboxPtr, int, void *, int);
#endif
