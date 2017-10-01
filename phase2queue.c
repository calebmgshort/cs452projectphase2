#include "phase2queue.h"
#include <stddef.h>

/*
 * Removes the first slot from the box's list of mail slots
 */
void removeSlotsHead(mailboxPtr box)
{
    box->slotsHead->mboxID = ID_NEVER_EXISTED;
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

/*
 * Adds a process to box's list of blocked procs.
 */
void addBlockedProcsTail(mailboxPtr box, mboxProcPtr proc)
{
    if (box->blockedProcsHead == NULL)
    {
        box->blockedProcsHead = proc;
        box->blockedProcsTail = proc;
    }
    else
    {
        box->blockedProcsTail->nextBlockedProc = proc;
        box->blockedProcsTail = proc;
    }
}
