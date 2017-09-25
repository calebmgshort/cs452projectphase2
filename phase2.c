/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */
#include <phase1.h>
#include <usloss.h>
#include <stddef.h>
#include <string.h>

#include "message.h"
#include "phase2.h"
#include "phase2utility.h"
#include "phase2queue.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);

/* -------------------------- Globals ------------------------------------- */
int debugflag2 = 0;

// the mail boxes
mailbox MailBoxTable[MAXMBOX];

// The Mailbox slots
mailSlot MailSlotTable[MAXSLOTS];

// The message process table
mboxProc ProcTable[MAXPROC];

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

    // Initialize the process table
    for (int i = 0; i < MAXPROC; i++)
    {
        ProcTable[i].pid = ID_NEVER_EXISTED;
    }

    // Create dummy mailboxes to synch for testing TODO remove
    for (int i = 0; i < 7; i++)
    {
        MboxCreate(0, 0);
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
    if (slot_size < 0 || slot_size > MAX_MESSAGE)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): Tried to create a mailbox with an invalid slot size (%d).\n");
        }
        return -1;
    }

    // Get the id and index
    int id = getNextMboxID();
    if (id == -1)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): No mailbox slots remaining.\n");
        }
        return -1;
    }
    int index = mboxIDToIndex(id);
    mailboxPtr box = &MailBoxTable[index];
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("MboxCreate(): Creating mailbox with id %d at index %d.\n", id, index);
    }

    // Set fields
    box->mboxID = id;
    box->size = slots;
    box->numSlotsOccupied = 0;
    box->slotSize = slot_size;
    box->slotsHead = NULL;
    box->slotsTail = NULL;
    box->blockedProcsHead = NULL;
    box->blockedProcsTail = NULL;

    // return the id of the new box
    return id;
} /* MboxCreate */

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
    // Get the mailbox that mbox_id corresponds to
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxSend(): mbox_id out of range.\n");
        }
        return -1;
    }
    mailboxPtr box = &MailBoxTable[mbox_id];
    if (box->mboxID == ID_NEVER_EXISTED)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxSend(): mbox_id does not correspond to a mailbox.\n");
        }
        return -1;
    }

    // Check the message size
    if (msg_size < 0 || msg_size > box->slotSize)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxSend(): msg_size out of range for box %d.\n", box->mboxID);
        }
        return -1;
    }

    // Check to see if anything is blocked on box
    if (box->blockedProcsHead != NULL)
    {
        // Check the amount of space left in box
        if (box->slotsHead == NULL)   // Something is blocked on a receive
        {
            // Something is blocked on a receive
            mboxProcPtr proc = box->blockedProcsHead;
            removeBlockedProcsHead(box);

            // Check that the message can be received
            if (msg_size > proc->bufSize)
            {
                if (DEBUG2 && debugflag2)
                {
                    USLOSS_Console("MboxSend(): message sent directly to process %d is too large (%d) for buffer (%d).\n", proc->pid, msg_size, proc->bufSize);
                }
                proc->msgSize = -1;
                // TODO should we discard messages that could not be delivered or keep them
            }
            memcpy(proc->msgBuf, msg_ptr, msg_size);
            proc->msgSize = msg_size;
            unblockProc(proc->pid);
            return 0;
        }
    }

    // Get a slot for the new message
    slotPtr slot = findEmptyMailSlot();
    if (slot == NULL)
    {
        // No space is available.
        USLOSS_Console("MboxSend(): No more space in the slots table.\n");
        USLOSS_Halt(1);
    }

    // ensure there is space in box for one more message, block if not
    if(box->numSlotsOccupied == box->size)
    {
        blockMe(STATUS_BLOCK_SEND);
    }
    // TODO check if zapped or if box has been released

    // Add slot to box
    addMailSlot(box, slot);

    // Init slot's fields
    slot->mboxID = box->mboxID;
    memcpy(slot->data, msg_ptr, msg_size);
    slot->size = msg_size;


    return 0;
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
int MboxReceive(int mbox_id, void *msg_ptr, int max_msg_size)
{
    // Get the mailbox that mbox_id corresponds to
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): mbox_id out of range.\n");
        }
        return -1;
    }
    mailboxPtr box = &MailBoxTable[mbox_id];
    if (box->mboxID == ID_NEVER_EXISTED)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): mbox_id does not correspond to a mailbox.\n");
        }
        return -1;
    }

    // Check the message size
    if (max_msg_size < 0)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): max_msg_size out of range for box %d.\n", box->mboxID);
        }
        return -1;
    }

    slotPtr slot = box->slotsHead;

    // Dequeue a message
    if (slot == NULL)
    {
        // Init the proc in the proc table
        int currentPid = getpid();
        initProc(currentPid, msg_ptr, max_msg_size);

        // Add the proc to box's list of blocked procs
        mboxProcPtr proc = &ProcTable[pidToSlot(currentPid)];
        box->blockedProcsTail->nextBlockedProc = proc;
        box->blockedProcsTail = proc;

        // Block until a message is available
        blockMe(STATUS_BLOCK_RECEIVE);

        // TODO check if zapped or if mailbox released while blocked



        // Clear the unblocked proc from the table
        clearProc(currentPid);


        return proc->msgSize;
    }

    // Check that the message will fit in the buffer
    if (slot->size > max_msg_size)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): message received from box %d is too large (%d) for buffer (%d).\n", box->mboxID, slot->size, max_msg_size);
        }
        return -1;
    }
    // The slot is valid so we can remove it from the box
    removeSlotsHead(box);

    // Check to see if anything is blocked on box
    if (box->blockedProcsHead != NULL)
    {
        // Check the amount of space left in box
        if (box->slotsHead != NULL)   // Something is blocked on a send
        {
            mboxProcPtr proc = box->blockedProcsHead;
            removeBlockedProcsHead(box);
            unblockProc(proc->pid);
        }
    }


    // Copy the message
    memcpy(msg_ptr, slot->data, slot->size);
    return slot->size;
} /* MboxReceive */
