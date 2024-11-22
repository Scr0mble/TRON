#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include "scheduler.h"

#include <assert.h>
#include <curses.h>
#include <unistd.h>
#include <ucontext.h>
#include <time.h>

#include "util.h"
#include <sys/resource.h>


// This is an upper limit on the number of tasks we can create.
#define MAX_TASKS 128

// This is the size of each task's stack memory
#define STACK_SIZE 65536
// size_t current_time_ms() {
//   struct timespec ts;
//   clock_gettime(CLOCK_REALTIME, &ts);
//   return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
// }


enum code{
  inactive,
  waiting,
  sleeping,
  blocked,
  done
};

// This struct will hold the all the necessary information for each task
typedef struct task_info {
  // This field stores all the state required to switch back to this task
  ucontext_t context;

  // This field stores another context. This one is only used when the task
  // is exiting.
  ucontext_t exit_context;

  // TODO: Add fields here so you can:
  //   a. Keep track of this task's state.
  // 0 is inactive, 1 is active
  enum code process;

  //   b. If the task is sleeping, when should it wake up?
  size_t wakeuptime;
  //   c. If the task is waiting for another task, which task is it waiting for?
  int pre;
  //   d. Was the task blocked waiting for user input? Once you successfully
  //      read input, you will need to save it here so it can be returned.
  int input;
} task_info_t;

int current_task = 0;          //< The handle of the currently-executing task
int num_tasks = 1;             //< The number of tasks created so far
task_info_t tasks[MAX_TASKS];  //< Information for every task
// timer_t timerid;
// timer_create(CLOCK_REALTIME, &sev, &timer_id);

/**
 * Initialize the scheduler. Programs should call this before calling any other
 * functiosn in this file.
 */
void scheduler_init() {
    // current_task = 0;   
    // num_tasks = 1;     
    // for (int i = 0; i < MAX_TASKS; ++i) {
    //     tasks[i].process = inactive;      
    //     tasks[i].wakeuptime = 0;   
    //     tasks[i].pre = -1;        
    // }

    // getcontext(&tasks[0].context);
}

int task_swap();

/**
 * This function will execute when a task's function returns. This allows you
 * to update scheduler states and start another task. This function is run
 * because of how the contexts are set up in the task_create function.
 */
void task_exit() {
    tasks[current_task].process = done;  
    task_swap();
}


/**
 * Create a new task and add it to the scheduler.
 *
 * \param handle  The handle for this task will be written to this location.
 * \param fn      The new task will run this function.
 */
void task_create(task_t* handle, task_fn_t fn) {
  // Claim an index for the new task
  int index = num_tasks;
  num_tasks++;

  // Set the task handle to this index, since task_t is just an int
  *handle = index;

  // We're going to make two contexts: one to run the task, and one that runs at the end of the task
  // so we can clean up. Start with the second

  // First, duplicate the current context as a starting point
  getcontext(&tasks[index].exit_context);

  // Set up a stack for the exit context
  tasks[index].exit_context.uc_stack.ss_sp = malloc(STACK_SIZE);
  tasks[index].exit_context.uc_stack.ss_size = STACK_SIZE;

  // Set up a context to run when the task function returns. This should call task_exit.
  makecontext(&tasks[index].exit_context, task_exit, 0);

  // Now we start with the task's actual running context
  getcontext(&tasks[index].context);

  // Allocate a stack for the new task and add it to the context
  tasks[index].context.uc_stack.ss_sp = malloc(STACK_SIZE);
  tasks[index].context.uc_stack.ss_size = STACK_SIZE;

  // Now set the uc_link field, which sets things up so our task will go to the exit context when
  // the task function finishes
  tasks[index].context.uc_link = &tasks[index].exit_context;

  // And finally, set up the context to execute the task function
  makecontext(&tasks[index].context, fn, 0);
  tasks[index].process = inactive;
}

/**
 * Wait for a task to finish. If the task has not yet finished, the scheduler should
 * suspend this task and wake it up later when the task specified by handle has exited.
 *
 * \param handle  This is the handle produced by task_create
 */

int task_swap() {
  int index = (current_task + 1) % num_tasks;
  int last_task = current_task;

  while(true) {
    if(tasks[index].process == inactive) {
      current_task = index;
      swapcontext(&tasks[last_task].context, &tasks[current_task].context);
      return 0;

    } else if(tasks[index].process == waiting) {
      if(tasks[tasks[index].pre].process == done) {
        tasks[index].process = inactive;
        current_task = index;
        swapcontext(&tasks[last_task].context, &tasks[current_task].context);
        return 0;
      } else {
        current_task = index;
        index = (index + 1) % num_tasks;
      }

    } else if(tasks[index].process == sleeping) {
      if(tasks[index].wakeuptime < time_ms()) {
        tasks[index].process = inactive;
        current_task = index;
        swapcontext(&tasks[last_task].context, &tasks[current_task].context);
        return 0;
      } else {
        current_task = index;
        index = (index + 1) % num_tasks;
      }

    } else if(tasks[index].process == blocked) {
      int ch;
      if((ch = getch()) != ERR) {
        tasks[index].process = inactive;
        current_task = index;
        tasks[index].input = ch;
        swapcontext(&tasks[last_task].context, &tasks[current_task].context);
        return 0;
      } else {
        current_task = index;
        index = (index + 1) % num_tasks;
      }

    } else if(tasks[index].process == done) {
      current_task = index;
      index = (index + 1) % num_tasks;
    }
  }
}

void task_wait(task_t handle) {
    tasks[current_task].process = waiting;  
    tasks[current_task].pre = handle;
    task_swap();


  // int i = 0;
  // while (true) {
  //   i %= num_tasks;
  //   if (!tasks[i].active) {
  //     int a = current_task;
  //     current_task = i;
  //     tasks[a].pre = i;

  //     swapcontext(&tasks[a].context, &tasks[i].context);
  //     return;
  //   }
  //   i++;
  // }
}

// void *timer_thread(size_t time){
//   time /= 1000.0;
//   int thread_task = current_task;
//   int second = *(int *)time
//   task[thread_task]->active = 0;
//   sleep(second);
//   task[thread_task]->active = 1;
//   return;
// }

/**
 * The currently-executing task should sleep for a specified time. If that time is larger
 * than zero, the scheduler should suspend this task and run a different task until at least
 * ms milliseconds have elapsed.
 *
 * \param ms  The number of milliseconds the task should sleep.
 */
void task_sleep(size_t ms) {
  size_t wakeup_time = time_ms() + ms;

  tasks[current_task].wakeuptime = wakeup_time;
  tasks[current_task].process = sleeping;
  task_swap();
  // TODO: Block this task until the requested time has elapsed.
  // Hint: Record the time the task should wake up instead of the time left for it to sleep. The
  // bookkeeping is easier this way.
  // pthread_t thread;
  // pthread_create(&thread, NULL, timer_thread, &
}

/**
 * Read a character from user input. If no input is available, the task should
 * block until input becomes available. The scheduler should run a different
 * task while this task is blocked.
 *
 * \returns The read character code
 */
int task_readchar() {
  tasks[current_task].process = blocked;
  int inp;
  if((inp = getch()) != ERR) {
    return inp;
  }
  inp = task_swap();
  return tasks[current_task].input;
}
