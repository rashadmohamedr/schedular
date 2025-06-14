# OS Process Scheduler Simulator
### Project Description
This project is a simulation of an operating system process scheduler in C, intended to allow one to better understand how an operating system manages process scheduling, memory management, and execution timing. It was developed for an Operating Systems course and focuses on practical application of core OS concepts.

### Project Objective
The main goal of this simulator is to demonstrate the functioning of various CPU scheduling algorithms in a controlled, user-space environment. Instead of working at the kernel level, the simulation uses separate programs that utilize inter-process communication (IPC), signals, and shared headers to mimic real OS behavior. It enables students to visualize and analyze how an OS decides which process runs, when, and for how long.

### Main Components
scheduler.c
This file has the skeleton implementation of scheduling disciplines such as First-Come-First-Serve (FCFS), Shortest Job First (SJF), and Round Robin. It manages the execution life cycle of processes by accepting them from the process generator and deciding how they must be scheduled.

process_generator.c
This is an application that reads a sequence of processes from an input file (processes.txt) and sends them to the scheduler via IPC mechanisms such as message queues or signals.

clk.c / clk.out
This is a clock module that virtualizes system time. It allows other parts of the system to synchronize actions based on virtualized time units. It's required to initialize the scheduler and process generator appropriately.

process.c
Contains the structure definition of a process and its properties such as arrival time, burst time, and priority.

headers.h
A shared header file that contains constants, data structures, and general functions used in all modules.

Supporting Files
processes.txt – Input file containing the list of processes to be scheduled.

Makefile – Automates the compilation of source files into executables.

scheduler.log – Records scheduling events and state transitions for every process.

mem.log, scheduler.perf – Optional memory usage and performance metric tracking files.

.vscode/ – VS Code settings for simple building and debugging of the project.

### How to Build the Project
Make sure you have a C compiler (for example, gcc) and make on your system. 

In your terminal, switch into the project directory and run:
```
$ make clean
$ make
```
This will compile the project and generate files like the following executable files:

scheduler.out

process_generator.out

clk.out

How to Run the Simulation
First, run the clock in the background:

``` bash
$ ./clk.out &
``` 
Then, run the scheduler:

``` bash
$ ./scheduler.out
```
Then, run the process generator:

``` bash
$ ./process_generator.out
```
You should see output files like scheduler.log, scheduler.perf, and possibly mem.log if you implemented it.

### Output and Logs
scheduler.log records the start times, end times, preemptions, and other scheduling decisions of the processes.

scheduler.perf can have performance metrics like average waiting time, turnaround time, and CPU utilization.

mem.log, if used, can show how memory was allocated or freed during the simulation.


### Notes
This project is purely educational and is typically used in university operating systems courses. It could be extended with additions such as memory management, process priorities, or multi-core support.
