/**
 * @file lab11.c
 * @author Arun Karki
 * @date 2023-11-23
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "parse_csv.h"

#define NUM_CPU 4             // number of consumer threads
#define MAX_QUEUE_PROCESS 100 // max of 100 processes per queue
#define MILLISECOND_TO_MICROSECOND 1000 // times by * 1000 for the usleep() calls; assume all numbers used in base are in milliseconds
#define MAX(a, b) ((a) > (b) ? (a) : (b)) // find the larger number
#define MIN(a, b) ((a) < (b) ? (a) : (b)) // find the smaller number

// basic process information
struct process_information{
    int PID;                        // pid generated
    int STATIC_PRIORITY;            // default = 120; shud be b/w 100-139, lower # = higher priority
    int DYNAMIC_PRIORITY;           // changing priority value
    int REMAIN_TIME;                // remainder execution time
    int TIME_SLICE;                 // max amount of cpu time for process
    int ACCU_TIME_SLICE;            // accumulated time slice
    int LAST_CPU;                   // the thread id that the process last ran
    int EXECUTION_TIME;             // actual cpu use time of process per iteration
    int CPU_AFFINITY;               // which CPU does the process want
    enum SchedulingType SCHED_POLICY; /* scheduling policy used for process */
    int SLEEP_AVG;                  // average sleep time of the process
    int BLOCKED; // 1 if the process is in a blocked state
    struct timeval BLOCKED_TIME; // when process was blocked
};

// ready queue structure
struct core_queues{
    struct process_information RQ_0[MAX_QUEUE_PROCESS]; // ready queue 0
    struct process_information RQ_1[MAX_QUEUE_PROCESS]; // ready queue 1
    struct process_information RQ_2[MAX_QUEUE_PROCESS]; // ready queue 2
};

// globals
pthread_mutex_t mutex_lock;            // lock to do CS
struct core_queues cpu_cores[NUM_CPU]; // create N struct queues; each represents 1 core
int ready_processes = 0;                 // TESTING: use this to stop the program once 0 processes are running
int GENERATING_PCB = 1;                    // TESTING: value is 1 if producer is producing still
int MAX_SLEEP_AVG = 10; // max sleep average (10 milliseconds)

// thread functions
void *producer_thread_function();
void *cpu_thread_function(void *arg);

// generate random numbers between [min, max] range
int generate_int(int min, int max){
    if (min >= max) {min = max;}
    return (rand() % (max - min + 1)) + min;
}

// pick a ready queue depending on the priority
int pick_ready_queue(int priority){
    if(priority >= 0 && priority < 100) {return 0;} // RQ_0
    else if(priority >= 100 && priority < 130) {return 1;} // RQ_1
    else if (priority >= 130 && priority < 140) {return 2;} // RQ_2
    else {perror("invalid priority"); exit(EXIT_FAILURE);}
}

// search the ready queue for an empty index and return it
int find_index(struct process_information *queue) {
    int new_priority = queue->STATIC_PRIORITY; // priority of the new process to be inserted
    int i = 0;

    // find the correct index based on priority
    for(i = 0; i < MAX_QUEUE_PROCESS; i++) {
        if(queue[i].PID == 0 || new_priority < queue[i].STATIC_PRIORITY) {break;} // found new index
    }

    // shift elements to make space for the new process
    if (i < MAX_QUEUE_PROCESS - 1) {
        for(int j = MAX_QUEUE_PROCESS - 1; j > i; j--){
            queue[j] = queue[j - 1];
        }
    }   
    return i; // return the index where the new process should be inserted
}


// find the first index of a ready process
int find_ready_index(struct process_information *queue){
    for(int i = 0; i < MAX_QUEUE_PROCESS; i++){
        if(queue[i].PID > 0){
            return i;
        }
    }
    // return -1 here will cause the loop to continue and keep searching; awaiting processes to be added to queue
    // if no processes are found in any queue and no PCBs are being generated; then we end the simulation
    return -1; 
}

// calculate time quantum
int calc_time_quantum(int STATIC_PRIORITY){
    if(STATIC_PRIORITY < 120){
        return (140 - STATIC_PRIORITY) * 20;
    }
    return (140 - STATIC_PRIORITY) * 5;
}

// calculate dynamic priority
int calc_dyanmic_priority(int STATIC_PRIORITY, int bonus){
    return MAX(100, MIN(STATIC_PRIORITY - bonus + 5, 139));
}

// calculate the time difference between 2 times; returns in milliseconds
long double calc_delta_time(struct timeval start, struct timeval end){
    return ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
}

