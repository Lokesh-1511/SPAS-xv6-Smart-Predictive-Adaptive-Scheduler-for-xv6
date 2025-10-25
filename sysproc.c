#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

// --- Externs for cpustat ---
extern int cpu_load;
extern int predicted_load;
extern enum freq_level current_frequency;
extern int THRESH_LOW_TO_MED;
extern int THRESH_MED_TO_HIGH;
extern int virtual_temp; // <-- ADDED for Phase 4
// --- End of externs ---

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

// --- Our new system call implementation ---
int
sys_cpustat(void)
{
  struct cpustat *st_user;
  struct cpustat st_kernel;

  // 1. Get the user-space pointer from the arguments
  if(argptr(0, (char**)&st_user, sizeof(*st_user)) < 0)
    return -1;

  // 2. Populate our kernel-space struct
  st_kernel.load = cpu_load;
  st_kernel.predicted_load = predicted_load;
  st_kernel.frequency_level = (int)current_frequency;
  st_kernel.temp = virtual_temp; // <-- UPDATED for Phase 4
  st_kernel.thresh_low_med = THRESH_LOW_TO_MED;
  st_kernel.thresh_med_high = THRESH_MED_TO_HIGH;

  // 3. Safely copy the kernel data to the user's pointer
  if(copyout(myproc()->pgdir, (uint)st_user, &st_kernel, sizeof(st_kernel)) < 0)
    return -1;

  return 0;
}
// --- End of new system call ---
