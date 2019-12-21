#define INVALID -1

#define RUNNABLE 0
#define RUNNING 1
#define WAITING 2
#define DONE 3
#define EXITED 4

#define UNLOCKED 0
#define LOCKED 1

#define PRINT 0

#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <assert.h>
#include "mythreads.h"

int interruptsAreDisabled = 0;
ucontext_t main_context;
int currentID;
int waitingFor;

void *caller(thFuncPtr funcPtr, void *argPtr);
void switchThread(int fromID, int toID);
struct Node *pickThread();

struct Node
{
  int id;
  int status; // RUNNABLE, RUNNING, WAITING, DONE, or EXITED
  ucontext_t context;
  void *result;

  int lockNum;
  int condNum;
  int queuePos;

  struct Node *next;
};

struct Node *newNode(ucontext_t context);
int pushNode(ucontext_t context);
int removeNode(int id);
void exitNode(int id);
void rotate();
struct Node* getNode(int id);
void printList();

int nextid;
struct Node* head;

static void interruptDisable();
static void interruptEnable();
int lockStates[NUM_LOCKS]; // UNLOCKED or LOCKED
int condVars[NUM_LOCKS][CONDITIONS_PER_LOCK]; // next position on the queue

void threadInit()
{
  interruptDisable();

  nextid = 0;
  head = NULL;
  waitingFor = INVALID;

  getcontext(&main_context);
  main_context.uc_stack.ss_size = STACK_SIZE;
  main_context.uc_stack.ss_flags = 0;
  currentID = pushNode(main_context);
  getNode(currentID)->status = RUNNING;

  int iter, jter;
  for (iter = 0; iter < NUM_LOCKS; iter++)
  {
    lockStates[iter] = UNLOCKED;
    for (jter = 0; jter < CONDITIONS_PER_LOCK; jter++)
      condVars[iter][jter] = 0;
  }

  interruptEnable();
}

// requires interrupts are Enabled
// ensures interrupts are Enabled
int threadCreate(thFuncPtr funcPtr, void *argPtr)
{
  interruptDisable();

  if (PRINT) printf("\n%d: Thread Create: ", currentID);

  ucontext_t targetcontext;
	//get the current context as a starting point
	//for making the new thread context.
  getcontext(&targetcontext);
  targetcontext.uc_stack.ss_sp = malloc(STACK_SIZE);
	targetcontext.uc_stack.ss_size = STACK_SIZE;
  targetcontext.uc_stack.ss_flags = 0;

  // argc = 2
  // passes one argument for the function to call and one for its argument
  makecontext(&targetcontext, (void (*) (void))caller, 2, funcPtr, argPtr);

  int targetID = pushNode(targetcontext);
  switchThread(currentID, targetID);

  interruptEnable();
  return targetID;
}

// requires interrupts are Enabled
// ensures interrupts are Enabled
void threadYield()
{
  interruptDisable();

  // decide which thread to yield to
  struct Node* targetNode = pickThread();
  if (targetNode == NULL) {
    interruptEnable();
    return;
  }

  struct Node *mynode = getNode(currentID);
  switchThread(mynode->id, targetNode->id);
  interruptEnable();
}

// requires interrupts are Enabled
// ensures interrupts are Enabled
void threadJoin(int thread_id, void **result)
{
  interruptDisable();
  struct Node *node = getNode(thread_id);
  if (node == NULL) {
    if (PRINT) printf("node does not exist\n");
    interruptEnable();
    return;
  }
  else {
    while (node->status != DONE && node->status != EXITED) {
      getNode(currentID)->status = WAITING;
      waitingFor = thread_id;
      interruptEnable();
      threadYield();
      interruptDisable();
    }
    if (result != NULL)
      *result = node->result;

    exitNode(thread_id);
  }
  interruptEnable();
}

// requires interrupts are Enabled
// ensures interrupts are Enabled
void threadExit(void *result)
{
  interruptDisable();
  if (currentID == 0)
  {
    struct Node *node = head, *temp;
    while (node != NULL)
    {
      temp = node->next;
      if (node->id != 0) removeNode(node->id);
      node = temp;
    }
    exit(0);
  }
  getNode(currentID)->status = DONE;
  getNode(currentID)->result = result;
  interruptEnable();
  threadYield();
}

// requires interrupts are Enabled
// ensures interrupts are Enabled
void threadLock(int lockNum)
{
  interruptDisable();
  while (lockStates[lockNum] == LOCKED) {
    interruptEnable();
    threadYield();
    interruptDisable();
  }
  lockStates[lockNum] = LOCKED;
  interruptEnable();
}

// requires interrupts are Enabled
// ensures interrupts are Enabled
void threadUnlock(int lockNum)
{
  interruptDisable();
  lockStates[lockNum] = UNLOCKED;
  interruptEnable();
}

// requires interrupts are Enabled
// ensures interrupts are Enabled
// requires the current thread has control of the lock
void threadWait(int lockNum, int conditionNum)
{
  interruptDisable();
  if (lockStates[lockNum] != LOCKED) {
    fprintf(stderr, "Failure: call to threadWait without possession of lock");
    exit(INVALID);
  }

  struct Node *node = getNode(currentID);
  node->lockNum = lockNum;
  node->condNum = conditionNum;
  node->queuePos = condVars[lockNum][conditionNum];
  condVars[lockNum][conditionNum]++;

  interruptEnable();
  threadUnlock(lockNum);
  interruptDisable();
  while (node->queuePos > 0) {
    interruptEnable();
    threadYield();
    interruptDisable();
  }
  node->queuePos = INVALID;
  node->lockNum = INVALID;
  node->condNum = INVALID;
  interruptEnable();
  threadLock(lockNum);
}