// calculate the sleep_avg for the process; returns int
int calc_sleep_avg(int sleep_avg, long double delta_time, int time_quantum){
    sleep_avg += delta_time / time_quantum - 1; 
    if(sleep_avg < 0) {
        sleep_avg=0;
    } else if( sleep_avg > MAX_SLEEP_AVG ){
        sleep_avg = MAX_SLEEP_AVG;
    }
    return (int) sleep_avg;
}

// enqueue a proccess to its associated queue
void enqueue(struct process_information *queue, struct process_information new_pcb){
    int index = find_index(queue);
    queue[index] = new_pcb; // add new pcb
    ready_processes++;
}

// dequeue a process from its associated queue
void dequeue(struct process_information *queue, int pid){
    for(int i = 0; i < MAX_QUEUE_PROCESS; i++){
        if(queue[i].PID == pid){
            queue[i].PID = 0; // set to 0 so another process can take queue spot
            ready_processes--; // decrement the # of processes
            break;
        }
    }
}

// useful print function to print PCB in table format
void print_pcb(struct process_information pcb){
    printf("\n[THREAD_ID_%lu]: process_information = {\n", (unsigned long) pthread_self());
    printf("\tPID = %d\n", pcb.PID);
    printf("\tSTATIC_PRIORITY = %d\n", pcb.STATIC_PRIORITY);
    printf("\tDYNAMIC_PRIORITY = %d\n", pcb.DYNAMIC_PRIORITY);
    printf("\tREMAIN_TIME = %d\n", pcb.REMAIN_TIME);
    printf("\tTIME_SLICE = %d\n", pcb.TIME_SLICE);
    printf("\tACCU_TIME_SLICE = %d\n", pcb.ACCU_TIME_SLICE);
    printf("\tLAST_CPU = %d\n", pcb.LAST_CPU);
    printf("\tEXECUTION_TIME = %d\n", pcb.EXECUTION_TIME);
    printf("\tCPU_AFFINITY = %d\n", pcb.CPU_AFFINITY);
    printf("\tSCHED_POLICY = %d\n", pcb.SCHED_POLICY);
    printf("\tSLEEP_AVG = %d\n", pcb.SLEEP_AVG);
    printf("\tBLOCKED = %d\n}\n", pcb.BLOCKED);
}

int main(){
    
    /* one thread = producer, multi-threads = consumers */
    srand(time(NULL));

    void *thread_return;            // the value returned at exit call
    pthread_t producer_thread;      // producer thread
    pthread_t cpu_threads[NUM_CPU]; // array of consumer threads (cpus)

    // init mutex lock
    if(pthread_mutex_init(&mutex_lock, NULL) != 0) {perror("Mutex initialization failed"); exit(EXIT_FAILURE);}

    // create producer thread
    if(pthread_create(&producer_thread, NULL, producer_thread_function, NULL) != 0){perror("Thread creation failed"); exit(EXIT_FAILURE);}

    // create consumer threads
    for(int thread_index = 0; thread_index < NUM_CPU; thread_index++){

        // thread pointer
        pthread_t *c_thread = &cpu_threads[thread_index];
        void *consumer_args = (void*) &thread_index;

        // create consumer thread
        if(pthread_create(c_thread, NULL, cpu_thread_function, consumer_args) != 0){perror("Thread creation failed"); exit(EXIT_FAILURE);}

        // prevent mismatches
        usleep(50);
    }

    // join cpu threads
    for(int thread_index = 0; thread_index < NUM_CPU; thread_index++){
        // thread pointer
        pthread_t *c_thread = &cpu_threads[thread_index];
        pthread_join(*c_thread, NULL); // takes value, not the pointer
    }

    // wait for producer to return; idk if necessary atm
    if(pthread_join(producer_thread, &thread_return) != 0) {perror("Thread join failed"); exit(EXIT_FAILURE);}

    // wait a second after threads join and clean-up
    sleep(1);

    pthread_mutex_destroy(&mutex_lock);

    printf("$$term$$\n");

    return 0;
}

