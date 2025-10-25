typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;

// --- Our new struct for cpustat ---
struct cpustat {
  int load;
  int predicted_load;
  int frequency_level; // 0=LOW, 1=MEDIUM, 2=HIGH
  int temp;            // Will be 0 for now
  int thresh_low_med;
  int thresh_med_high;
};
