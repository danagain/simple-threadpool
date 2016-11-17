/* 
 * File:   main.c
 * Author: Daniel Huffer
 * Reference: http://www.cs.kent.edu/~ruttan/sysprog/lectures/multi-thread/thread-pool-server-with-join.c
 * Created on November 17, 2016, 4:07 PM
 */

#define _GNU_SOURCE
#include <stdio.h>       /* standard I/O routines                     */
#include <pthread.h>     /* pthread functions and data structures     */
#include <stdlib.h>      /* rand() and srand() functions              */
#include <time.h>
#include <unistd.h>

#define NUM_HANDLER_THREADS 10

/*
Need the recursive mutex as a thread at some point may have access
to the lock and then call a function requiring access to the same lock
*/
pthread_mutex_t request_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
//Use of pthread cond for wait and signal calls (block & unblock)
pthread_cond_t got_request = PTHREAD_COND_INITIALIZER;

int num_jobs = 0;   //Global variable to track length of linked list
int finished = 0;  // all jobs complete


typedef struct Job{    //Self referential structure
    int num;
    struct Job* link;
}job;

job* head = NULL;  //Assign head and tail of the linked list
job* tail = NULL;

/*
 * Prototypes
*/
void add_job(int request_num, pthread_mutex_t* mutex, pthread_cond_t* cond_var);
struct job* get_job(pthread_mutex_t* mutex);
void thread_work(job* new_job, int thread_id);
void* process_jobs(void* t_id);

/*
 * Main Function
 */
int main(int argc, char** argv) {
    int i = 0;
    int thr_id[NUM_HANDLER_THREADS];
    struct timespec delay;  
    pthread_t p_threads[NUM_HANDLER_THREADS];
    for(int i = 0; i < NUM_HANDLER_THREADS; i++){
        thr_id[i] = i;
       pthread_create(&p_threads[i], NULL, process_jobs, (void*)&thr_id[i]); 
    }
      for (i=0; i<30; i++) {
        add_job(i, &request_mutex, &got_request);
        /* pause execution for a little bit, to allow      */
        /* other threads to run and handle some requests.  */
        if (rand() > 3*(RAND_MAX/4)) { /* this is done about 25% of the time */
            delay.tv_sec = 0;
            delay.tv_nsec = 10;
            nanosleep(&delay, NULL);
        }
    }

    /* Main thread changes the flag
     * notifying handler threads we're done creating requests. */
    {
        pthread_mutex_lock(&request_mutex);
        finished = 1;
        pthread_cond_broadcast(&got_request);
        pthread_mutex_unlock(&request_mutex);
    }


    /*pthread_join() function shall suspend execution of the calling thread 
     * until the target thread terminates */
    for (i=0; i<NUM_HANDLER_THREADS; i++) {
	void* thr_retval;

	pthread_join(p_threads[i], &thr_retval);
    }
    printf("Program exiting");
    return 0;
}


/* METHOD
 * add_job: Adds a job to the linked list queue system by...
 * 1.Dynamically allocating memory for a node structure
 * 2.Assigns the job_num and NULL's the link as it will be the new tail
 * 3.Updates the head and tail of the list accordingly
*/
void add_job(int request_num, pthread_mutex_t* mutex, pthread_cond_t* cond_var){
    job* new_job;    //Pointer to the structure
    new_job = (job*)malloc(sizeof(job));    //allocate memory for new job
    new_job->num = request_num;
    new_job->link = NULL;
 /*
 * Now the list which is a shared data structure has to be modified so
 * the mutex lock is required to ensure only 1 thread has access at a time.
 */
    pthread_mutex_lock(mutex);
 /*
 * Now have to add a new job to end of the list,
 * the list head and tail will then have to be updated
 */ 
    if(num_jobs == 0){
        head = new_job; //if there is no requests then the head of the list 
                        //is the new job
        tail = new_job;//similarly the tail is also the new job as only 1 exists 
    }else{
        tail->link = new_job;//Otherwise the new job is the tail
        tail = new_job;//and the new tail is now the new job
    }
    num_jobs++;//update the global variable
    pthread_mutex_unlock(mutex);//list editing complete, release the lock
    pthread_cond_signal(cond_var);//unblock the threads, there is now work
}//End add_job

/* METHOD
 * get_job: takes a job away from the linked list queue system by...
 * 1.Checking if there is a job to take
 * 2.If there is a job, it's taken and the list head updated
 * 3.After job removed if current link is now the end of the list, tail updated
 * 4.If there isn't any job's, new_job assigned NULL &  method returns NULL
*/

struct job* get_job(pthread_mutex_t* mutex){
    job* new_job;//setup pointer to point to head of list & take first job
    pthread_mutex_lock(mutex);//time to take job from the list
    if(num_jobs > 0){//if there is jobs avaliable
        new_job = head;//take the first job from the list
        head = new_job->link;//head is now 2nd link as we take 1st from list
        if(head == NULL){//If the new_job is now NULL it is last in list
            tail = NULL;//update the last link to reflect that
        }
        num_jobs--;//Job has been taken
    }else{
        new_job = NULL;//else the list is empty
    }
    pthread_mutex_unlock(mutex);//list updating complete
    return new_job;//return the node to caller
}

/* METHOD
 * thread_work: When the job is processed, this is the code that will be 
 * executed.
*/
void thread_work(job* new_job, int thread_id){
    if(new_job){//if new_job != NULL
        printf("Thread '%d' handled job '%d'\n\n", thread_id, new_job->num);
        fflush(stdout);
    }
}

/* METHOD
 * process_jobs: This method is the heart of the thread pool & ties everything
 * together. The basis is that it creates a pointer to a node, which is in turn
 * used to get a job when the number of jobs avaliable is greater than zero.
 * While the number of jobs is not greater than zero & there is no work 
 * the mutex condition variable is set to block the thread until the signal
 * is activated. This is all done inside a loop with locks allowing only 
 * exclusive entry.
*/
    //the function and argument must be void pointers as this method is 
    //the one been called by pthread_create
void* process_jobs(void* t_id){
    job* new_job;
   int thread_id = *((int*)t_id); //typecast & dereference void pointer
    pthread_mutex_lock(&request_mutex);//allow exclusive access
    while(1){
        if(num_jobs > 0){
            new_job = get_job(&request_mutex);
            if(new_job){
                pthread_mutex_unlock(&request_mutex);
                thread_work(new_job, thread_id );
                free(new_job);
                pthread_mutex_lock(&request_mutex);
            }
        }else{
            if(finished){ //if finished is flagged because no more jobs
              pthread_mutex_unlock(&request_mutex);
		printf("thread '%d' exiting\n", thread_id);
		fflush(stdout);
		pthread_exit(NULL);
	    }else{
                pthread_cond_wait(&got_request, &request_mutex);
            }
            }
        }
    }