// requires interrupts are Enabled
// ensures interrupts are Enabled
void threadSignal(int lockNum, int conditionNum)
{
  interruptDisable();
  if (condVars[lockNum][conditionNum] == 0) {
    interruptEnable();
    return;
  }
  condVars[lockNum][conditionNum]--;
  struct Node *node = head;
  while (node != NULL)
  {
    if (node->lockNum == lockNum && node->condNum == conditionNum)
      node->queuePos--;
    node = node->next;
  }
  interruptEnable();
}

// requires interrupts are Disabled
// should never return
void *caller(thFuncPtr funcPtr, void *argPtr)
{
  interruptEnable();
  void *result = (void *)funcPtr(argPtr);
  threadExit(result);
  return result;
}

// requires interrupts are Disabled
// ensures interrupts are Disabled
// simplifies calls to swapcontext
void switchThread(int fromID, int toID)
{
  if (fromID == toID) {
    if (PRINT) { printf("Switched to self: \n"); printList(); printf("\n"); }
    return;
  }

  struct Node *fromNode = getNode(fromID);
  struct Node *toNode = getNode(toID);

  ucontext_t *fromcontext, *tocontext;
  fromcontext = &(fromNode->context);
  tocontext = &(toNode->context);

  if (fromNode->status == RUNNING)
    fromNode->status = RUNNABLE;
  currentID = toID;
  toNode->status = RUNNING;

  if (PRINT) { printList(); printf("\n%d: ", currentID); }

  getcontext(fromcontext);

  //interruptEnable();
  swapcontext(fromcontext, tocontext);
  //interruptDisable();

  if (toNode->status == RUNNING)
    toNode->status = RUNNABLE;
  if (fromNode->status == RUNNABLE)
    fromNode->status = RUNNING;
  currentID = fromID;
}

// requires interrupts are Disabled
// ensures interrupts are Disabled
// chooses a RUNNABLE or WAITING Node != getNode(currentID)
// chooses the head node then uses rotate() to rearrange the list
struct Node *pickThread()
{
  struct Node *node = head;
  struct Node *currentNode = getNode(currentID),
    *waitingForNode = getNode(waitingFor);

  int index = 0;

  if (currentNode->status == WAITING && waitingForNode->status == DONE) {
    currentNode->status = RUNNING;
    node = waitingForNode;
    waitingFor = INVALID;
  } else {
    while (node != NULL)
    {
      if (node->status == RUNNABLE ||
        (node->status == WAITING && node->id != currentID)) break;
      else index++;
      node = node->next;
    }
  }

  for (; index >= 0; index--)
    rotate();
  return node;
}

struct Node *newNode(ucontext_t context)
{
  struct Node *node = malloc(sizeof(struct Node));
  node->id = nextid;
  nextid++; // error if nextid > INT_MAX
  node->status = RUNNABLE;
  node->context = context;

  node->lockNum = INVALID;
  node->condNum = INVALID;
  node->queuePos = INVALID;

  node->next = NULL;
  return node;
}

// Adds element to front of the list
// returns the new Node
struct Node *pushFront(ucontext_t context)
{
  struct Node *node = newNode(context);
  node->next = head;
  head = node;
  return node;
}

// Adds element to end of the list
// returns the new Node
struct Node *pushBack(ucontext_t context)
{
  struct Node *node = newNode(context);
  struct Node *last, *iter = head;
  if (iter == NULL) {
    head = node;
    return node;
  }
  while (iter != NULL)
  {
    last = iter;
    iter = iter->next;
  } last->next = node;
  return node;
}

int pushNode(ucontext_t context)
{
  return pushBack(context)->id;
}

// moves head to end of list
void rotate()
{
  // empty or one-member lists do not need to rotate
  if (head == NULL || head->next == NULL) return;

  struct Node *tail = head;
  while (tail != NULL)
  {
    if (tail->next == NULL) break;
    tail = tail->next;
  }
  tail->next = head;
  head = head->next;
  tail->next->next = NULL;
}

// Removes element in list with id = id
// returns id of removed item on success, otherwise returns INVALID (-1)
int removeNode(int id)
{
  struct Node *node = head, *last = NULL;

  if (id != 0) // Don't try to free the main context
    free((char *)getNode(id)->context.uc_stack.ss_sp);

  if (head->id == id) {
    node = head;
    head = head->next;
    //free(node);
    return id;
  } else {
    while (node != NULL)
    {
      if (node->id == id) {
        last->next = node->next;
        //free(node);
        return id;
      }
      last = node;
      node = node->next;
    }
  }
  return INVALID;
}

void exitNode(int id)
{
  struct Node* node = getNode(id);

  if (id != 0) // Don't try to free the main context
    free((char *)node->context.uc_stack.ss_sp);

  node->status = EXITED;
  node->lockNum = INVALID;
  node->condNum = INVALID;
  node->queuePos = INVALID;
}

// returns pointer to Node in list such that getNode->id = id
// returns NULL if id does not have a match in the list
struct Node* getNode(int id)
{
  struct Node *node = head;
  while (node != NULL)
  {
    if (node->id == id)
      return node;
    node = node->next;
  }
  return NULL;
}

void printList()
{
  struct Node *node = head;
  while (node != NULL)
  {
    if (node->id == currentID)
      printf("[%d] -> ", node->id);
    else
      printf("%d -> ", node->id);
    node = node->next;
  } printf("NULL\n");
}

static void interruptDisable() {
  //assert(!interruptsAreDisabled);
  interruptsAreDisabled = 1;
}
static void interruptEnable() {
  //assert(interruptsAreDisabled);
  interruptsAreDisabled = 0;
}
