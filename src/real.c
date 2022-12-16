#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> 
#include <signal.h>
#include <string.h>
#include <math.h>
#include "./../include/log_handle.h"
#define my_log "./logs/real_log.txt"
#define ENGINE_ERR 0.01 // error coefficient of 1% of the value on each position

int fdM1R, fdM2R, fdRI; // file descriptor for named pipe console M1(w) - real (r), M2(w) - real(r), real(w) - inspection(r)
const char * fifoM1R = "/tmp/fifoM1R"; // named fifo M1 - real
const char * fifoM2R = "/tmp/fifoM2R"; // named fifo M2 - real
const char * fifoRI = "/tmp/fifoRI"; // named fifo real - inspection

void sig_handler(int signo){
   switch (signo){
        case SIGTERM:{ // termination signal for the process
            close(fdM1R);
            close(fdM2R);
            close(fdRI);
            sleep(1);
            unlink(fifoM1R);
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logG(my_log,"unlinking terminated");
            file_logS(my_log,signo);
        } break;
        case SIGINT:{ // termination signal for the process
            close(fdM1R);
            close(fdM2R);
            close(fdRI);
            sleep(1);
            unlink(fifoM1R);
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logG(my_log,"unlinking terminated");
            file_logS(my_log,signo);
        } break;
        default:
            break;
      }
}


