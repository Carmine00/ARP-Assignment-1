#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
  set of functions created to write on the log file of the processes
  1) file_logE: write on the log file an error message and terminate the program
  2) file_logS: write on the log file that a termination signal was received
  3) file_logG: general message to be written in the log file (ex: program started, ecc...
  4) logTime: it is used to retrieve the last time of modification of the logs file (used by the watchdog)
*/


void file_logE(char *my_log,char *msg){ // function to write on the log file if an error occurred
  
  FILE *fp; // file pointer to the log

  // variables to handle time
  char dt[20];
  struct tm tm;
  time_t current_time;

  current_time = time(NULL);
  tm = *localtime(&current_time);
  // creation of the timestamp
  strftime(dt,sizeof(dt), "%d/%m/%Y %H:%M:%S", &tm);

  // opening of the log file and in case of error exit -5
  fp = fopen(my_log,"a");
  if (fp == NULL)
    exit(-5);
  else{
      /*
      print in the log file a message (ex: function where the program failed) set by the caller of the function,
      the specific error and its code and the timestamp
      */
      fprintf(fp,"%s:%s (%d), %s\n", msg,strerror(errno), errno, dt);
      fclose(fp);
      exit(errno);
  }
}

void file_logS(char *my_log, int val){ // function to write on the log file if a termination signal
  
  FILE *fp; // file pointer to the log

  // variables to handle time
  char dt[20];
  struct tm tm;
  time_t current_time;

  current_time = time(NULL);
  tm = *localtime(&current_time);
  strftime(dt,sizeof(dt), "%d/%m/%Y %H:%M:%S", &tm);

  fp = fopen(my_log,"a");
  if (fp == NULL)
    exit(-5);
  else{
      fprintf(fp,"Received signal %d, %s\n", val, dt); 
      fclose(fp);
      exit(0); 
  }
}

void file_logG(char *my_log, char *msg){ // function to write on the log file to state what the process is doing
  
  FILE *fp; // file pointer to the log

  // variables to handle time
  char dt[20];
  struct tm tm;
  time_t current_time;

  current_time = time(NULL);
  tm = *localtime(&current_time);
  strftime(dt,sizeof(dt), "%d/%m/%Y %H:%M:%S", &tm);

  fp = fopen(my_log,"a");
  if (fp == NULL)
    exit(-5);
  else{
      fprintf(fp,"%s, %s\n", msg, dt);
      fclose(fp);
  }
}

int logTime(char *path){
  
  struct stat filestat;
  char val[10];
  int min;

  /* 
  the stat() function gets status information about a specified file and saves
  the information, in seconds since the Epoch in the struct stat variable; 
  the most recent status of change of the file is then obtained with the function 
  localtime that returns the time broken-down in parts (day, hour, minute ecc...)
   */
  if(stat(path, &filestat) != -1){
     strftime(val,sizeof(val),"%M",localtime(&filestat.st_ctim));
     sscanf(val,"%d",&min);
     return min;
  }else
    return -1;

}