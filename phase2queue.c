#include "phase2queue.h"
#include <stddef.h>

/*
 * Removes the first slot from the box's list of mail slots
 */
void removeSlotsHead(mailboxPtr box)
{
    box->slotsHead = box->slotsHead->next;
    if (box->slotsHead == NULL)
    {
        box->slotsTail = NULL;
    }
    box->numSlotsOccupied--;
}

/*
 * Removes the first blocked proc from the box's list of blocked procs
 */
void removeBlockedProcsHead(mailboxPtr box)
{
    box->blockedProcsHead = box->blockedProcsHead->nextBlockedProc;
    if (box->blockedProcsHead == NULL)
    {
        box->blockedProcsTail = NULL;
    }
}