// producer thread
void *producer_thread_function(){

    printf("[PRODUCER_ID_%lu]: thread function invoked\n", (unsigned long) pthread_self());

    // opening csv file
    FILE *file = fopen("pcb_data.csv", "r");
    if(file == NULL) {perror("Error opening file");}

    // store data read from csv file in here
    struct ProcessData process_data_copy;
    /* once > 5, start sleeping */
    int NUM_GENERATED = 0; // keep track of the nubmer of processes generated thus far

    while(GENERATING_PCB){

        int result = parse_csv_line(file, &process_data_copy);
        if(result == 1){

            // if i can put in any cpu then pick one at random
            if(process_data_copy.cpu_affinity == -1){
                process_data_copy.cpu_affinity = generate_int(0, 3);
            }

            pthread_mutex_lock(&mutex_lock);

            // pick a CPU; pick a queue; pick an index in queue
            struct core_queues *core = &cpu_cores[process_data_copy.cpu_affinity]; // pick the cpu
            int ready_queue = pick_ready_queue(process_data_copy.priority);        // R0, R1, or R3
            struct process_information *pcb;
            int index; // index in queue

            if(ready_queue == 0){               // RO
                index = find_index((core->RQ_0));
                pcb = &(core->RQ_0)[index];
            }else if(ready_queue == 1){         // R1
                index = find_index((core->RQ_1));
                pcb = &(core->RQ_1)[index];
            }else{                              // R2
                index = find_index((core->RQ_2));
                pcb = &(core->RQ_2)[index];
            }

            // initiate pcb
            pcb->PID = process_data_copy.pid;
            pcb->STATIC_PRIORITY = process_data_copy.priority; // 120;
            pcb->DYNAMIC_PRIORITY = pcb->STATIC_PRIORITY;
            pcb->REMAIN_TIME = process_data_copy.execution_time; // milliseconds
            pcb->TIME_SLICE = 0;
            pcb->ACCU_TIME_SLICE = 0;
            pcb->LAST_CPU = -1;
            pcb->EXECUTION_TIME = process_data_copy.execution_time;
            pcb->CPU_AFFINITY = process_data_copy.cpu_affinity;
            pcb->SCHED_POLICY = process_data_copy.policy;
            pcb->SLEEP_AVG = 0;

            ready_processes++;
            NUM_GENERATED++;

            pthread_mutex_unlock(&mutex_lock);

            // start sleeping after ive rapid generated the first batch of 6 processes
            if(NUM_GENERATED > 5){
                usleep(generate_int(100, 600) * MILLISECOND_TO_MICROSECOND); // sleep 100us-600 ms per each read
            }
        }else{
            break;
        }
    }

    fclose(file);
    GENERATING_PCB = 0;

    pthread_exit(NULL);
}

