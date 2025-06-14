#include "headers.h"
int remainTshm_id, MQ_id, timeline_id, nubmerOfTotalProcesses, finishedprocesses = 0, prevClkID;
int algorithm, quantum;
int *timeline, *remainingTime;
struct PCB *runningP;

// Priority Queue for PHPF & SJF
Prinode *readyPQ,*waitingPQ;
// Queues for RR
Queue *readyQ;

// Scheduler Performance Variables
double totalbrust = 0; // Total burst time
int finalclk = 0;      // Final clock time
double TotalWTA = 0;   // Total Weighted Turnaround Time
int idx = 0;
double TotalWaitingTime = 0; // Total waiting time
double *WTA_Arr = NULL;      // Array for WTA values
FILE *SchedulerLog, *schedulerperf, *memlog;

#define MEMORY_LIMIT 1024
// Physical memory
struct memory_node *memory[11];

void addMemoryBlock(int address, int level, int blockSize) {
	struct memory_node* newBlock = malloc(sizeof(struct memory_node));
	newBlock->start = address;
	newBlock->size = blockSize;
	newBlock->next = memory[level];
	memory[level] = newBlock;
}


int allocateBlock(int requestedSize) {
	int requiredLevel = ceil(log2(requestedSize));
	int currentLevel = requiredLevel;

	// Find the first available block at or above the required level
	while (currentLevel < 11 && !memory[currentLevel]) {
		currentLevel++;
	}

	// Split larger blocks down to the required level
	for (int level = currentLevel; level > requiredLevel && level < 11; level--) {
		int halfSize = pow(2, level - 1);
		struct memory_node* currentBlock = memory[level];
		memory[level] = memory[level]->next; // Remove the current block from the list
		int startAddress = currentBlock->start;
		free(currentBlock);

		// Add the two smaller blocks to the next lower level
		addMemoryBlock(startAddress + halfSize, level - 1, halfSize);
		addMemoryBlock(startAddress, level - 1, halfSize);
	}

	// Allocate the block at the required level if available
	if (memory[requiredLevel]) {
		struct memory_node* allocatedBlock = memory[requiredLevel];
		memory[requiredLevel] = memory[requiredLevel]->next; // Remove allocated block
		int allocatedAddress = allocatedBlock->start;
		free(allocatedBlock);
		return allocatedAddress;
	}
	else {
		return -1; // Allocation failed
	}
}


void freeBlock(int address, int blockSize) {
	int level = ceil(log2(blockSize));
	blockSize = pow(2, level); // Using pow() to calculate the block size
	int blockIndex = address / blockSize;
	int buddyIndex ; // Determines if buddy is left or right
	if (blockIndex % 2 == 0)
	{
		buddyIndex = blockIndex + 1;
	}
	else
		buddyIndex = blockIndex - 1;
		 

	struct memory_node* current = memory[level];
	struct memory_node* previous = NULL;
	int buddyFound = 0;

	// Traverse the free list to find the buddy
	while (current) {
		int currentIndex = current->start / current->size;
		if (currentIndex == (buddyIndex)) {
			// Remove the buddy block from the free list
			if (previous == NULL) {
				memory[level] = current->next;
			}
			else {
				previous->next = current->next;
			}

			// Merge the current block and its buddy
			if (blockIndex % 2 == 0) {
				freeBlock(address, blockSize * 2);
			}
			else {
				freeBlock(current->start, blockSize * 2);
			}

			free(current);
			buddyFound = 1;
			break;
		}

		previous = current;
		current = current->next;
	}

	// If no buddy was found, add the block to the free list
	if (!buddyFound) {
		addMemoryBlock(address, level, blockSize);
	}
}


bool allocateMemoryLog(struct PCB *p) {
    printf("Allocating Memory..\n");
    p->startAddress = allocateBlock(p->size);
    int i = p->startAddress;
    printf("After Allocation - start %d - size %d\n", i, p->size);
    if (i == -1) {
        return false;
    }
    int j = i + pow(2, ceil(log2(p->size))) - 1;
    fprintf(memlog, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), p->size, p->id, i, j);
    return true;
}

