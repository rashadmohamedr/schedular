#include "headers.h"

void clearResources(int);
struct message {
    long mtype; // Message type (required for message queues)
    struct pData data; // Actual process data
};

int msgq_id;
int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    key_t TL_id,MQ_id;
    MQ_id = ftok("keyfile",100);
    msgq_id = msgget(MQ_id, 0666| IPC_CREAT);
    if (msgq_id == -1)
    {
        perror("Error in creating the message queue\n");
        exit(-1);
    }

    TL_id = ftok("keyfile",150);
    int shmid = shmget(TL_id,sizeof(int)*1000,0666 | IPC_CREAT);

    if (shmid == -1)
    {
        perror("Error in creating the Time Line shared memory\n");
        exit(-1);
    }
    

    int* timeLine;
    timeLine = (int*)shmat(shmid, 0, 0);

    for (int i = 0; i < 1000; i++)
    {
        timeLine[i] = 0;
    }

    // 1. Read the input files.

    FILE* file = fopen("processes.txt","r");
    if (file == NULL) {
        printf("Error! File not opened.\n");
        return 1;
    }
    fscanf(file, "%*[^\n]");
    int totalProcesses = 0;
    
    int id, arrivalT, runT, priority, memsize;
    while (fscanf(file, "%d\t%d\t%d\t%d\t%d", &id, &arrivalT, &runT, &priority,&memsize) == 5) {
        totalProcesses = id;
    }
    struct pData* processes = malloc(sizeof(struct pData) * totalProcesses); // processes will be sent to the scheduler
    int counter = 0;
    rewind(file);
    fscanf(file, "%*[^\n]");
    while (counter < totalProcesses) {
        fscanf(file, "%d\t%d\t%d\t%d\t%d", &id, &arrivalT, &runT, &priority,&memsize);
        processes[counter].pID = id;
        processes[counter].pArrivalT = arrivalT;
        processes[counter].pRunT = runT;
        processes[counter].pPriority = priority;
        processes[counter].memsize = memsize;
        timeLine[arrivalT]++;
        counter++;
    }
    fclose(file);
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    
    printf("1: SRTF \n");
    printf("2: pre-emptive HPF \n");
    printf("3: RR \n");
    printf("Please Enter the algorithm you want: \n");
    int algoType,quantum;
    scanf("%d", &algoType);
    if (algoType == 3 )
    {
        printf("Please enter the time slot\n");
        scanf("%d", &quantum);
    }
    // 3. Initiate and create the scheduler and clock processes.
    int pid = fork();
    if (pid==-1)
    {
        printf("error in forking 1");
        exit(-1);
    }
    else if(pid ==0)
    {
        system("gcc -Wall -o scheduler.out scheduler.c -lm -fno-stack-protector");
        printf("Scheduling..\n");
        char n_str[10], a_str[10], t_str[10];
        sprintf(n_str, "%d", totalProcesses);

        if (algoType == 3) {
            sprintf(t_str, "%d", quantum);
            sprintf(a_str, "%d", algoType);
            execl("./scheduler.out", n_str, a_str, t_str, NULL);
        } 
        else {
            sprintf(a_str, "%d", algoType);
            execl("./scheduler.out", n_str, a_str, NULL);
        }

    }
    else
    {
        int cpid = fork();
        if (cpid == -1)
        {
            printf("error in forking clk");
        }
        else if (cpid == 0)
        {
            system("gcc clk.c -o clk.out -fno-stack-protector");
            execl("./clk.out", "clk", NULL);
        }
        // 4. Use this function after creating the clock process to initialize clock
        initClk();
        // To get time use this
        int x = getClk();

        // TODO Generation Main Loop
        // 5. Create a data structure for processes and provide it with its parameters.
        // 6. Send the information to the scheduler at the appropriate time.
        // 7. Clear clock resources
        counter = 0;
        int send;
        while (counter <= (totalProcesses)) {
            if (processes[counter].pArrivalT == getClk())
            {
                struct message msg;
                msg.mtype = 1; // Use a consistent type across generator and scheduler
                msg.data = processes[counter];

                send = msgsnd(msgq_id, &msg, sizeof(msg.data), !IPC_NOWAIT);
                if (send == -1) {
                    perror("Error sending message");
                    exit(-1);
                }

                counter++;
            }
        }
    }
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
    msgctl( msgq_id,IPC_RMID,0);
    raise(SIGKILL);
}
