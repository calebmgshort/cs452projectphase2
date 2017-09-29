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
int debugflag2 = 1;

// the mail boxes
mailbox MailBoxTable[MAXMBOX];

// The Mailbox slots
mailSlot MailSlotTable[MAXSLOTS];

// The message process table
mboxProc ProcTable[MAXPROC];

extern void clockHandler2(int, void *);


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

    // Create mailboxes for devices
    for (int i = 0; i < NUM_DEVICES; i++)
    {
        MboxCreate(0, MAX_MESSAGE);
        disableInterrupts();
    }

    // Initialize the interrupt handlers
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler2;

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
    // Check kernel mode
    check_kernel_mode("MboxCreate");

    // Disable interrupts
    disableInterrupts();

    // Check args
    if (slots < 0 || slots > MAXSLOTS)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): Tried to create a mailbox with an invalid number of slots (%d).\n");
        }
        enableInterrupts();
        return -1;
    }
    if (slot_size < 0 || slot_size > MAX_MESSAGE)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): Tried to create a mailbox with an invalid slot size (%d).\n");
        }
        enableInterrupts();
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
        enableInterrupts();
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

    enableInterrupts();

    // return the id of the new box
    return id;
} /* MboxCreate */

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Return values:
      -3: process zap’d or the mailbox released while blocked on the mailbox.
      -1: illegal values given as arguments.
       0: message sent successfully.
0: message sent successfully.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    // Check kernel mode
    check_kernel_mode("MboxSend");

    // Disable interrupts
    disableInterrupts();

    // Get the mailbox that mbox_id corresponds to
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxSend(): mbox_id out of range.\n");
        }
        enableInterrupts();
        return -1;
    }
    mailboxPtr box = &MailBoxTable[mbox_id];
    if (box->mboxID == ID_NEVER_EXISTED)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxSend(): mbox_id does not correspond to a mailbox.\n");
        }
        enableInterrupts();
        return -1;
    }

    // Check the message size
    if (msg_size < 0 || msg_size > box->slotSize)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxSend(): msg_size out of range for box %d.\n", box->mboxID);
        }
        enableInterrupts();
        return -1;
    }

    // Check if we were zapped beforehand
    if (isZapped())
    {
        enableInterrupts();
        return -3;
    }

    // Handle 0 slots boxes
    if (box->size == 0)
    {
        // Add current to the list of procs blocked on box
        int pid = getpid();
        mboxProcPtr proc = &ProcTable[pidToSlot(pid)];
        initProc(pid, NULL, -1);
        addBlockedProcsTail(box, proc);
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxSend(): blocking process %d.\n", proc->pid);
        }

        // Block until something is blocked on a receive
        blockMe(STATUS_BLOCKED_SEND);

        // redisable interrupts
        disableInterrupts();
    }

    // Check to see if anything is blocked on a receive from box
    if (box->blockedProcsHead != NULL && box->slotsHead == NULL)
    {
        // Put the message directly into the proc's buffer
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
            // TODO Should we discard messages that could not be delivered or keep them? Currently, we discard them.
            enableInterrupts();
            return 0; // TODO what is the correct return value?
        }
        memcpy(proc->msgBuf, msg_ptr, msg_size);
        proc->msgSize = msg_size;
        unblockProc(proc->pid); // enables interrupts
        enableInterrupts();
        return 0;
    }

    // Otherwise, the message gets put into a slot

    // ensure there is space in box for one more message, block if not
    if(box->numSlotsOccupied == box->size)
    {
        // Add current to the list of procs blocked on box
        int pid = getpid();
        mboxProcPtr proc = &ProcTable[pidToSlot(pid)];
        initProc(pid, NULL, -1);
        addBlockedProcsTail(box, proc);
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxSend(): blocking process %d.\n", proc->pid);
        }

        // Block until the message could be sent
        blockMe(STATUS_BLOCK_SEND); // enables interrupts

        // Redisable interrupts after call to blockMe
        disableInterrupts();

        // Clear proc from the table
        clearProc(pid);

        // Check if the mailbox was released
        if (proc->mboxReleased || isZapped())
        {
            enableInterrupts();
            return -3;
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

    // Add slot to box
    addMailSlot(box, slot);

    // Init slot's fields
    slot->mboxID = box->mboxID;
    memcpy(slot->data, msg_ptr, msg_size);
    slot->size = msg_size;

    enableInterrupts();
    return 0;
} /* MboxSend */