void deallocateMemoryLog(struct PCB *p) {
    printf("Deallocating Memory..\n");
    int i = p->startAddress;
    int j = i + pow(2, ceil(log2(p->size))) - 1;
    freeBlock(p->startAddress, p->size);
    fprintf(memlog, "At time %d freed %d bytes for process %d from %d to %d\n", getClk(), p->size, p->id, i, j);
}

void remainTShm() {
    remainTshm_id = shmget(19, sizeof(int), 0666 | IPC_CREAT);
    if (remainTshm_id == -1) {
        printf("Error while creating the remaining time shared memory");
        exit(-1);
    }
}

void prevClkShm() {
    prevClkID = shmget(1, sizeof(int), IPC_CREAT | 0666);
    if (prevClkID == -1) {
        perror("Error in creating the shared memory");
        exit(-1);
    }
}

void arrivalsShm() {
    key_t key_id;
    key_id = ftok("keyfile", 100);
    timeline_id = shmget(key_id, sizeof(int), 0666);
    if (timeline_id == -1) {
        printf("Error while creating the remaining time shared memory");
        exit(-1);
    }
}

void msgQCreat() {
    key_t key_id;
    key_id = ftok("keyfile", 100);
    MQ_id = msgget(key_id, 0444);
    if (MQ_id == -1) {
        printf("Error while creating the remaining time shared memory");
        exit(-1);
    }
}

struct message {
    long mtype;
    struct pData data;
};

struct PCB *initiateP() {
    struct message msg;
    int receive = msgrcv(MQ_id, &msg, sizeof(msg.data), 1, !IPC_NOWAIT);
    if (receive == -1) {
        perror("Error receiving message");
        return NULL;
    }

    struct PCB *process = malloc(sizeof(struct PCB));
    if (process == NULL) {
        perror("Error allocating memory for process");
        exit(1);
    }

    process->arrivalT = msg.data.pArrivalT;
    process->id = msg.data.pID;
    process->priority = msg.data.pPriority;
    process->runningT = msg.data.pRunT;
    process->finishAt = 0;
    process->tProcessed = 0;
    process->startAt = 0;
    process->stop = 0;
    process->waitingTime = 0;
    process->size = msg.data.memsize;

    return process;
}

void Start(struct PCB *process) {
    process->startAt = getClk();
    *remainingTime = process->runningT;
    process->waitingTime = process->startAt - process->arrivalT;

    fprintf(SchedulerLog, "At time %d process %d started arr %d total %d remain %d wait %d\n", 
            process->startAt, process->id, process->arrivalT, process->runningT, 
            process->runningT - process->tProcessed, process->waitingTime);
}

void stop(struct PCB *process) {
    process->stop = getClk();
    fprintf(SchedulerLog, "At time %d process %d stopped arr %d total %d remain %d wait %d\n", 
            process->stop, process->id, process->arrivalT, process->runningT, 
            process->runningT - process->tProcessed, process->waitingTime);
    kill(process->pid, SIGSTOP);
}

