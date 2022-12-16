#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "./../include/log_handle.h"
#define N_proc 6
#define my_log "./logs/master_log.txt"

// vector of all the pids 
pid_t pid[N_proc];

/*
  since the command console and the inspection process are spawned on 
  the konsole application, when a CTRL-C is used in the terminal, this signal 
  is received by all the other processes but not by these two, so a SIGTERM
  is sent to the two of them
*/
void sig_handler(int signo){
   switch (signo){
        case SIGINT:{ // termination signal for the process
            kill(pid[0],SIGTERM);
            file_logG(my_log,"Sent SIGTERM to console...");
            kill(pid[5],SIGTERM);
            file_logG(my_log,"Sent SIGTERM to inspection...");
            file_logS(my_log,signo);
        } break;
        default:
            break;
      }
}

int spawn(char **args_list, pid_t *list, int count){
  pid_t child_pid = fork();

  if (child_pid < 0){ // if fork fails
    for (size_t i = 0; i < count; i++)
      kill(list[i], SIGTERM); //kill all child processes that were correctly forked up to count-1
    file_logE(my_log,"fork");
  } else if (child_pid == 0){
    execvp(args_list[0], args_list);
    file_logE(my_log,"execvp");
  } else
    return child_pid;
}

int main(){

  // setup to receive SIGINT
  signal(SIGINT, sig_handler);

  //ret_pid: variable for the pid of the first process that ended
  
  pid_t ret_pid;

  /*
   status: it is used to store information about a child process exiting and check what happened
   exit_status: it is used to retrieve the actual value used in the exit() by the child process
   count: it is used for counting and killing the child processes in case of errors in the fork
   fdCm, fdIm: file descriptor for pipes between the master and the command console and the inspection
  */
  int status,exit_status, count = 0, fdCm, fdIm;

  /* 
  seq_pid: string containing the pids of M1 and M2, it is passaed to the inspection process as a
  command line parameter and used for the Stop/Reset functionalities

  msg: formatted string printed in the master's log file with the returned value
  of the first terminated child process
  */
  char seq_pid[25],msg[100];

  // path for the named fifos
  char *fifoCm = "/tmp/fifoCm";
  char *fifoIm = "/tmp/fifoIm";
  
  // matrices of strings for the spawn
  char * arg_console[] = {"konsole", "-e", "./bin/command_console",NULL};
  char * arg_M1[] = {"./bin/M1",NULL};
  char * arg_M2[] = {"./bin/M2",NULL};
  char * arg_real[] = {"./bin/real",NULL};
  char * arg_watchdog[] = {"./bin/watchdog",NULL};

  // update log file
  file_logG(my_log,"Program started...");

  // spawn of the processes and storing of their pids
  pid[0] = spawn(arg_console,pid,count);

  // create pipe for receiving the correct pid of the process, because of the use of konsole
  if(mkfifo(fifoCm, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo fifoCm");
  }

  //receive pid from the console process thru a pipe, check for errors on the sys calls
  fdCm = open(fifoCm,O_RDONLY);

  if(fdCm == -1 && errno !=EINTR){
    /*
      the unlink is not checked because every pipe is created such that if it already exists,
      because an unlink might have failed previously and there was no way to fix that, then the whole 
      system should not fail or be shut down because failing an unlink is not a critical problem
      for the correct behaviour of the hoist
    */
    unlink(fifoCm); 
    file_logE(my_log,"open fifoCm");
    }

  if(read(fdCm, &pid[0], sizeof(pid[0])) == -1){
      close(fdCm);
      unlink(fifoCm); 
      file_logE(my_log,"read fdCm");
    }
  close(fdCm);


  pid[1] = spawn(arg_M1,pid,count++); 
  pid[2] = spawn(arg_M2,pid,count++);
  pid[3] = spawn(arg_real,pid,count++);
  pid[4] = spawn(arg_watchdog,pid,count++);

  // string to be passed as an argument to the inspection process
  sprintf(seq_pid,"%d %d %d", pid[0],pid[1], pid[2]);

  char * arg_inspection[] = {"konsole", "-e", "./bin/inspection",seq_pid, NULL};
  pid[5] = spawn(arg_inspection,pid,count++);

   if(mkfifo(fifoIm, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo fifoIm");
  }

  //receive pid from the inspection process thru a pipe, check for errors on the sys calls
  fdIm = open(fifoIm,O_RDONLY);

  if(fdIm == -1 && errno !=EINTR){
    unlink(fifoIm); 
    file_logE(my_log,"open fifoIm");
    }

  if(read(fdCm, &pid[5], sizeof(pid[5])) == -1){
      close(fdIm);
      unlink(fifoIm); 
      file_logE(my_log,"read fdIm");
    }
  close(fdIm);

  // the master process waits for a child process to return a value
  ret_pid = wait(&status);

  // check whether the child process exited normally and obtain the exited value
  if(WIFEXITED(status)){
    exit_status = WEXITSTATUS(status);
  }

  /*
    if the master gets to this point of the code, either one of the child process
    failed because of a sys call error or it received a CTRL-C; in any case all the other
    child processes are killed by sending a SIGTERM
  */
  for (size_t i = 0; i < N_proc; i++){
    if(pid[i] != ret_pid){ // send SIGTERM to child processes still alive
      kill(pid[i], SIGTERM);
      file_logG(my_log,"sent sigterm...");
    }
  }

  sprintf(msg,"Main program ended with value: %d",exit_status);
  file_logG(my_log, msg);
  printf("%s\n",msg);


  return 0;
}
