#include <stdio.h>      //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300


///==============================
//don't mess with this variable//
int * shmaddr;                 //
//===============================
// Memory Node
struct memory_node
{
    int size;
    int start;
    struct memory_node*next;
};
struct pData {
    int pID;
    int pArrivalT;
    int pRunT;
    int pPriority;
    int memsize;
};

struct PCB
{
    int id;
    int pid;
    int arrivalT;
    int runningT;
    int finishAt;
    int tProcessed;
    int stop;
    int priority;
    int startAt;
    int waitingTime;
    int startAddress;
    int size;
};
// Priority Queue
typedef struct prinode {
    struct PCB* data;

    // Lower values indicate higher priority
    int priority;

    struct prinode* next;
} Prinode;

// Function to Create A New Prinode
Prinode* newPrinode(struct PCB* d, int p)
{
    Prinode* temp = (Prinode*)malloc(sizeof(Prinode));
    temp->data = d;
    temp->priority = p;
    temp->next = NULL;

    return temp;
}

// Return the value at head
struct PCB* peek(Prinode** head)
{
    return (*head)->data;
}

// Removes the element with the
// highest priority from the list
void pop(Prinode** head)
{
    Prinode* temp = *head;
    (*head) = (*head)->next;
    free(temp);
}

void PriorityQueuePrint(Prinode** head)
{
    Prinode* start = *head;
    int index = 1;
    while (start) {
        printf("The prinode number %d which has id %d and burst time %d\n", index, start->data->id, start->data->runningT);
        index++;
        start = start->next;
    }
}

// Function to push according to priority
void push(Prinode** head, struct PCB* d, int p)
{
    Prinode* start = (*head);
    if (start == NULL)
    {
        (*head) = newPrinode(d, p);
        return;
    }
    // Create new Prinode
    Prinode* temp = newPrinode(d, p);

    // Special Case: The head of list has lesser
    // priority than new prinode. So insert new
    // prinode before head prinode and change head prinode.
    if ((*head)->priority > p) {

        // Insert New Prinode before head
        temp->next = *head;
        (*head) = temp;
    }
    else {

        // Traverse the list and find a
        // position to insert new prinode
        while (start->next != NULL &&
            start->next->priority <= p) {
            start = start->next;
        }

        // Either at the ends of the list
        // or at required position
        temp->next = start->next;
        start->next = temp;
    }
}

// Function to check if list is empty
int isEmpty(Prinode** head)
{
    return (*head) == NULL;
}

bool IsThere(Prinode** head, int val)
{
    Prinode* start = *head;
    if (start == NULL)
        return false;
    if (start->data->id == val)
    {
        //*head = start->next;
        return true;
    }
    while (start->next)
    {
        if (start->next->data->id == val)
        {
            start->next = start->next->next;
            return true;
        }
        start = start->next;
    }
    return false;
}

// normal Q
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Define the structure for the queue node
typedef struct queueNode {
    struct PCB* data;
    struct queueNode* next;
} QueueNode;

// Define the structure for the queue
typedef struct queue {
    QueueNode* head;
    QueueNode* rear;
} Queue;

// Function to create a new QueueNode
QueueNode* newQueueNode(struct PCB* d)
{
    QueueNode* temp = (QueueNode*)malloc(sizeof(QueueNode));
    temp->data = d;
    temp->next = NULL;
    return temp;
}

// Function to initialize a new Queue
Queue* createQueue()
{
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->head = NULL;
    q->rear = NULL;
    return q;
}

// Function to add an element to the rear of the queue
void enqueue(Queue* q, struct PCB* d)
{
    QueueNode* temp = newQueueNode(d);
    if (q->rear == NULL) {
        q->head = q->rear = temp;
        return;
    }

    q->rear->next = temp;
    q->rear = temp;
}

// Function to remove an element from the front of the queue
struct PCB* dequeue(Queue* q)
{
    if (q->head == NULL)
        return NULL;

    QueueNode* temp = q->head;
    q->head = q->head->next;

    if (q->head == NULL)
        q->rear = NULL;

    struct PCB* data = temp->data;
    free(temp);
    return data;
}

// Function to check if the queue is empty
bool isQueueEmpty(Queue* q)
{
    return q->head == NULL;
}

// Function to peek at the front element of the queue
struct PCB* queuePeek(Queue* q)
{
    if (isQueueEmpty(q))
        return NULL;
    return q->head->data;
}

// Function to print the contents of the queue
void printQueue(Queue* q)
{
    QueueNode* current = q->head;
    int index = 1;
    while (current) {
        printf("The queue node number %d which has id %d and burst time %d\n", index, current->data->id, current->data->runningT);
        current = current->next;
        index++;
    }
}

int getClk()
{
    return *shmaddr;
}


/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        //Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *) shmat(shmid, (void *)0, 0);
}


/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}
