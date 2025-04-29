# Kernel Scheduling Simulator Report

## Data Structures Used

- **Min Heap**: Used for HPF and SRTN scheduling to efficiently select the process with the highest priority or shortest
  remaining time.
- **Queue**: Used for Round Robin (RR) scheduling to maintain the order of processes.
- **Linked List**: Underlying structure for queue and deque implementations.
- **Shared Memory**: Used for synchronization and communication between the scheduler and processes.
- **Message Queue**: Used for IPC between the process generator and scheduler.

## Algorithm Explanation

### Process Generator

- Reads process definitions from a file.
- At each clock tick, spawns processes at their arrival time and sends their PCB to the scheduler via message queue.

### Scheduler

- Receives PCBs from the process generator.
- Depending on the selected algorithm:
    - **HPF**: Picks the process with the highest priority (lowest value) from the min heap and runs it to completion.
    - **SRTN**: Picks the process with the shortest remaining time from the min heap. Preempts if a new process with a
      shorter remaining time arrives.
    - **RR**: Picks the next process from the queue and runs it for a fixed quantum or until completion, then
      re-enqueues if not finished.
- Uses shared memory for handshaking and process control.
- Logs process state transitions and statistics.

### Process

- Waits for the scheduler to signal it to run.
- Runs for the specified time slice, then updates its status in shared memory.
- Notifies the process generator upon completion.

## Results

- The simulator produces `scheduler.log` and `scheduler.perf` files.
- Example statistics (from `scheduler.perf`):

  ```
  CPU utilization = 100.00%
  Avg WTA = 1.15
  Avg Waiting = 1.00
  Std WTA = 0.15
  ```

- The log file records all process state transitions with timestamps.

## Assumptions

- All processes are independent and do not require I/O.
- Arrival times are non-decreasing in the input file.
- The number of processes does not exceed `MAX_INPUT_PROCESSES` (100).

## Workload Distribution

| Name            | Tasks                                                                                            |
|-----------------|--------------------------------------------------------------------------------------------------|
| Anas Magdy      | Process generator, process file parsing, Data structures,                                        |
| Youssef Noser   | message queue IPC, Scheduler core logic, SRTN algorithm , shared memory protocol , Makefile      | 
| Hussein Mohamed | HPF algorithm, logging, statistics, Helped in message queue IPC                                  |
| Karim Yasser    | Process code, signal handling, RR algorithm, testing, documentation, Helped in message queue IPC |

## Time Taken for Each Task

| Task                             | Time (hours) |
|----------------------------------|--------------|
| Data structure implementation    | 2            |
| Process generator & file parsing | 2            |
| Scheduler logic (HPF/SRTN/RR)    | 3            |
| Shared memory & IPC integration  | 12           |
| Logging & statistics             | 1            |
| Testing & debugging              | 4            |
| Documentation & report           | 0.5          |

---

*This report summarizes the design and implementation of the kernel scheduling simulator. All code and documentation are
included in the project repository.*
