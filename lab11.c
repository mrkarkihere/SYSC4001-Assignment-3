/** NOTE **/

// need to do the new calculation bullshits
// make sure to end proccesses at the right time -> some checks with the number
// re-add the correct csv file, not the short one
// change scheduling depending on enum thing

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "parse_csv.h"

#define NUM_CPU 4             // number of consumer threads
#define MAX_QUEUE_PROCESS 100 // max of 100 processes per queue

// basic process information
struct process_information
{
    int PID;              // pid generated
    int STATIC_PRIORITY;  // default = 120; shud be b/w 100-139, lower # = higher priority
    int DYNAMIC_PRIORITY; // changing priority value
    int REMAIN_TIME;      // remainder execution time; initially b/w 5-20 secs
    int TIME_SLICE;       // calculated time slice value
    int ACCU_TIME_SLICE;  // accumulated time slice
    int LAST_CPU;         // the thread id that the process last ran
    int EXECUTION_TIME; // calculated t`otal execution time for the process
    int CPU_AFFINITY; // which CPU does the process want
};

// ready queue structure
struct core_queues
{
    struct process_information RQ_0[MAX_QUEUE_PROCESS]; // ready queue 0
    struct process_information RQ_1[MAX_QUEUE_PROCESS]; // ready queue 1
    struct process_information RQ_2[MAX_QUEUE_PROCESS]; // ready queue 2
};

// globals
pthread_mutex_t mutex_lock;            // lock to do CS
struct core_queues cpu_cores[NUM_CPU]; // create N struct queues; each represents 1 core
int running_processes; // TESTING: use this to stop the program once 0 processes are running

// thread functions
void *producer_thread_function();
void *cpu_thread_function(void *arg);

// generate random numbers between [min, max] range
int generate_int(int min, int max)
{
    if (min >= max)
    {
        min = max;
    }
    return (rand() % (max - min + 1)) + min;
}

// pick a ready queue depending on the priority
int pick_ready_queue(int priority)
{
    if (priority >= 0 && priority < 100)
    {
        return 0; // RQ_0
    }
    else if (priority >= 100 && priority < 130)
    {
        return 1; // RQ_1
    }
    else if (priority >= 130 && priority < 140)
    {
        return 2; // RQ_2
    }
    else
    {
        perror("invalid priority");
        exit(EXIT_FAILURE);
    }
}

// search the ready queue for an empty index and return it
int find_index(struct process_information *queue)
{
    for (int i = 0; i < MAX_QUEUE_PROCESS; i++)
    {
        if (queue->PID == 0)
        {
            return i;
        }
    }
    return -1;
}

// find the first index of a ready process
int find_ready_index(struct process_information *queue){
    for (int i = 0; i < MAX_QUEUE_PROCESS; i++)
    {
        if (queue->PID > 0)
        {
            return i;
        }
    }
    return -1;
}


