#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "barrier.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_count_num_of_digits(void)
{
  struct proc *curproc = myproc();
  int num = curproc->tf->edx;
  int num_of_digits = 0;
  while(num > 0) num /= 10, num_of_digits++;
  cprintf("number of digits: %d \n", num_of_digits);
  return 1;
}

int
sys_set_path(void)
{
  char *new_path;
  if (argstr(0, &new_path) < 0)
    return -1;
  int new_path_len = strlen(new_path);
  int global_path_index = 0, new_dir_index = 0;
  char new_directory[1000];
  for (int i = 0; i < new_path_len; i++)
    if (new_path[i] != ':')
      new_directory[new_dir_index++] =  new_path[i];
    else
    {
      new_directory[new_dir_index] = '\0';
      for (int i = 0; i <= new_dir_index; i++)
        global_path[global_path_index][i] = new_directory[i];
      new_dir_index = 0;
      global_path_index++;
    }
  global_path_len = global_path_index;
  return 1;
}

int
sys_get_parent_id(void)
{
  return get_parent_id();
}

int
sys_get_children(void)
{
  int pid = 0;
  char* buf;
  int buf_size = 0;

  if (argint(0, &pid) < 0)
    return -1;
  else if(argstr(1, (void*)&buf) < 0)
    return -1;
  else if(argint(2, &buf_size) < 0)
    return -1;

  get_children(pid, buf, buf_size);
  return 0;
}

int
sys_get_posteriors(void)
{
  int pid = 0;
  char* buf;
  int buf_size = 0;

  if (argint(0, &pid) < 0)
    return -1;
  else if(argstr(1, (void*)&buf) < 0)
    return -1;
  else if(argint(2, &buf_size) < 0)
    return -1;

  get_posteriors(pid, buf, buf_size);
  return 0;
}

int 
sys_set_sleep(void)
{
  int n;

  if(argint(0, &n) < 0)
    return -1;

  set_sleep(n);
  return 0;
}

int 
sys_fill_date(void)
{
  struct rtcdate *r;
  if(argptr(0, (void*)&r, sizeof(*r)) < 0)
    return -1;

  cmostime(r);
  return 0;
}

void
sys_set_schedule_queue()
{
  int schedule_queue;
  int pid;
  argint(0, &schedule_queue);
  argint(1, &pid);
  set_schedule_queue(schedule_queue, pid);
}

void
sys_set_lottery_ticket()
{
  int lottery_ticket;
  int pid;
  argint(0, &lottery_ticket);
  argint(1, &pid);
  set_lottery_ticket(lottery_ticket, pid);
}

void
sys_set_srpf_remaining_priority()
{
  int remaining_priority;
  int num_of_decimal;
  int pid;
  argint(0, &remaining_priority);
  argint(1, &num_of_decimal);
  argint(2, &pid);
  set_srpf_remaining_priority(remaining_priority, num_of_decimal, pid);
}

void
sys_ps()
{
  ps();
}

int
sys_initbarrierlock()
{
  int max_processes_count;
  struct barrierlock* nb;
  argint(0, &max_processes_count);
  if(argptr(0, (void*)&nb, sizeof(*nb)) < 0)
    return -1;
  else
    return initbarrierlock(max_processes_count);
}

int
sys_acquirebarrierlock()
{
  int bid;
  argint(0, &bid);
  acquirebarrierlock(bid);
  return 0;
}

void
sys_test_reentrant_lock()
{
  test_reentrant_lock();
}