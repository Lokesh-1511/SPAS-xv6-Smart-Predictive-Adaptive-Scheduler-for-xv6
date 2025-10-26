#include "types.h"
#include "user.h"

// This array must match the one in proc.c
char *freq_str[] = { "LOW", "MEDIUM", "HIGH" };

int
main(int argc, char *argv[])
{
  struct cpustat st;
  int count = 0;

  // Loop for 10 iterations, printing stats every second
  while(count < 10) {
    if(cpustat(&st) < 0) {
      printf(2, "cpustat failed\n");
      exit();
    }

    // Print the report
    printf(1, "--- SPAS-xv6 Scheduler Status ---\n");
    printf(1, "CPU Load:     %d%%\n", st.load);
    printf(1, "Pred. Load:   %d%%\n", st.predicted_load);
    printf(1, "Frequency:    %s\n", freq_str[st.frequency_level]);
    // UPDATED LINE: Print temperature with one decimal place
    printf(1, "Virtual Temp: %d.%d C\n", st.temp / 10, st.temp % 10);
    printf(1, "Thresholds:   L->M %d%%, M->H %d%%\n", st.thresh_low_med, st.thresh_med_high);
    printf(1, "\n");

    sleep(100); // sleep for 100 ticks (1 second)
    count++;
  }

  exit();
}
