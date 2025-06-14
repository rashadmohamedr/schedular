            #include "headers.h"

        void signalHandler(int signalNumber)
        {
            destroyClk(false); 
            raise(SIGKILL);    
        }

        int main(int argc, char* argv[])
        {
            initClk(); // Initialize the clock system

            // Register the signal handler for child process termination
            signal(SIGCHLD, signalHandler);

            // get shared memory IDs
            int previousClockID = shmget(1, sizeof(int), 0666);
            if (previousClockID == -1)
            {
                perror("Error: Unable to access shared memory for previous clock");
                exit(EXIT_FAILURE);
            }

            int counterMemoryID = shmget(19, sizeof(int), 0666);
            if (counterMemoryID == -1)
            {
                perror("Error: Unable to access shared memory for counter");
                exit(EXIT_FAILURE);
            }

            // Attach to shared memory
            int* counter = (int*)shmat(counterMemoryID, NULL, 0);
            int* previousClock = (int*)shmat(previousClockID, NULL, 0);

            // Initialize the previousClock 
            *previousClock = getClk();

            while (*counter > 0)
            {
                printf("Current Clock: %d\n", getClk());
                printf("Remaining Time: %d\n", *counter);
                fflush(stdout);

                // Wait for a clock tick
                while (*previousClock == getClk())
                {
                    //  wait for clock to adjust
                }
                while (*previousClock == getClk())
                {
                    //  wait for clock to adjust
                }

                // Update the previous clock and decrement the counter
                *previousClock = getClk();
                (*counter)--;
            }

            // Detach from shared memory
            shmdt(counter);
            shmdt(previousClock);

            destroyClk(false); // Clean up the clock system
            return 0;
        }

