#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <stdlib.h>

#include "phase2utility.h"

extern mailbox MailBoxTable[];
extern mailSlot MailSlotTable[];
extern mboxProc ProcTable[];
extern int debugflag2;

/*
 * Halts if called in user mode.
 */
void check_kernel_mode(char* funcName)
{
    // test if in kernel mode; halt if in user mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_Console("%s(): called while in user mode. Halting...\n", funcName);
        USLOSS_Halt(1);
    }
}

/*
 * Disables interrupts
 */
void disableInterrupts()
{
    // check for kernel mode
    check_kernel_mode("disableInterrupts");

    // get the current value of the psr
    unsigned int psr = USLOSS_PsrGet();

    // set the current interrupt bit to 0
    psr = psr & ~USLOSS_PSR_CURRENT_INT;

    // if USLOSS gives an error, we've done something wrong!
    if (USLOSS_PsrSet(psr) == USLOSS_ERR_INVALID_PSR)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("disableInterrupts(): Bug in disableInterrupts.");
        }
    }
}

/*
 * Enables interrupts
 */
void enableInterrupts()
{
    // check for interrupts
    check_kernel_mode("enableInterrupts");

    // get the current value of the psr
    unsigned int psr = USLOSS_PsrGet();

    // set the current interrupt bit to 1
    psr = psr | USLOSS_PSR_CURRENT_INT;

    // if USLOSS gives an error, we've done something wrong!
    if (USLOSS_PsrSet(psr) == USLOSS_ERR_INVALID_PSR)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("enableInterrupts(): Bug in enableInterrupts.");
        }
    }
}

/*
 * Returns a pointer to the mailbox that id correpsonds to
 */
mailboxPtr getMailbox(int mboxID)
{
    // Get the mailbox that mbox_id corresponds to
    if (mboxID < 0 || mboxID >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("getMailbox(): mboxID %d out of range.\n", mboxID);
        }
        return NULL;
    }
    mailboxPtr box = &MailBoxTable[mboxID];
    if (box->mboxID == ID_NEVER_EXISTED)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("getMailbox(): mboxID %d does not correspond to a mailbox.\n", mboxID);
        }
        return NULL;
    }
    return box;
}

/*
 * Blocks the current process and makes a process table entry for it. Enables 
 * interrupts.
 */
void blockCurrent(mailboxPtr box, int status, void *msgBuf, int bufSize)
{
    // Add current to the list of procs blocked on box
    int pid = getpid();
    mboxProcPtr proc = &ProcTable[pid % MAXPROC];
    initProc(proc, status, msgBuf, bufSize);
    addBlockedProcsTail(box, proc);
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("MboxSend(): blocking process %d.\n", proc->pid);
    }

    blockMe(status); // enables interrupts
}

static void initProc(mboxProcPtr proc, int status, void *msgBuf, int bufSize)
{
    if (proc->pid != ID_NEVER_EXISTED)
    {
        USLOSS_Console("initProc(): Trying to overwrite existing proc entry.  Halting...\n");
        USLOSS_Halt(1);
    }
    proc->pid = pid;
    proc->status = status;
    proc->nextBlockedProc = NULL;
    proc->msgBuf = msgBuf;
    proc->bufSize = bufSize;
    proc->mboxReleased = 0;
}

/*
 * Clears space in the process table that corresponds to the given pid.
 */
void clearProc(int pid)
{
    ProcTable[pidToSlot(pid)].pid = ID_NEVER_EXISTED;
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

int getDeviceMboxID(int type, int unit)
{
    int mboxID = -1;
    int numUnits = -1;
    if (type == USLOSS_CLOCK_DEV)
    {
        numUnits = USLOSS_CLOCK_UNITS;
        mboxID = 0;
    }
    else if (type == USLOSS_DISK_DEV)
    {
        numUnits = USLOSS_DISK_UNITS;
        mboxID = 1;
    }
    else if (type == USLOSS_TERM_DEV)
    {
        numUnits = USLOSS_TERM_UNITS;
        mboxID = 3;
    }
    else
    {
        USLOSS_Console("getDeviceMboxID(): type %d does not correspond to any device type.\n", type);
        USLOSS_Halt(1);
    }
    // Check that the unit is correct
    if (unit >= numUnits)
    {
        USLOSS_Console("getDeviceMboxID(): unit number %d is invalid for device type %d.\n", unit, type);
        USLOSS_Halt(1);
    }
    // Adjust mbox_ID for unit number
    mboxID += unit;
    return mboxID;
}
