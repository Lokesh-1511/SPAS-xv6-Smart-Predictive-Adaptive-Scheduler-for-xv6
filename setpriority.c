#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid, priority;

  if(argc < 3){
    printf(2, "Usage: setpriority pid priority\n");
    exit();
  }

  pid = atoi(argv[1]);
  priority = atoi(argv[2]);

  if(setpriority(pid, priority) < 0){
    printf(2, "setpriority failed\n");
    exit();
  }

  printf(1, "Set priority of process %d to %d\n", pid, priority);
  exit();
}