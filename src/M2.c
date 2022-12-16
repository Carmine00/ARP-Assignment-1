#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> 
#include <signal.h>
#include "./../include/log_handle.h"
#define my_log "./logs/M2_log.txt"
#define Z_LIM 9

int vz = 0; // inital vertical velocity
int fdCM2, fdM2R; // file descriptor for named pipe console (w) - M2(r), M2(w) - real(r)
char * fifoCM2 = "/tmp/fifoCM2"; // named fifo console - M2
char * fifoM2R = "/tmp/fifoM2R"; // named fifo real - M2

void sighandlerM2(int signo){
  switch (signo){
        case SIGTERM:{ // termination signal for the process
            close(fdCM2);
            close(fdM2R);
            sleep(1);
            unlink(fifoCM2);
            unlink(fifoM2R);
            file_logG(my_log,"unlinking terminated");
            file_logS(my_log,signo);
        } break;
        case SIGINT:{ // termination signal for the process
            close(fdCM2);
            close(fdM2R);
            sleep(1);
            unlink(fifoCM2);
            unlink(fifoM2R);
            file_logG(my_log,"unlinking terminated");
            file_logS(my_log,signo);
        } break;
        case SIGUSR1:{ // setup velocity for Stop
            file_logG(my_log,"Received SIGUSR1!");
            vz = 0;
        } break;
        case SIGUSR2:{ // setup velocity for Reset
            file_logG(my_log,"Received SIGUSR2!");
            vz =-3;
        } break;
        default:
            break;
      }
}

int main(int argc, char const *argv[])
{
  // setup to receive SIGINT and SIGTERM
  signal(SIGTERM, sighandlerM2);
  signal(SIGINT, sighandlerM2);

  // sigaction is preferred for SIGUSR1 and SIGUSR2 because of the flags options
  struct sigaction sa1;
  /* set sa to zero using the memset() C library function */
  memset(&sa1, 0, sizeof(sa1)); // put zeros in some data area
  sa1.sa_handler = sighandlerM2;
  sa1.sa_flags = SA_RESTART;
  if(sigaction (SIGUSR1, &sa1, NULL) == -1){
    file_logE(my_log,"sigaction SIGUSR1");
  }

  struct sigaction sa2;
  sa2.sa_handler = sighandlerM2;
  sa2.sa_flags = SA_RESTART;
  if(sigaction (SIGUSR2, &sa2, NULL) == -1){
    file_logE(my_log,"sigaction SIGUSR2");
  }
    
  

  fd_set rfds;
  struct timeval tv;

  int retval,loop = 0;
  float z = 0.0, delta_t = 0.033; // f = 30 Hz
  char msg[100];

  // creation of the named fifo and check for errors
  if(mkfifo(fifoCM2, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo");
  }
  if(mkfifo(fifoM2R, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo");
  } 

  sleep(2);
  // update log file
  file_logG(my_log,"Program started...");
  


  while(1){

    fdCM2 = open(fifoCM2,O_RDONLY | O_NONBLOCK);
    if(fdCM2 == -1){
      unlink(fifoCM2); 
      unlink(fifoM2R);
      file_logE(my_log,"open fdCM2");
    }
    // Watch file descriptor for the console - M2 pipe to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(fdCM2, &rfds);

    /* Wait up to one second*/
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    retval = select(fdCM2+1, &rfds, NULL, NULL, &tv);

    if (retval == -1 && errno !=EINTR){
      close(fdCM2);
      unlink(fifoCM2); 
      unlink(fifoM2R);
      file_logE(my_log,"select");
    } else if (retval){ // new data has been sent
      if(read(fdCM2, &vz, sizeof(vz)) == -1){
        close(fdCM2);
        unlink(fifoCM2); 
        unlink(fifoM2R);
        file_logE(my_log,"read");
      }
      close(fdCM2); 
      if(vz!=0 && ((Z_LIM - z) >= 0 || vz < 0) && (z >= 0 || vz >= 0)){ 
       z = z + vz*delta_t*0.25; // if limit of the hoist are respected, then the motor is activated
       fdM2R = open(fifoM2R,O_WRONLY | O_NONBLOCK) ; 
       if(fdM2R == -1){
        unlink(fifoCM2);
        unlink(fifoM2R); 
        file_logE(my_log,"open fdM2R");
      } 
       if(write(fdM2R, &z, sizeof(z)) < 0){
        close(fdM2R);
        unlink(fifoCM2);
        unlink(fifoM2R);
        file_logE(my_log,"write");
       }
       close(fdM2R); 
       if(loop == 10){
          sprintf(msg, "Position update %f with velocity %d",z,vz);
          file_logG(my_log,msg);
          loop = 0;
        }else{
          loop++;
        }
      }
    }else{ // no new data has been sent, so mantain old velocity but check boundary
      close(fdCM2);
      if(vz!=0 && ((Z_LIM - z) >= 0 || vz < 0) && (z >= 0 || vz >= 0)){
        z = z + vz*delta_t*0.25;
        fdM2R = open(fifoM2R,O_WRONLY | O_NONBLOCK); 
        if(fdM2R == -1){
        unlink(fifoCM2);
        unlink(fifoM2R); 
        file_logE(my_log,"open fdM2R");
      } 
      if(write(fdM2R, &z, sizeof(z)) < 0){
        close(fdM2R);
        unlink(fifoCM2); 
        unlink(fifoM2R);
        file_logE(my_log,"write");
        }
        close(fdM2R); 
        if(loop == 10){
          sprintf(msg, "Position update %f with velocity %d",z,vz);
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



 