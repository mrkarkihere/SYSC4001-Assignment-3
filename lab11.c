#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "parse_csv.h"

#define NUM_CPU 1                // number of consumer threads
#define MAX_QUEUE_PROCESS 100 // max of 100 processes per queue

// basic process information
struct process_information{
    int PID;              // pid generated
    int STATIC_PRIORITY;  // default = 120; shud be b/w 100-139, lower # = higher priority
    int DYNAMIC_PRIORITY; // changing priority value
    int REMAIN_TIME;      // remainder execution time; initially b/w 5-20 secs
    int TIME_SLICE;       // calculated time slice value
    int ACCU_TIME_SLICE;  // accumulated time slice
    int LAST_CPU;         // the thread id that the process last ran
};

// ready queue structure
struct core_queues{
    struct process_information RQ_0[MAX_QUEUE_PROCESS]; // ready queue 0
    struct process_information RQ_1[MAX_QUEUE_PROCESS]; // ready queue 1
    struct process_information RQ_2[MAX_QUEUE_PROCESS]; // ready queue 2
};

// globals
pthread_mutex_t mutex_lock;                          // lock to do CS
struct core_queues cpu_cores[NUM_CPU]; // create N struct queues; each represents 1 core

// thread functions
void *producer_thread_function();
void *cpu_thread_function(void *arg);

// generate random numbers between [min, max] range
int generate_int(int min, int max){
    if(min >= max) {min = max;}
    return (rand() % (max - min + 1)) + min;
}

int main(){
    /* one thread = producer, multi-threads = consumers */
    srand(time(NULL));

    void *thread_return;                      // the value returned at exit call
    pthread_t producer_thread;                // producer thread
    pthread_t cpu_threads[NUM_CPU]; // array of consumer threads (cpus)

    // init mutex lock
    if(pthread_mutex_init(&mutex_lock, NULL) != 0) {perror("Mutex initialization failed"); exit(EXIT_FAILURE);}

    // create producer thread
    if(pthread_create(&producer_thread, NULL, producer_thread_function, NULL) != 0) {perror("Thread creation failed"); exit(EXIT_FAILURE);}
    
    // create consumer threads
    for(int thread_index = 0; thread_index < NUM_CPU; thread_index++){

        // cons attr
        pthread_t c_thread = cpu_threads[thread_index];
        pthread_attr_t c_thread_attr; // consumer thread's attributes
        void *consumer_args = thread_index;

        // set consumer thread to be detached
        if(pthread_attr_init(&c_thread_attr) != 0) {perror("Attribute creation failed"); exit(EXIT_FAILURE);}
        pthread_attr_setdetachstate(&c_thread_attr, PTHREAD_CREATE_DETACHED); // set it in a detached state; dont wait for anything

        // create consumer thread
        if(pthread_create(&c_thread, &c_thread_attr, cpu_thread_function, &consumer_args) != 0) {perror("Thread creation failed"); exit(EXIT_FAILURE);}

        // free attribute resources
        (void) pthread_attr_destroy(&c_thread_attr);

        // prevent mismatches
        usleep(50);
    }
    
    // wait for producer to return; idk if necessary atm
    if(pthread_join(producer_thread, &thread_return) != 0) {perror("Thread join failed"); exit(EXIT_FAILURE);}

    // just end after 2 secs
    sleep(2);
    pthread_mutex_destroy(&mutex_lock);

    printf("$$term$$\n");

    return 0;
}

// producer thread
void *producer_thread_function(){

    printf("[PRODUCER_ID_%lu]: thread function invoked\n", pthread_self());
    //printf("[PRODUCER_ID_%lu]: generating %d PCB...\n", pthread_self(), NUM_PCB_GENERATE);

    // generate random pcbs
    pthread_mutex_lock(&mutex_lock);

    // adding 2 processes in the Ready Queue 0 of Core 0
    struct core_queues* core = &cpu_cores[0];

    for(int i = 0; i < 2; i++){
        struct process_information* pcb = &(core->RQ_0)[i];
        pcb->PID = generate_int(0, 1000);
        pcb->STATIC_PRIORITY = 120;
        pcb->DYNAMIC_PRIORITY = pcb->STATIC_PRIORITY;
        pcb->REMAIN_TIME = generate_int(5, 20) * 1000; // milliseconds
        pcb->TIME_SLICE = 0;
        pcb->ACCU_TIME_SLICE = 0;
        pcb->LAST_CPU = 0;
        printf("index %d: %d\n", i, pcb->PID);
    }

    pthread_mutex_unlock(&mutex_lock);

    pthread_exit(NULL);
}

// consumer threat
void *cpu_thread_function(void *arg){

    int thread_index = *(int *) arg;
    printf("[CONSUMER_ID_%lu]: thread #%d invoked\n", pthread_self(), thread_index);

    pthread_mutex_lock(&mutex_lock);
    struct core_queues* core = &cpu_cores[thread_index];
    // access the processes
    for(int i = 0; i < 2; i++){

        struct process_information* pcb = &(core->RQ_0)[i];
        int time_quantum; // in millseconds

        // calculate time quantum
        if(pcb->STATIC_PRIORITY < 120){
            time_quantum = (140 - pcb->STATIC_PRIORITY) * 20;
        }else{
            time_quantum = (140 - pcb->STATIC_PRIORITY) * 5;
        }

        // calculate dp
        int DP;
        int bonus = generate_int(0, 10);
        DP = pcb->STATIC_PRIORITY - bonus + 5;

        if (DP < 100){
            DP = 100;
        }else if (DP > 139){
            DP = 139;
        }

        // sleep the time quantum -> "running the process"
        pcb->TIME_SLICE = time_quantum;
        usleep(pcb->TIME_SLICE);
        
        // set values
        pcb->REMAIN_TIME = (pcb->REMAIN_TIME - pcb->TIME_SLICE >= 0) ? (pcb->REMAIN_TIME - pcb->TIME_SLICE) : 0;
        pcb->ACCU_TIME_SLICE += pcb->TIME_SLICE;
        pcb->LAST_CPU = pthread_self();
        pcb->DYNAMIC_PRIORITY = DP;
        /*
        // print pcb in table format -> do i print this before or after??
        printf("\n[CONSUMER_ID_%lu]: process_information = {\n", pthread_self());
        printf("\tPID = %d\n", pcb.PID);
        printf("\tSTATIC_PRIORITY = %d\n", pcb.STATIC_PRIORITY);
        printf("\tDYNAMIC_PRIORITY = %d\n", pcb.DYNAMIC_PRIORITY);
        printf("\tREMAIN_TIME = %d\n", pcb.REMAIN_TIME);
        printf("\tTIME_SLICE = %d\n", pcb.TIME_SLICE);
        printf("\tACCU_TIME_SLICE = %d\n", pcb.ACCU_TIME_SLICE);
        printf("\tLAST_CPU = %d\n}\n", pcb.LAST_CPU);
        */
    }

    pthread_mutex_unlock(&mutex_lock);
    
    pthread_exit(NULL);
}
