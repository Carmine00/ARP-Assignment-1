#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <signal.h>
#include "./../include/log_handle.h"
#define my_log "./logs/M1_log.txt"
#define X_LIM 39

int vx = 0; // inital velocity of the motor 
int fdCM1, fdM1R; // file descriptor for named pipe console (w) - M1(r), M1(w) - real (r)
char * fifoCM1 = "/tmp/fifoCM1"; // named fifo console - M1
char * fifoM1R = "/tmp/fifoM1R"; // named fifo M1 - real

void sighandlerM1(int signo){
   switch (signo){
        case SIGTERM:{ // termination signal for the process
            close(fdCM1);
            close(fdM1R);
            sleep(1);
            unlink(fifoCM1);
            unlink(fifoM1R);
            file_logG(my_log,"unlinking terminated");
            file_logS(my_log, signo);
        } break;
        case SIGINT:{ // termination signal for the process
            close(fdCM1);
            close(fdM1R);
            sleep(1);
            unlink(fifoCM1);
            unlink(fifoM1R);
            file_logG(my_log,"unlinking terminated");
            file_logS(my_log, signo);
        } break;
        case SIGUSR1:{ // setup velocity for Stop
            file_logG(my_log,"Received SIGUSR1 (Stop)!");
            vx = 0;
        } break;
        case SIGUSR2:{ // setup velocity for Reset
            file_logG(my_log,"Received SIGUSR2 (Reset)!");
            vx = -3;
        } break;
        default:
            break;
      }
}

int main(int argc, char const *argv[])
{
  // setup to receive SIGINT and SIGTERM
  signal(SIGTERM, sighandlerM1);
  signal(SIGINT, sighandlerM1);


  // sigaction is preferred for SIGUSR1 and SIGUSR2 because of the flags options
  struct sigaction sa1;
  /* set sa to zero using the memset() C library function */
  memset(&sa1, 0, sizeof(sa1)); // put zeros in some data area
  sa1.sa_handler = sighandlerM1;
  sa1.sa_flags = SA_RESTART;
  if(sigaction (SIGUSR1, &sa1, NULL) == -1){
    file_logE(my_log,"sigaction SIGUSR1");
  }

  struct sigaction sa2;
  sa2.sa_handler = sighandlerM1;
  sa2.sa_flags = SA_RESTART;
  if(sigaction (SIGUSR2, &sa2, NULL) == -1){
    file_logE(my_log,"sigaction SIGUSR2");
  }

  fd_set rfds;
  struct timeval tv;
  pid_t my_pid = getpid();

  int retval, loop = 0;
  float x = 0.0, delta_t = 0.033; // f = 30 Hz
  char msg[100];

  // creation of the named fifo and check for errors

  if(mkfifo(fifoCM1, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo");
  } 
  if(mkfifo(fifoM1R, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo");
  } 

  // setupt time
  sleep(2);

  while(1){

    fdCM1 = open(fifoCM1,O_RDONLY | O_NONBLOCK);
 
    if(fdCM1 == -1 && errno !=EINTR){
      unlink(fifoCM1); 
      unlink(fifoM1R);
      file_logE(my_log,"open fdCM1");
    }

    // Watch file descriptor for the console - M1 pipe to see when it has input.
    FD_ZERO(&rfds);
    FD_SET(fdCM1, &rfds);
    // Wait up to one second. 
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    retval = select(fdCM1+1, &rfds, NULL, NULL, &tv);
    if (retval == -1 && errno !=EINTR){
      close(fdCM1);
      unlink(fifoCM1); 
      unlink(fifoM1R);
      file_logE(my_log,"select");
    }else if(retval){ // new data has been received
      if(read(fdCM1, &vx, sizeof(vx)) == -1 && errno !=EINTR){
        close(fdCM1);
        unlink(fifoCM1); 
        unlink(fifoM1R);
        file_logE(my_log,"read");
      }
      close(fdCM1); 
      if(vx!=0 && ((X_LIM - x) >= 0 || vx < 0) && (x >= 0 || vx >= 0)){
        /*
          boundary, do not update position (aka set motor): if velocity is equal to 0,
          the difference between the max value (X_LIM) and the actual position (x) is
          not greater than 0 or the velocity is less than zero, to allow the hoist 
          to move backward, or last if the position is not greater or equal than 0 
        */
        x = x + vx*delta_t; // if limit of the hoist are respected, then the motor is activated
        // send new position to the real process
        fdM1R = open(fifoM1R,O_WRONLY | O_NONBLOCK);
        if(fdM1R == -1 && errno !=EINTR){
          unlink(fifoCM1);
          unlink(fifoM1R); 
          file_logE(my_log,"open fdM1R");
        } 
        if(write(fdM1R, &x, sizeof(x)) < 0 && errno !=EINTR){
          close(fdM1R);
          unlink(fifoCM1); 
          unlink(fifoM1R);
          file_logE(my_log,"write");
        }
        close(fdM1R);
        if(loop == 10){
          sprintf(msg, "Position update %f with velocity %d",x,vx);
          file_logG(my_log,msg);
          loop = 0;
        }else{
          loop++;
        }
      }
    }else{ // no new data has been sent, so mantain old velocity but check boundary
          close(fdCM1);
          if(vx!=0 && ((X_LIM - x) >= 0 || vx < 0) && (x >= 0 || vx >= 0)){
            x = x + vx*delta_t;
            // send new position to the real process
            fdM1R = open(fifoM1R,O_WRONLY | O_NONBLOCK);
            if(fdM1R == -1 && errno !=EINTR){
              unlink(fifoCM1);
              unlink(fifoM1R); 
              file_logE(my_log,"open fdM1R");
            } 
            if(write(fdM1R, &x, sizeof(x)) < 0 && errno !=EINTR){
              close(fdM1R);
              unlink(fifoCM1); 
              unlink(fifoM1R);
              file_logE(my_log,"write");
            }
            close(fdM1R); 
            if(loop == 10){
              sprintf(msg, "Position update %f with velocity %d",x,vx);
              file_logG(my_log,msg);
              loop = 0;
            }else{
                loop++;
            }
          }
        }
        sleep(delta_t);
  }
    return 0;
}


    
