#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include "./../include/log_handle.h"
#define TSLEEP 60
#define my_log "./logs/watchdog_log.txt"

void sig_endhandler(int signo){
  if(signo == SIGINT){ // termination signal for the process
     file_logS(my_log,signo);
  }else if(signo == SIGTERM){ // termination signal for the process
     file_logS(my_log,signo);
  }else
    file_logE(my_log,"sig_endhandler");
}


int main(){

  // setup to receive SIGINT and SIGTERM
  signal(SIGINT, sig_endhandler);
  signal(SIGTERM, sig_endhandler);

  /* 
  count: check the number of processes that have done nothing in 1 minute
  min: current minute when the watchdog is ready again (after the 1 minute sleep)
  log_min: last minute the log file of a process was modified
  fdWI: file descriptor for the watchdog - inspection pipe
  */
  int count, min, log_min, fdWI;

  // variable for the inspection pid
  pid_t ins_pid;

  /*
    curr_min: string used to retrieve the current minute
    min_Log: string used to retrieve the last minute modification of the log file
    log_list: matrices of string used to check the log files
  */
  char curr_min[10], minLog[10],*log_list[] = {"./logs/console_log.txt","./logs/M1_log.txt","./logs/M2_log.txt","./logs/real_log.txt",
  "./logs/inspection_log.txt"};

  // fifo watchdog - inspection
  char * fifoWI = "/tmp/fifoWI";

  // variables used to get the time
  struct tm tm;
  time_t current_time;

  // creation of the named fifo and check for errors

  if(mkfifo(fifoWI, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo fifoWI");
  }

  fdWI = open(fifoWI,O_RDONLY);
 
  if(fdWI == -1 && errno !=EINTR){
    unlink(fifoWI); 
    file_logE(my_log,"open fifoWI");
  }

  if(read(fdWI, &ins_pid, sizeof(ins_pid)) == -1){
     unlink(fifoWI); 
     file_logE(my_log,"read fifoWI");
  }


  close(fdWI);
  // update log file
  file_logG(my_log,"Program started...");

  

  while(1){
    count = 0;

    sleep(TSLEEP); // sleep for 1 min
    file_logG(my_log,"Woke up, begin to check..."); //update log

    // obtain current time
    current_time = time(NULL);
    tm = *localtime(&current_time);
    strftime(curr_min,sizeof(curr_min), "%M", &tm); // obtain current minute
    sscanf(curr_min,"%d",&min);

    for(int i = 0; i < 5; i++){ // check log files minute
      log_min = logTime(log_list[i]); // use custom function to obtain the minute of modification
      if(log_min == -1){
        file_logE(my_log,"stat");
      }
      if(min - log_min > 0){
        count++;
      }
      sleep(3);
    }
    if(count == 5){
      file_logG(my_log,"Reset setup launched...");
      if(kill(ins_pid, SIGUSR1) == -1){ // send signal to inspection to launch Reset functionality and check for errors
        file_logE(my_log,"kill RST inspection");
      }else{
        file_logG(my_log,"Signal sent to inspection...");
      }
    }else{
       file_logG(my_log,"Reset postponed...");
    }
  }


  return 0;
}