/* ------------------------------------------------------------------------
   Name - MboxCondSend
   Purpose - Conditionally send a message to a mailbox. Do not block the invoking process.
             Rather, if there is no empty slot in the mailbox in which to place the message,
             the value is returned. Also return -2 in the case that all the mailbox slots in the
             system are used and none are available to allocate for this message.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.

   Return values:
      -3: process was zap’d.
      -2: mailbox full, message not sent; or no slots available in the system.
      -1: illegal values given as arguments.
       0: message sent successfully.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
    // Check kernel mode
    check_kernel_mode("MboxCondSend");

    // Disable interrupts
    disableInterrupts();

    // Get the mailbox that mbox_id corresponds to
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondSend(): mbox_id out of range.\n");
        }

        enableInterrupts();
        return -1;
    }
    mailboxPtr box = &MailBoxTable[mbox_id];
    if (box->mboxID == ID_NEVER_EXISTED)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondSend(): mbox_id does not correspond to a mailbox.\n");
        }
        enableInterrupts();
        return -1;
    }

    // Check the message size
    if (msg_size < 0 || msg_size > box->slotSize)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondSend(): msg_size out of range for box %d.\n", box->mboxID);
        }
        enableInterrupts();
        return -1;
    }

    // Check if we were zapped beforehand
    if (isZapped())
    {
        enableInterrupts();
        return -3;
    }

    // Check to see if anything is blocked on a receive from box
    if (box->blockedProcsHead != NULL && box->slotsHead == NULL)
    {
        // Put the message directly into the proc's buffer
        mboxProcPtr proc = box->blockedProcsHead;
        removeBlockedProcsHead(box);

        // Check that the message can be received
        if (msg_size > proc->bufSize)
        {
            if (DEBUG2 && debugflag2)
            {
                USLOSS_Console("MboxCondSend(): message sent directly to process %d is too large (%d) for buffer (%d).\n", proc->pid, msg_size, proc->bufSize);
            }
            proc->msgSize = -1;
            enableInterrupts();
            return -2;
        }
        memcpy(proc->msgBuf, msg_ptr, msg_size);
        proc->msgSize = msg_size;
        unblockProc(proc->pid); // enables interrupts
        enableInterrupts();
        return 0;
    }

    // Handle 0 slot boxes
    if (box->size == 0)
    {
        // Nothing is blocked on a receive, so we cannot deliver a message
        enableInterrupts();
        return -2;
    }

 
    // Otherwise, the message gets put into a slot

    // ensure there is space in box for one more message, return -2 if not
    if(box->numSlotsOccupied == box->size)
    {
        enableInterrupts();
        return -2;
    }

    // Get a slot for the new message
    slotPtr slot = findEmptyMailSlot();
    if (slot == NULL)
    {
        // No space is available.
        USLOSS_Console("MboxCondSend(): No more space in the slots table.\n");
        enableInterrupts();
        return -2;
    }

    // Add slot to box
    addMailSlot(box, slot);

    // Init slot's fields
    slot->mboxID = box->mboxID;
    memcpy(slot->data, msg_ptr, msg_size);
    slot->size = msg_size;

    enableInterrupts();
    return 0;
} /* MboxCondSend */

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
    // Check kernel mode
    check_kernel_mode("MboxReceive");

    // Disable interrupts
    disableInterrupts();

    // Get the mailbox that mbox_id corresponds to
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): mbox_id out of range.\n");
        }
        enableInterrupts();
        return -1;
    }
    mailboxPtr box = &MailBoxTable[mbox_id];
    if (box->mboxID == ID_NEVER_EXISTED)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): mbox_id does not correspond to a mailbox.\n");
        }
        enableInterrupts();
        return -1;
    }

    // Check the message size
    if (max_msg_size < 0)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): max_msg_size out of range for box %d.\n", box->mboxID);
        }
        enableInterrupts();
        return -1;
    }

    // Handle 0 slot boxes TODO
    if (box->size == 0 && box->blockedProcsHead != NULL)
    {
        // Check if proc is blocked on a send
        mboxProcPtr proc = box->blockedProcsHead;
        
    }

    // Get the first message
    slotPtr slot = box->slotsHead;

    // No message is in the box, so we block
    if (slot == NULL)
    {
        // Init the proc in the proc table
        int currentPid = getpid();
        initProc(currentPid, msg_ptr, max_msg_size);

        // Add the proc to box's list of blocked procs
        mboxProcPtr proc = &ProcTable[pidToSlot(currentPid)];
        addBlockedProcsTail(box, proc);

        // Block until a message is available
        blockMe(STATUS_BLOCK_RECEIVE); // enables interrupts

        // redisable interrupts
        disableInterrupts();

        // Clear the unblocked proc from the table
        clearProc(currentPid);

        // Check if zapped or if the mailbox was released
        if (proc->mboxReleased || isZapped())
        {
            enableInterrupts();
            return -3;
        }

        enableInterrupts();
        return proc->msgSize;
    }

    // Check that the message will fit in the buffer
    if (slot->size > max_msg_size)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): message received from box %d is too large (%d) for buffer (%d).\n", box->mboxID, slot->size, max_msg_size);
        }
        enableInterrupts();
        return -1;
    }

    // The slot is valid so we can remove it from the box
    removeSlotsHead(box);

    // Check to see if anything is blocked on a send to box
    if (box->blockedProcsHead != NULL)
    {
        // Check the amount of space left in box
        mboxProcPtr proc = box->blockedProcsHead;
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): unblocking process %d.\n", proc->pid);
        }
        removeBlockedProcsHead(box);
        unblockProc(proc->pid); // enables Interrupts
    }

    // Copy the message
    memcpy(msg_ptr, slot->data, slot->size);
    enableInterrupts();
    return slot->size;
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - MboxCondReceive
   Purpose - Conditionally receive a message from a mailbox. Do not block the invoking
             process. Rather, if there is no message in the mailbox, the value -2 is returned.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns -
      -3: process was zap’d while blocked on the mailbox.
      -2: no message available to receive.
      -1: illegal values given as arguments; or, message sent is too large for receiver’s
          buffer (no data copied in this case).
     >=0: the size of the message received.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mbox_id, void *msg_ptr, int max_msg_size)
{
    // Check kernel mode
    check_kernel_mode("MboxCondReceive");

    // disable interrupts
    disableInterrupts();

    // Get the mailbox that mbox_id corresponds to
    if (mbox_id < 0 || mbox_id >= MAXMBOX)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondReceive(): mbox_id out of range.\n");
        }
        enableInterrupts();
        return -1;
    }
    mailboxPtr box = &MailBoxTable[mbox_id];
    if (box->mboxID == ID_NEVER_EXISTED)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondReceive(): mbox_id does not correspond to a mailbox.\n");
        }
        enableInterrupts();
        return -1;
    }

    // Check the message size
    if (max_msg_size < 0)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondReceive(): max_msg_size out of range for box %d.\n", box->mboxID);
        }
        enableInterrupts();
        return -1;
    }

    // Check if we were zapped beforehand
    if (isZapped())
    {
        enableInterrupts();
        return -3;
    }

    // Get the first next message
    slotPtr slot = box->slotsHead;

    // No message is in the box, so we block
    if (slot == NULL)
    {
        enableInterrupts();
        return -2;
    }

    // Check that the message will fit in the buffer
    if (slot->size > max_msg_size)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondReceive(): message received from box %d is too large (%d) for buffer (%d).\n", box->mboxID, slot->size, max_msg_size);
        }
        enableInterrupts();
        return -1;
    }

    // The slot is valid so we can remove it from the box
    removeSlotsHead(box);

    // Check to see if anything is blocked on a send to box
    if (box->blockedProcsHead != NULL)
    {
        // Check the amount of space left in box
        mboxProcPtr proc = box->blockedProcsHead;
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondReceive(): unblocking process %d.\n", proc->pid);
        }
        removeBlockedProcsHead(box);
        unblockProc(proc->pid); // enables interrupts
    }

    // Copy the message
    memcpy(msg_ptr, slot->data, slot->size);
    enableInterrupts();
    return slot->size;
} /* MboxCondReceive */