void cont(struct PCB* process) {
	process->waitingTime = getClk() - process->stop; // used in the output
	printf( "At time %d process %d continued arrival %d total %d remain %d wait %d\n",
    getClk(), process->id, process->arrivalT, process->runningT, process->runningT - process->tProcessed, process->waitingTime);
	
	fprintf(SchedulerLog, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
		getClk(), process->id, process->arrivalT, process->runningT,process->runningT-process->tProcessed, process->waitingTime);
	kill(process->pid, SIGCONT);
}
void finish(struct PCB* process) {
	int status;
	waitpid(runningP->pid, &status, 0);
	finalclk=getClk();
	process->waitingTime = finalclk - process->arrivalT - process->runningT;// used in the output
	int waitingTime = process->waitingTime;
	printf( "At time %d process %d finished arrival %d total %d remain %d wait %d\n",
		getClk(), process->id, process->arrivalT, process->runningT, process->runningT - process->tProcessed, process->waitingTime);
	
	totalbrust+=process->runningT;
	TotalWaitingTime+=waitingTime;
	deallocateMemoryLog(process);
	double WTA = (getClk() - process->arrivalT) * 1.0 / process->runningT; // weighted turnaround
	if (process->runningT==0)
		WTA=0;
	TotalWTA+=((int)(WTA*100))/100.0;
	WTA_Arr[idx]=((int)(WTA*100))/100.0;
	idx++;
	fprintf(SchedulerLog, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n", 
														 getClk(), process->id, process->arrivalT, process->runningT - process->tProcessed, (finalclk - process->arrivalT),process->waitingTime,finalclk - process->arrivalT,WTA);
	
	while(!isEmpty(&waitingPQ)) {
		
        struct PCB *waitingProcess = peek(&waitingPQ);
		pop(&waitingPQ);
        int allocated = allocateMemoryLog(waitingProcess);
        if (allocated) {
                enqueue(readyQ, waitingProcess);
        }else{
			push(&waitingPQ, waitingProcess, waitingProcess ->arrivalT);
			break;
		}
    }
		
	
}
void SJF() {
	while (finishedprocesses < nubmerOfTotalProcesses) {
		while (timeline[getClk()] > 0) {
			struct PCB* nProcess = initiateP();
			push(&readyPQ, nProcess, nProcess->runningT);
			timeline[getClk()]--;
		}

		if (!runningP && !isEmpty(&readyPQ)) {

			runningP = peek(&readyPQ);
			pop(&readyPQ);

			*remainingTime = runningP->runningT;

			int pid = fork();
			if (pid == -1) {
				printf("Error: Unable to fork process.\n");
				exit(-1);
			}
			else if (pid == 0) {
				execl("./process", "process", NULL);
			}
			runningP->pid = pid;
			Start(runningP);
		}

		if (runningP && *remainingTime == 0) {
			finish(runningP);
			finishedprocesses++;
			free(runningP);
			runningP = NULL;
		}
	}
}
void PHPF() {
	while (finishedprocesses < nubmerOfTotalProcesses ) {
		while (timeline[getClk()]) {
			
			struct PCB* nProcess = initiateP();
			push(&readyPQ, nProcess, nProcess->priority);
			timeline[getClk()]--;

		}
		
		
		if (runningP)
		{
			if (*remainingTime == 0) {
				runningP->tProcessed = runningP->runningT;

				//finish process;
				finish(runningP);
				//delete process data
				free(runningP);
				runningP = NULL;
				finishedprocesses++;
			}
			else if (*remainingTime > 0&& !isEmpty(&readyPQ))
			{
				if (peek(&readyPQ)->priority < runningP->priority)
				{
					runningP->tProcessed = runningP->runningT - *remainingTime;
					// stop process 

					stop(runningP);
					push(&readyPQ, runningP , runningP->priority);
					runningP = NULL;
				}
			}
		}
		if (!runningP&& !isEmpty(&readyPQ))
		{

			runningP = peek(&readyPQ);
			pop(&readyPQ);
			*remainingTime = runningP->runningT - runningP->tProcessed;


			if (runningP->tProcessed)
			{
				// resume process
				cont(runningP);
			}
			else  {
				// start process
				Start(runningP);

				int pid = fork();

				if (pid == -1)
				{
					printf("error forking the process");
					exit(-1);
				}
				else if (pid == 0) {
					// run  process file

					execl("./process", "process", NULL);
				}
				runningP->pid = pid;
			}
		}

	}

};