// consumer thread
void *cpu_thread_function(void *arg){

    int thread_index = *(int *)arg;
    //printf("[CONSUMER_ID_%lu]: thread #%d invoked\n", (unsigned long) pthread_self(), thread_index);

    struct core_queues *core = &cpu_cores[thread_index];

    // while there's a ready process OR processes are being created
    while(ready_processes > 0 || GENERATING_PCB){

        // check all queues, in highest priority -> lowest
        struct process_information *pcb;
        struct process_information *selected_queue;
        struct process_information temp_pcb;
        struct timeval UNBLOCKED_TIME;
        long double DELTA_TIME; // time difference in milliseconds

        int rq0_index = find_ready_index(core->RQ_0);
        int rq1_index = find_ready_index(core->RQ_1);
        int rq2_index = find_ready_index(core->RQ_2);

        // find a process to run, if not found, continue loop
        if(rq0_index > -1){
            pcb = &(core->RQ_0)[rq0_index];
            selected_queue = core->RQ_0;
        }else if (rq1_index > -1){
            pcb = &(core->RQ_1)[rq1_index];
            selected_queue = core->RQ_1;
        }else if (rq2_index > -1){
            pcb = &(core->RQ_2)[rq2_index];
            selected_queue = core->RQ_2;
        }else{
            continue; // no ready processes found
        }

        pthread_mutex_lock(&mutex_lock); // lock cs
        // copy pcb data to temporary buffer
        temp_pcb = *pcb;
        dequeue(selected_queue, pcb->PID); // remove process from queue

        // if its NORMAL scheduling and the task was blocked; record the time it was unblocked
        if(temp_pcb.SCHED_POLICY == NORMAL && temp_pcb.BLOCKED){
            gettimeofday(&UNBLOCKED_TIME, NULL); // record when the process was unblocked
            DELTA_TIME = calc_delta_time(UNBLOCKED_TIME, temp_pcb.BLOCKED_TIME); // calculate time difference in milliseconds
        }

        // set values
        temp_pcb.TIME_SLICE = calc_time_quantum(temp_pcb.STATIC_PRIORITY); // max cpu time
        temp_pcb.EXECUTION_TIME = generate_int(1,10); // how long process runs in cpu; in milliseconds
        temp_pcb.LAST_CPU = thread_index;
        pthread_mutex_unlock(&mutex_lock); // exit cs

        // check type of scheduling
        if(temp_pcb.SCHED_POLICY == NORMAL){ // NORMAL
            printf("[NORMAL]: Process #%d running\n", temp_pcb.PID);

            usleep(temp_pcb.TIME_SLICE * MILLISECOND_TO_MICROSECOND); // idk what this shud be..

            pthread_mutex_lock(&mutex_lock); // enter cs
            temp_pcb.ACCU_TIME_SLICE += temp_pcb.TIME_SLICE;
            temp_pcb.REMAIN_TIME = (temp_pcb.REMAIN_TIME - temp_pcb.TIME_SLICE >= 0) ? (temp_pcb.REMAIN_TIME - temp_pcb.TIME_SLICE) : 0;

            // process didn't use entire time slice
            if(temp_pcb.EXECUTION_TIME < temp_pcb.TIME_SLICE){
                temp_pcb.BLOCKED = 1;
                gettimeofday(&temp_pcb.BLOCKED_TIME, NULL); 
            }
            
            // update sleep_avg
            temp_pcb.SLEEP_AVG = calc_sleep_avg(temp_pcb.SLEEP_AVG, DELTA_TIME, temp_pcb.TIME_SLICE);

            // if 0 time remaining then process has ended
            if(temp_pcb.REMAIN_TIME == 0){
                printf("[NORMAL]: Process #%d completed. Printing table format...\n", temp_pcb.PID);
                print_pcb(temp_pcb);

            }else{ // process not done yet
                temp_pcb.DYNAMIC_PRIORITY = calc_dyanmic_priority(temp_pcb.DYNAMIC_PRIORITY, temp_pcb.SLEEP_AVG);
                // if DP >= 130 then move to RQ2; else goes back to original queue
                if(temp_pcb.DYNAMIC_PRIORITY >= 130 ){
                    printf("[NORMAL]: Process #%d finished time_slice! DP has changed to %d, enqueuing to RQ2...\n", temp_pcb.PID, temp_pcb.DYNAMIC_PRIORITY);
                    enqueue(core->RQ_2, temp_pcb); // pop back in queue   
                }else{
                    printf("[NORMAL]: Process #%d finished time_slice! Enqueuing to previous queue...\n", temp_pcb.PID);
                    enqueue(selected_queue, temp_pcb); // pop back in queue   
                }
            }
            pthread_mutex_unlock(&mutex_lock); // exit cs

        }else if(temp_pcb.SCHED_POLICY == RR){ // RR

            printf("[RR]: Process #%d running\n", temp_pcb.PID);
            usleep(temp_pcb.TIME_SLICE * MILLISECOND_TO_MICROSECOND); // how long the cpu is needed; assume its the time_slice for RR processes

            pthread_mutex_lock(&mutex_lock); // enter cs
            temp_pcb.ACCU_TIME_SLICE += temp_pcb.TIME_SLICE;
            temp_pcb.REMAIN_TIME = (temp_pcb.REMAIN_TIME - temp_pcb.TIME_SLICE >= 0) ? (temp_pcb.REMAIN_TIME - temp_pcb.TIME_SLICE) : 0;

            // if 0 time remaining then process has ended
            if(temp_pcb.REMAIN_TIME == 0){
                printf("[RR]: Process #%d completed. Printing table format...\n", temp_pcb.PID);
                print_pcb(temp_pcb);
            }else{ // process not done yet
                printf("[RR]: Process #%d finished time_slice! Enqueuing to previous queue...\n", temp_pcb.PID);
                enqueue(selected_queue, temp_pcb); // pop back in queue
            }
            pthread_mutex_unlock(&mutex_lock); // exit cs

        }else{ // FIFO
            /* FIFO tasks are non-preemptive; they run to completion, no iterations are needed */

            printf("[FIFO]: Process #%d running\n", temp_pcb.PID);
            usleep(temp_pcb.REMAIN_TIME * MILLISECOND_TO_MICROSECOND); // run to completion

            // all this is not necessary as 'dequeue' already got rid of the process from the queue; for testing
            pthread_mutex_lock(&mutex_lock); // enter cs
            temp_pcb.ACCU_TIME_SLICE += temp_pcb.TIME_SLICE;
            temp_pcb.REMAIN_TIME = (temp_pcb.REMAIN_TIME - temp_pcb.TIME_SLICE >= 0) ? (temp_pcb.REMAIN_TIME - temp_pcb.TIME_SLICE) : 0;
            printf("[FIFO]: Process #%d completed. Printing table format...\n", temp_pcb.PID);
            print_pcb(temp_pcb);
            pthread_mutex_unlock(&mutex_lock); // exit cs
        }
    }

    pthread_exit(NULL);
}