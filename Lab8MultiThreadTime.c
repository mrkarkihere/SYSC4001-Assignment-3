#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define NUM_THREADS 8

#define SIZE 1000000
int array[SIZE];

int sum = 0; // sum of the array here
pthread_mutex_t lock; // lock to do CS (add to total sum)

void *sum_array(void *arg);

int main(){

    printf("Number of Threads: %d\n", NUM_THREADS);
    printf("Array Size: %d\n", SIZE);
    struct timeval start, end;
    int res;
    int thread_index;
    pthread_t threads[NUM_THREADS];
    void *thread_result;

    res = pthread_mutex_init(&lock, NULL);
    if (res != 0) {
        perror("Mutex initialization failed");
        exit(EXIT_FAILURE);
    }

    // init array
    for(int i = 0; i < SIZE; i++){
        array[i] = 1;
    }

    //gettimeofday(&start, NULL); // first time stamp

    // start threads
    for(thread_index = 0; thread_index < NUM_THREADS; thread_index++){
        gettimeofday(&start, NULL); // first time stamp
        res = pthread_create(&(threads[thread_index]), NULL, sum_array, (void *)&thread_index);
        if (res != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }

    // join threads
    for(thread_index = 0; thread_index < NUM_THREADS; thread_index++){
        res = pthread_join(threads[thread_index], NULL);
        gettimeofday(&end, NULL); // first time stamp
    }

    //gettimeofday(&end, NULL); // first time stamp

    printf("Multithread Elapsed Time: %ld microseconds, Array Sum: %d\n", ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)), sum);
    pthread_mutex_destroy(&lock);
    return 0;
}

void *sum_array(void *arg){
    int index = *(int *)arg; // basiclly which thread im at
    int thread_sum = 0;
    // draw sample arrays in paper with L = 8, L = 12, etc..
    int start_index = (SIZE/NUM_THREADS) * index;
    int end_index = (SIZE/NUM_THREADS) * (index + 1) - 1;

    for(int i = start_index; i <= end_index; i++){
        thread_sum += array[i];
    }

    // lock and unlock so only one proccess can sum at one time
    pthread_mutex_lock(&lock);
    sum += thread_sum;
    pthread_mutex_unlock(&lock);

    pthread_exit(NULL);
}