void RR() {
	readyQ = createQueue();
	int track = 0;// to store the initial remaining time value
	while (finishedprocesses < nubmerOfTotalProcesses) {
		while (timeline[getClk()]) {
			struct PCB* nProcess = initiateP();
			int isAllocated = allocateMemoryLog(nProcess);
			printf("%d\n",isAllocated);
			if(isAllocated){
				enqueue(readyQ, nProcess);
			}
			else{
				push(&waitingPQ, nProcess, nProcess->arrivalT);
			}
			timeline[getClk()]--;
		};
		if (!runningP && !isQueueEmpty(readyQ))
		{
			runningP = dequeue(readyQ);
			*remainingTime = runningP->runningT - runningP->tProcessed;
			track = *remainingTime;
			if (runningP->tProcessed)
			{
				// resume process
				cont(runningP);
			}
			else {
				// start process
				Start(runningP);
				int pid = fork();
				if (pid == -1)
				{
					printf("error forking the process");
					exit(-1);
				}
				else if (pid == 0) {
					// run  process file
					execl("./process", "process", NULL);
				}
				runningP->pid = pid;
			}
		}
		if (runningP)
		{
			if (!(*remainingTime))
			{
				runningP->tProcessed = runningP->runningT;
				//finish process;
				finish(runningP);
				finishedprocesses++;
				//delete process data
				free(runningP);
				runningP = NULL;
			}
			else if ((track - *remainingTime == quantum))
			{
				// update track value so that if the same process continue 
				track = *remainingTime;
				if (!isQueueEmpty(readyQ))
				{
					runningP->tProcessed = runningP->runningT - *remainingTime;
					// stop process 
					stop(runningP);
					enqueue(readyQ, runningP);
					runningP = NULL;
				}
			}
		}


	};
};

int main(int argc, char* argv[])
{
	initClk();
	addMemoryBlock(0,10,MEMORY_LIMIT);
	//TODO implement the scheduler :)
	SchedulerLog = fopen("scheduler.log", "w");
	schedulerperf = fopen("scheduler.perf", "w");
	memlog = fopen("mem.log", "w");
	remainTShm();
	arrivalsShm();
	msgQCreat();
	prevClkShm();

	timeline = (int*)shmat(timeline_id, 0, 0);

	if (timeline == (int*)-1) {
    perror("Error attaching timeline shared memory");
    exit(1);
	}
	remainingTime = (int*)shmat(remainTshm_id, 0, 0);
	if (remainingTime == (int*)-1) {
    perror("Error attaching remaining time shared memory");
    exit(1);
	}
	if (argc < 2)
	{
		printf("error");
		exit(1);
	}
	nubmerOfTotalProcesses = atoi(argv[0]);
	algorithm = atoi(argv[1]);
	WTA_Arr = (double*)malloc(sizeof(double) * nubmerOfTotalProcesses); // for .pref

	if (algorithm == 3)
		quantum = atoi(argv[2]);
	runningP = NULL;
	system("gcc process.c -o process");
	switch (algorithm)
	{
	case 1:
		SJF();
		break;
	case 2:
		
		PHPF();
		break;
	case 3:
		RR();
		break;

	}
	fprintf(SchedulerLog, "At time %d ", getClk());
	// Write performance metrics
	fprintf(schedulerperf, "CPU Utilization = %.2f%c\n", ((float)totalbrust / finalclk) * 100, '%');
	fprintf(schedulerperf, "Avg WTA = %.2f\n", TotalWTA / nubmerOfTotalProcesses);
	fprintf(schedulerperf, "Avg Waiting = %.2f\n", TotalWaitingTime / nubmerOfTotalProcesses);

	double avgWTA = TotalWTA / nubmerOfTotalProcesses;
	double stdWTA = 0;
	for (int i = 0; i < nubmerOfTotalProcesses; i++) {
			stdWTA += (WTA_Arr[i] - avgWTA)*(WTA_Arr[i] - avgWTA);
	}
	fprintf(schedulerperf, "Std WTA = %.2f\n", sqrt(stdWTA / nubmerOfTotalProcesses));
	// Cleanup
	fclose(SchedulerLog);
	fclose(schedulerperf);
	fclose(memlog);
	free(WTA_Arr);
	//upon termination release the clock resources
	shmdt(remainingTime);
	shmctl(remainTshm_id,IPC_RMID,NULL);
	shmdt(timeline);
	shmctl(timeline_id,IPC_RMID,NULL);
	shmctl(prevClkID, IPC_RMID, NULL);
	destroyClk(true);
}