int main()
{
    running_processes = 1; // to start loop initially
    /* one thread = producer, multi-threads = consumers */
    srand(time(NULL));

    void *thread_return;            // the value returned at exit call
    pthread_t producer_thread;      // producer thread
    pthread_t cpu_threads[NUM_CPU]; // array of consumer threads (cpus)

    // init mutex lock
    if (pthread_mutex_init(&mutex_lock, NULL) != 0)
    {
        perror("Mutex initialization failed");
        exit(EXIT_FAILURE);
    }

    // create producer thread
    if (pthread_create(&producer_thread, NULL, producer_thread_function, NULL) != 0)
    {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    // create consumer threads
    for (int thread_index = 0; thread_index < NUM_CPU; thread_index++)
    {

        // cons attr
        pthread_t c_thread = cpu_threads[thread_index];
        pthread_attr_t c_thread_attr; // consumer thread's attributes
        void *consumer_args = thread_index;

        // set consumer thread to be detached
        if (pthread_attr_init(&c_thread_attr) != 0)
        {
            perror("Attribute creation failed");
            exit(EXIT_FAILURE);
        }
        pthread_attr_setdetachstate(&c_thread_attr, PTHREAD_CREATE_DETACHED); // set it in a detached state; dont wait for anything

        // create consumer thread
        if (pthread_create(&c_thread, &c_thread_attr, cpu_thread_function, &consumer_args) != 0)
        {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }

        // free attribute resources
        (void)pthread_attr_destroy(&c_thread_attr);

        // prevent mismatches
        usleep(50);
    }

    // wait for producer to return; idk if necessary atm
    if (pthread_join(producer_thread, &thread_return) != 0)
    {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }

    // just end after 2 secs
    sleep(2);
    pthread_mutex_destroy(&mutex_lock);

    printf("$$term$$\n");

    return 0;
}

// producer thread
void *producer_thread_function()
{

    printf("[PRODUCER_ID_%lu]: thread function invoked\n", pthread_self());
    // printf("[PRODUCER_ID_%lu]: generating %d PCB...\n", pthread_self(), NUM_PCB_GENERATE);

    // opening csv file
    FILE *file = fopen("pcb_data.csv", "r");
    if (file == NULL)
    {
        perror("Error opening file");
        return 1;
    }

    // store data read from csv file in here
    struct ProcessData process_data_copy;

    while (1)
    {

        int result = parse_csv_line(file, &process_data_copy);
        if (result == 1)
        {
            // put process in this designated queue
            pthread_mutex_lock(&mutex_lock);

            // if i can put in any cpu then pick one at random
            if (process_data_copy.cpu_affinity == -1)
            {
                process_data_copy.cpu_affinity = generate_int(0, 3);
            }

            // pick a CPU; pick a queue; pick an index in queue
            int ready_queue = pick_ready_queue(process_data_copy.priority);        // R0, R1, or R3
            struct core_queues *core = &cpu_cores[process_data_copy.cpu_affinity]; // pick the cpu
            struct process_information *pcb;
            int index; // index in queue

            if (ready_queue == 0)
            { // RO
                index = find_index(&(core->RQ_0));
                pcb = &(core->RQ_0)[index];
            }
            else if (ready_queue == 1)
            { // R1
                index = find_index(&(core->RQ_1));
                pcb = &(core->RQ_1)[index];
            }
            else
            { // R2
                index = find_index(&(core->RQ_2));
                pcb = &(core->RQ_2)[index];
            }

            // initiate pcb
            pcb->PID = process_data_copy.pid; //generate_int(0, 1000);
            pcb->STATIC_PRIORITY = process_data_copy.priority;//120;
            pcb->DYNAMIC_PRIORITY = pcb->STATIC_PRIORITY;
            pcb->REMAIN_TIME = generate_int(5, 20) * 1000; // milliseconds
            pcb->TIME_SLICE = 0;
            pcb->ACCU_TIME_SLICE = 0;
            pcb->LAST_CPU = 0;
            pcb->EXECUTION_TIME = process_data_copy.execution_time;
            pcb->CPU_AFFINITY = process_data_copy.cpu_affinity;

            running_processes++;

            pthread_mutex_unlock(&mutex_lock);
        }
        else
        {
            break;
        }
    }

    fclose(file);

    pthread_exit(NULL);
}

// consumer threat
void *cpu_thread_function(void *arg)
{
    int thread_index = *(int *)arg;
    printf("[CONSUMER_ID_%lu]: thread #%d invoked\n", pthread_self(), thread_index);
    
    struct core_queues *core = &cpu_cores[thread_index];

    // while # running processes > 0
    while(running_processes > 0){

        // check all queues, in highest priority -> lowest
        struct process_information *pcb;
        int rq0_index = find_ready_index(core->RQ_0);
        int rq1_index = find_ready_index(core->RQ_1);
        int rq2_index = find_ready_index(core->RQ_2);

        // find a process to run, if not found, continue loop
        if(rq0_index > -1){
            pcb = &(core->RQ_0)[rq0_index];
        }else if(rq1_index > -1){
            pcb = &(core->RQ_1)[rq1_index];
        }else if(rq2_index > -1){
            pcb = &(core->RQ_2)[rq2_index];
        }else{
            continue;
        }

        // lock mutex and enter critical section
        pthread_mutex_lock(&mutex_lock);
        int time_quantum; // in millseconds

        // calculate time quantum
        if (pcb->STATIC_PRIORITY < 120)
        {
            time_quantum = (140 - pcb->STATIC_PRIORITY) * 20;
        }
        else
        {
            time_quantum = (140 - pcb->STATIC_PRIORITY) * 5;
        }

        // calculate dp
        int DP;
        int bonus = generate_int(0, 10);
        DP = pcb->STATIC_PRIORITY - bonus + 5;

        if (DP < 100)
        {
            DP = 100;
        }
        else if (DP > 139)
        {
            DP = 139;
        }

        // set values
        pcb->TIME_SLICE = time_quantum;
        pcb->REMAIN_TIME = (pcb->REMAIN_TIME - pcb->TIME_SLICE >= 0) ? (pcb->REMAIN_TIME - pcb->TIME_SLICE) : 0;
        pcb->ACCU_TIME_SLICE += pcb->TIME_SLICE;
        pcb->LAST_CPU = thread_index;//pthread_self();
        pcb->DYNAMIC_PRIORITY = DP;

        // print pcb in table format -> do i print this before or after??
        printf("\n[CONSUMER_ID_%lu]: process_information = {\n", pthread_self());
        printf("\tPID = %d\n", pcb->PID);
        printf("\tSTATIC_PRIORITY = %d\n", pcb->STATIC_PRIORITY);
        printf("\tDYNAMIC_PRIORITY = %d\n", pcb->DYNAMIC_PRIORITY);
        printf("\tREMAIN_TIME = %d\n", pcb->REMAIN_TIME);
        printf("\tTIME_SLICE = %d\n", pcb->TIME_SLICE);
        printf("\tACCU_TIME_SLICE = %d\n", pcb->ACCU_TIME_SLICE);
        printf("\tLAST_CPU = %d\n", pcb->LAST_CPU);
        printf("\tEXECUTION_TIME = %d\n", pcb->EXECUTION_TIME);
        printf("\tCPU_AFFINITY = %d\n}\n", pcb->CPU_AFFINITY);  
        
        // TESTING: just assume its done 
        pcb->PID = 0;
        running_processes--;

        pthread_mutex_unlock(&mutex_lock);

        // sleep the time quantum -> "running the process"
        usleep(pcb->TIME_SLICE);
    }

    pthread_exit(NULL);
}