int MboxRelease(int mbox_id)
{
    // Check kernel mode
    check_kernel_mode("MboxRelease");

    // Disable interrupts
    disableInterrupts();

    if (mbox_id < 0 || mbox_id > MAXMBOX)
    {
        enableInterrupts();
        return -1;
    }
    mailboxPtr box = &MailBoxTable[mboxIDToIndex(mbox_id)];
    if (box->mboxID == ID_NEVER_EXISTED)
    {
        enableInterrupts();
        return -1;
    }
    mboxProcPtr proc = box->blockedProcsHead;
    while (proc != NULL)
    {
        // Flag proc as having a released mailbox and unblock
        proc->mboxReleased = 1;
        unblockProc(proc->pid); // enables interrupts
        disableInterrupts();
        proc = proc->nextBlockedProc;
    }

    enableInterrupts();    

    // Check if we were zapped while releasing the mailbox
    if (isZapped())
    {
        return -3;
    }
    return 0;
} /* MboxRelease */

int waitDevice(int type, int unit, int *status)
{
    // Check kernel mode
    check_kernel_mode("waitDevice");

    // Disable interrupts
    disableInterrupts();

    int mboxID = getDeviceMboxID(type, unit);

    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("waitDevice(): mailbox corresponding to device type %d, unit %d is %d.\n", type, unit, mboxID);
    }

    char buf[MAX_MESSAGE];

    // Do the receive on the mailbox
    MboxReceive(mboxID, buf, MAX_MESSAGE); // enables interrupts
    
    // Redisable interrupts
    disableInterrupts();

    // Check the status of the device
    if (USLOSS_DeviceInput(type, unit, status) != USLOSS_DEV_OK)
    {
        USLOSS_Console("waitDevice(): Device type %d, unit %d is invalid.\n", type, unit);
        USLOSS_Halt(1);
    }

    enableInterrupts();

    // Check if zapped
    if (isZapped())
    {
        return -1;
    }
    return 0;
}
