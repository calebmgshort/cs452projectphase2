#include <phase1.h>
#include <phase2.h>
#include <usloss.h>

#include "message.h"
#include "phase2utility.h"

extern mailbox MailBoxTable[];

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

/*
 * Returns a pointer to a clean, unused entry in the slots table. Returns NULL
 * if all slots are in use.
 */
slotPtr initNewMailSlot()
{
    // Check for empty slots
    for (int i = 0; i < MAXSLOTS; i++)
    {
        if (MailSlotTable[i].mboxID == ID_NEVER_EXISTED)
        {
            slotPtr slot = &MailSlotTable[i];
            initSlot(slot);
            return slot;
        }
    }
    // TODO Check for slots to repurpose

    // No slots available
    return NULL;
}

/*
 * Helper for initNewMailSlot that cleans the memory pointed to by slot.
 */
static void initSlot(slotPtr slot)
{
    slot->mboxID = ID_NEVER_EXISTED;
    slot->status = 0; // TODO what is the default status?
    slot->size = -1;
    slot->next = NULL;
}