int main(int argc, char const *argv[]){

  // setup to receive SIGINT and SIGTERM
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  
  fd_set rfds;
  struct timeval tv;

  // rand_val: it is used to randomly select a file descriptor
  int retval, rand_val, loop = 0;
  float x = 0.0, z = 0.0;

  char id_x = 'x', id_z = 'z', str[20], msg[100];

  // creation of the named fifo and check for errors

  if(mkfifo(fifoM1R, S_IRWXU) != 0 && errno!=EEXIST){
     file_logE(my_log,"mkfifo");
  }

  if(mkfifo(fifoM2R, S_IRWXU) != 0 && errno!=EEXIST){
     file_logE(my_log,"mkfifo");
  }

  if(mkfifo(fifoRI, S_IRWXU) != 0 && errno!=EEXIST){
     file_logE(my_log,"mkfifo");
  }

  sleep(2);
  // update log file
  file_logG(my_log,"Program started...");
  
  while(1){

    // open pipe with the motors and check for mistakes

    fdM1R = open(fifoM1R,O_RDONLY | O_NONBLOCK);
    if(fdM1R == -1){
      unlink(fifoM1R); 
      unlink(fifoM2R);
      unlink(fifoRI);
      file_logE(my_log,"open");
    }

    fdM2R = open(fifoM2R,O_RDONLY | O_NONBLOCK);
    if(fdM2R == -1){
      unlink(fifoM1R); 
      unlink(fifoM2R);
      unlink(fifoRI);
      file_logE(my_log,"open");
    }

    // set a variable to a random value in between the two file descriptors
    rand_val = rand() % (((fdM1R > fdM2R) ? fdM1R: fdM2R)*2 - 0) + 0;

    FD_ZERO(&rfds);
    FD_SET(fdM1R, &rfds);
    FD_SET(fdM2R, &rfds);
    
    // timeout for incoming data
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    // find maxinum between file descriptor pipe 1 and pipe 2
    retval = select((fdM1R > fdM2R) ? fdM1R + 1 : fdM2R + 1, &rfds, NULL, NULL, &tv);
    
    if (retval == -1 && errno !=EINTR){
      close(fdM1R);
      close(fdM2R);
      unlink(fifoM1R); 
      unlink(fifoM2R);
      unlink(fifoRI);
      file_logE(my_log,"select");
    }else if(retval){ 
      if(FD_ISSET(fdM1R, &rfds) && FD_ISSET(fdM2R, &rfds)){ // if both files have data to be read, then select randomly one
        if(abs(rand_val - fdM1R) < abs(rand_val - fdM2R)){ // check "distance" between a random generated number and the file descriptor values
          if(read(fdM1R, &x, sizeof(x)) == -1){ // case fdM1R ready before fdM2R
            close(fdM1R);
            close(fdM2R);
            unlink(fifoM1R); 
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"read");
        }
        close(fdM1R); 
        // compute error on the position
        x += ENGINE_ERR*x;
        // prepare strinf to be sent to the inspection process
        sprintf(str, "%c %f", id_x, x);
        fdRI = open(fifoRI,O_WRONLY);
        if(fdRI == -1){
          close(fdM2R);
          unlink(fifoM1R); 
          unlink(fifoM2R);
          unlink(fifoRI);
          file_logE(my_log,"open");
        }
        if(write(fdRI, str, strlen(str)+1) < 0){
          close(fdM2R);
          close(fdRI);
          unlink(fifoM1R); 
          unlink(fifoM2R);
          unlink(fifoRI);
          file_logE(my_log,"write");
        }
        close(fdRI);
        if(read(fdM2R, &z, sizeof(z)) == -1){
          close(fdM2R);
          close(fdRI);
          unlink(fifoM1R); 
          unlink(fifoM2R);
          unlink(fifoRI);
          file_logE(my_log,"read");
        }
        close(fdM2R); 
        // compute error on the position
        z += ENGINE_ERR*z;
        sprintf(str, "%c %f", id_z, z);
        fdRI = open(fifoRI,O_WRONLY);
        if(fdRI == -1){
          unlink(fifoM1R); // DA CONTROLLARE
          unlink(fifoM2R);
          unlink(fifoRI);
          file_logE(my_log,"open");
        }
        if(write(fdRI, str, strlen(str)+1) < 0){
          close(fdRI);
          unlink(fifoM1R); // DA CONTROLLARE
          unlink(fifoM2R);
          unlink(fifoRI);
          file_logE(my_log,"write");

        }
        close(fdRI); //MANCA CONTROLLO
        if(loop == 10){
          sprintf(msg, "Position updated x %f z %f",x,z);
          file_logG(my_log,msg);
          loop = 0;
        }else{
          loop++;
        }
      } 
        else{ // case fdM2R ready before fdM1R
          if(read(fdM2R, &z, sizeof(z)) == -1){
              close(fdM1R);
              close(fdM2R);
              unlink(fifoM1R); 
              unlink(fifoM2R);
              unlink(fifoRI);
              file_logE(my_log,"read");
          }
          close(fdM2R); 
          // compute error on the position
          z += ENGINE_ERR*z;
          sprintf(str, "%c %f", id_z, z);
          fdRI = open(fifoRI,O_WRONLY);
          if(fdRI == -1){
            close(fdM1R);
            unlink(fifoM1R); 
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"open");
          }
          if(write(fdRI, str, strlen(str)+1) < 0){
            close(fdM1R);
            close(fdRI);
            unlink(fifoM1R); 
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"write");
          }
          close(fdRI);
          if(read(fdM1R, &x, sizeof(x)) == -1){
            close(fdM1R);
            close(fdRI);
            unlink(fifoM1R); 
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"read");
        }
          close(fdM1R); 
          // compute error on the position
          x += ENGINE_ERR*x;
          sprintf(str, "%c %f", id_x, x);
          fdRI = open(fifoRI,O_WRONLY);
          if(fdRI == -1){
            unlink(fifoM1R);
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"open");
          }
          if(write(fdRI, str, strlen(str)+1) < 0){
            close(fdRI);
            unlink(fifoM1R); 
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"write");
          }
          close(fdRI); //MANCA CONTROLLO
          if(loop == 10){
          sprintf(msg, "Position updated x %f z %f",x,z);
          file_logG(my_log,msg);
          loop = 0;
          }else{
            loop++;
          }
        }
        sleep(2);
    } // if not both have data aivalable 
    else if(FD_ISSET(fdM1R, &rfds)){ // check if only the first is ready
        close(fdM2R);
        if(read(fdM1R, &x, sizeof(x)) == -1){
          close(fdM1R);
          close(fdM2R);
          unlink(fifoM1R);
          unlink(fifoM2R);
          unlink(fifoRI);
          file_logE(my_log,"read");
        }
        close(fdM1R); 
        // error on the position
        x += ENGINE_ERR*x;
        sprintf(str, "%c %f\n", id_x, x);
        if(fdRI = open(fifoRI,O_WRONLY) == -1){
            unlink(fifoM1R); 
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"open");
        }
        if(write(fdRI, str, strlen(str)+1) < 0){
            close(fdRI);
            unlink(fifoM1R); 
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"write");
        }
        close(fdRI); 
        if(loop == 10){
          sprintf(msg, "Position updated x %f",x);
          file_logG(my_log,msg);
          loop = 0;
        }else{
          loop++;
        }
      }
    else if(FD_ISSET(fdM2R, &rfds)){ // check if only the second is ready
        close(fdM1R);
        if(read(fdM2R, &z, sizeof(z)) == -1){
          close(fdM2R);
          unlink(fifoM1R); 
          unlink(fifoM2R);
          unlink(fifoRI);
          file_logE(my_log,"read");
        }
        close(fdM2R); 
        // error on the position
        z += ENGINE_ERR*z;
        sprintf(str, "%c %f", id_z, z);
        if(fdRI = open(fifoRI,O_WRONLY) == -1){
            unlink(fifoM1R); 
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"open");
        }
        if(write(fdRI, str, strlen(str)+1) < 0){
            close(fdRI);
            unlink(fifoM1R); 
            unlink(fifoM2R);
            unlink(fifoRI);
            file_logE(my_log,"write");
        }
        close(fdRI); 
        if(loop == 10){
          sprintf(msg, "Position updated z %f",z);
          file_logG(my_log,msg);
          loop = 0;
        }else{
          loop++;
        }
    
      } 
    }else{ // no data is available
      close(fdRI);
      sleep(2);
    }
  }

  return 0;
}