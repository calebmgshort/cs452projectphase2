/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <stddef.h>

#include "message.h"
#include "phase2utility.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// the mail boxes
mailbox MailBoxTable[MAXMBOX];

// The Mailbox slots
mailSlot MailSlotTable[MAXSLOTS];

// also need array of mail slots, array of function ptrs to system call
// handlers, ...




/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("start1(): Called.\n");
    }

    // Check the current mode
    check_kernel_mode("start1");

    // Disable interrupts
    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    // Initialize USLOSS_IntVec and system call handlers,
    // allocate mailboxes for interrupt handlers.  Etc...

    // Initialize the mailbox table
    for (int i = 0; i < MAXMBOX; i++)
    {
        MailBoxTable[i].mboxID = ID_NEVER_EXISTED;
    }

    // Initialize the slots table
    for (int i = 0; i < MAXSLOTS; i++)
    {
        MailSlotTable[i].mboxID = ID_NEVER_EXISTED;
    }

    // Enable interrupts
    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("start1(): fork'ing start2 process.\n");
    }
    int kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    int status;
    if (join(&status) != kid_pid)
    {
        USLOSS_Console("start2(): join returned something other than start2's pid.\n");
    }

    return 0;
} /* start1 */

/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    // Check args
    // TODO return -1 or -2 on incorrect args?
    if (slots < 0 || slots > MAXSLOTS)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): Tried to create a mailbox with an invalid number of slots (%d).\n");
        }
        return -1;
    }
    if (slot_size < 0 || slot_size > MAX_MESSAGE) // TODO can slot size == 0?
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): Tried to create a mailbox with an invalid slot size (%d).\n");
        }
        return -1;
    }

    // Get the slot and id
    int id = getNextMboxID();
    if (id == -1)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): No mailbox slots remaining.\n");
        }
        return -1;
    }
    int slot = mboxIDToSlot(id);
    mailboxPtr box = &MailBoxTable[slot];
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("MboxCreate(): Creating mailbox with id %d in slot %d.\n", id, slot);
    }

    // Set fields
    box->mboxID = id;
    box->size = slots;
    box->slotSize = slot_size;
    box->slotsHead = NULL;
    box->slotsTail = NULL;

    // return the id of the new box
    return id;
} /* MboxCreate */

int mboxIDToSlot(int id)
{
    return id % MAXMBOX;
}

int getNextMboxID()
{
    // Check for empty slots
    for (int i = 0; i < MAXMBOX; i++)
    {
        if (MailBoxTable[i].mboxID == ID_NEVER_EXISTED)
        {
            return i;
        }
    }
    // TODO Check for slots to repurpose

    // No slots available
    return -1;
}

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
} /* MboxReceive */
