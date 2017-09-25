#ifndef _PHASE2QUEUE_H
#define _PHASE2QUEUE_H

#include "message.h"

void removeSlotsHead(mailboxPtr);
void removeBlockedProcsHead(mailboxPtr);
void addBlockedProcsTail(mailboxPtr, mboxProcPtr);

#endif /* _PHASE2QUEUE_H */
