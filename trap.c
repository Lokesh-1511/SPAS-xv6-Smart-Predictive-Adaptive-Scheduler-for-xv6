#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers

// --- CORRECTED LINES ---
extern struct spinlock tickslock;
extern uint ticks;
// --- END OF CORRECTION ---

// --- Our new code ---
extern int is_idle;
extern uint tot_ticks;
extern uint idle_ticks;
// --- End of new code ---

// --- Phase 2: Extern Variables ---
extern int cpu_load;
extern int predicted_load;
extern int load_history[];
extern int history_index;
extern enum freq_level current_frequency;
extern int THRESH_LOW_TO_MED;
extern int THRESH_MED_TO_HIGH;
// --- End of Phase 2 Externs ---

// --- Phase 4: Extern Thermal Variables ---
extern int virtual_temp;
extern int HEATING_FACTOR;
extern int COOLING_FACTOR;
extern int TEMP_THROTTLE_LIMIT;
extern int AMBIENT_TEMP;
// --- End Phase 4 ---

// --- Phase 5: Adaptive Thresholds Externs ---
extern int oscillation_count;
extern int last_switch_tick;
extern int OSCILLATION_WINDOW;
extern int MAX_OSCILLATION;
extern int ADAPTATION_PERIOD;
extern int last_adaptation_tick;
extern enum freq_level prev_frequency;
extern int adaptation_counter;
// --- End Phase 5 ---


void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}


// --- Phase 2 & 4: Predictive Scheduler & Thermal Logic ---

// This function is called every LOAD_PERIOD ticks from the timer interrupt
void
update_scheduler_analytics(void)
{
  int i; // Declare 'i' at the top of the function

  // 1. Calculate Current CPU Load
  if (tot_ticks > 0) {
    // Load = (total ticks - idle ticks) * 100 / total ticks
    cpu_load = ((tot_ticks - idle_ticks) * 100) / tot_ticks;
  } else {
    cpu_load = 0;
  }

  // --- Phase 4: Update Virtual Temperature ---
  // Apply heating based on current load (scaled by factor)
  virtual_temp += (cpu_load * HEATING_FACTOR) / 100; // Divide by 100 since load is %
  // Apply cooling
  virtual_temp -= COOLING_FACTOR;
  // Clamp to ambient temperature
  if (virtual_temp < AMBIENT_TEMP) {
    virtual_temp = AMBIENT_TEMP;
  }
  // --- End Phase 4 Temperature Update ---


  // 2. Update Moving Average History
  load_history[history_index] = cpu_load;
  history_index = (history_index + 1) % HISTORY_SIZE;

  // 3. Calculate Predicted Load (Moving Average)
  int total_load = 0;
  for (i = 0; i < HISTORY_SIZE; i++) {
    total_load += load_history[i];
  }
  predicted_load = total_load / HISTORY_SIZE;

  // 4. Dynamic Frequency Simulation (based on predicted load)
  enum freq_level next_frequency; // Temporary variable
  if (predicted_load > THRESH_MED_TO_HIGH) {
    next_frequency = HIGH;
  } else if (predicted_load > THRESH_LOW_TO_MED) {
    next_frequency = MEDIUM;
  } else {
    next_frequency = LOW;
  }

  // --- Phase 4: Apply Thermal Throttling ---
  // Override frequency decision if temperature is too high
  if (virtual_temp > TEMP_THROTTLE_LIMIT) {
    next_frequency = LOW; // Force LOW frequency regardless of predicted load
  }
  // --- End Phase 4 Throttling ---

  // Update the global frequency state
  current_frequency = next_frequency;


  // --- Phase 5: Adaptive Thresholds ---
  // Check for frequency change (oscillation detection)
  if (current_frequency != prev_frequency) {
    oscillation_count++;
    last_switch_tick = ticks;  // Use global ticks
    prev_frequency = current_frequency;
  }

  // Reset oscillation count if window expired
  if (ticks - last_switch_tick > OSCILLATION_WINDOW) {
    oscillation_count = 0;
  }

  // Adaptive logic: widen thresholds if oscillating
  if (oscillation_count >= MAX_OSCILLATION) {
    THRESH_LOW_TO_MED += 5;
    THRESH_MED_TO_HIGH += 5;
    if (THRESH_MED_TO_HIGH > 90) THRESH_MED_TO_HIGH = 90;
    if (THRESH_LOW_TO_MED > THRESH_MED_TO_HIGH - 10) THRESH_LOW_TO_MED = THRESH_MED_TO_HIGH - 10;
    oscillation_count = 0;  // Reset after adaptation
  }

  // Periodic tuning: narrow thresholds if stable and low load
  adaptation_counter++;
  if (adaptation_counter >= (ADAPTATION_PERIOD / LOAD_PERIOD)) {
    adaptation_counter = 0;
    if (oscillation_count == 0 && predicted_load < 20) {
      THRESH_LOW_TO_MED = THRESH_LOW_TO_MED > 20 ? THRESH_LOW_TO_MED - 2 : 20;
      THRESH_MED_TO_HIGH = THRESH_MED_TO_HIGH > 40 ? THRESH_MED_TO_HIGH - 2 : 40;
    }
  }
  // --- End Phase 5 ---

  // 5. Reset counters for the next period
  tot_ticks = 0;
  idle_ticks = 0;
}
// --- End of Phase 2 & 4 Logic ---


//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;

      // --- Our new code ---
      tot_ticks++;       // Increment total ticks
      if(is_idle)
        idle_ticks++;    // Increment idle ticks if scheduler is idle

      // Quantum enforcement: decrement current process's quantum
      struct proc *cur = myproc();
      if(cur && cur->state == RUNNING && cur->quantum_remaining > 0){
        cur->quantum_remaining--;
        if(cur->quantum_remaining == 0){
          yield();  // Time slice expired, yield CPU
        }
      }

      // Call our new scheduler logic periodically
      if(ticks % LOAD_PERIOD == 0)
        update_scheduler_analytics();
      // --- End of new code ---

      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
