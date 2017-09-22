#ifndef _PHASE2QUEUE_H
#define _PHASE2QUEUE_H

#include "message.h"

void removeSlotsHead(mboxPtr);
void removedBlockedProcsHead(mboxPtr);

#endif /* _PHASE2QUEUE_H */
