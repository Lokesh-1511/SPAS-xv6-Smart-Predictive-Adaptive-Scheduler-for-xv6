#include "types.h"
#include "stat.h"
#include "user.h"

// Small test program for SPAS scheduler
// - forks `NCHILD` CPU-bound workers
// - parent periodically calls cpustat() and prints scheduler state
// Usage: spas_test [nchildren]

#define DEFAULT_CHILDREN 4
#define REPORTS 20

int
main(int argc, char *argv[])
{
  int n = DEFAULT_CHILDREN;
  if(argc > 1) n = atoi(argv[1]);
  if(n <= 0) n = DEFAULT_CHILDREN;

  printf(1, "spas_test: starting %d busy children\n", n);

  int pids[n];
  int i;

  for(i = 0; i < n; i++){
    int pid = fork();
    if(pid < 0){
      printf(2, "spas_test: fork failed\n");
      exit();
    }
    if(pid == 0){
      // Child: busy-loop for a while, then exit.
      volatile unsigned long x = 0;
      // Work amount chosen to keep CPU busy for a few seconds total.
      unsigned long work = 20000000UL; // adjust if needed
      while(x < work){
        x++;
        // Do not call I/O here to avoid serial contention.
      }
      // Child done
      exit();
    }
    pids[i] = pid;
  }

  // Parent: print cpustat reports
  struct cpustat st;
  int rep;
  for(rep = 0; rep < REPORTS; rep++){
    if(cpustat(&st) < 0){
      printf(2, "spas_test: cpustat failed\n");
      break;
    }
    printf(1, "--- SPAS Test Report %d/%d ---\n", rep+1, REPORTS);
    printf(1, "CPU Load:       %d%%\n", st.load);
    printf(1, "Predicted Load: %d%%\n", st.predicted_load);
    char *freq[] = { "LOW", "MEDIUM", "HIGH" };
    int fl = st.frequency_level;
    if(fl < 0 || fl > 2) fl = 1;
    printf(1, "Frequency:      %s\n", freq[fl]);
    printf(1, "Virtual Temp:   %d.%d C\n", st.temp/10, st.temp%10);
    printf(1, "Thresholds:     L->M %d%%, M->H %d%%\n", st.thresh_low_med, st.thresh_med_high);

    // Print remaining children (basic check)
    printf(1, "Children PIDs: ");
    for(i = 0; i < n; i++){
      printf(1, "%d ", pids[i]);
    }
    printf(1, "\n");

    // Guidance: what to look for
    if(rep == 0){
      printf(1, "Note: watch Frequency and Virtual Temp — with busy children Frequency should increase and temp should rise.\n");
      printf(1, "If Frequency moves LOW->MEDIUM->HIGH as load rises, dynamic time quanta assignment is active.\n");
    }

    sleep(100); // sleep 1 second
  }

  // Wait for children to finish
  for(i = 0; i < n; i++){
    wait();
  }

  printf(1, "spas_test: all children exited, done.\n");
  exit();
}
