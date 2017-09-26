#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <stdlib.h>

#include "phase2utility.h"

extern mailbox MailBoxTable[];
extern mailSlot MailSlotTable[];
extern mboxProc ProcTable[];

void check_kernel_mode(char* functionName){

}

void disableInterrupts(){

}

void enableInterrupts(){

}

/*
 * Converts an mboxID to an index in the mailbox table
 */
int mboxIDToIndex(int id)
{
    return id % MAXMBOX;
}

/*
 * Helper for MboxCreate that returns the next mboxID. Returns -1 iff no space is available.
 */
int getNextMboxID()
{
    // Check for empty boxes
    for (int i = 0; i < MAXMBOX; i++)
    {
        if (MailBoxTable[i].mboxID == ID_NEVER_EXISTED)
        {
            return i;
        }
    }
    // TODO Check for boxes to repurpose

    // No space available
    return -1;
}

static void cleanSlot(slotPtr);

/*
 * Returns a pointer to a clean, unused entry in the slots table. Returns NULL
 * if all slots are in use.
 */
slotPtr findEmptyMailSlot()
{
    // Check for empty slots
    for (int i = 0; i < MAXSLOTS; i++)
    {
        if (MailSlotTable[i].mboxID == ID_NEVER_EXISTED)
        {
            slotPtr slot = &MailSlotTable[i];
            cleanSlot(slot);
            return slot;
        }
    }
    // TODO Check for slots to repurpose

    // No slots available
    return NULL;
}

/*
 * Helper for findEmptyMailSlot that cleans the memory pointed to by slot.
 */
static void cleanSlot(slotPtr slot)
{
    slot->mboxID = ID_NEVER_EXISTED;
    slot->status = 0; // TODO what is the default status?
    slot->size = -1;
    slot->next = NULL;
}

/*
 * Adds slot to box's list of mail slots.
 */
void addMailSlot(mailboxPtr box, slotPtr slot)
{
    if (box->slotsHead == NULL)
    {
        box->slotsHead = slot;
        box->slotsTail = slot;
    }
    else
    {
        box->slotsTail->next = slot;
        box->slotsTail = slot;
    }
    box->numSlotsOccupied++;
}

int pidToSlot(int pid)
{
    return pid % MAXPROC;
}

void initProc(int pid, void *msgBuf, int bufSize)
{
    mboxProcPtr proc = &ProcTable[pidToSlot(pid)];
    if (proc->pid != ID_NEVER_EXISTED)
    {
        USLOSS_Console("initProcInTable(): Trying to overwrite existing proc entry.  Halting...\n");
        USLOSS_Halt(1);
    }
    proc->pid = pid;
    proc->nextBlockedProc = NULL;
    proc->msgBuf = msgBuf;
    proc->bufSize = bufSize;
    proc->mboxReleased = 0;
}

void clearProc(int pid)
{
    ProcTable[pidToSlot(pid)].pid = ID_NEVER_EXISTED;
}
