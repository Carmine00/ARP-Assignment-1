#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h> 
#include "./../include/inspection_utilities.h"
#include "./../include/log_handle.h"
#define my_log "./logs/inspection_log.txt"

int fdRI, fdIm,fdWI; // file descriptor for named pipe real(w) - inspection(w), master(r) - inspection(r), watchdog(r)-inspection(w)
int pid[3]; // pid of the console and motors (STOP button setting)
int flag = 0; // flag for the reset function
char * fifoRI = "/tmp/fifoRI"; // named fifo real - inspection
char * fifoWI = "/tmp/fifoWI"; // named fifo watchdog - inspection (just to send the pid)
char * fifoIm = "/tmp/fifoIm"; // named fifo master - inspection (just to send the pid)

void sig_handler(int signo){
   switch (signo){
        case SIGTERM:{ // termination signal for the process
            close(fdRI);
            sleep(1);
            unlink(fifoRI);
            file_logG(my_log,"unlinking terminated");
            file_logS(my_log,signo);
        } break;
        case SIGINT:{ // termination signal for the process
            close(fdRI);
            sleep(1);
            unlink(fifoRI);
            unlink(fifoWI);
            unlink(fifoIm);
            file_logG(my_log,"unlinking terminated");
            file_logS(my_log,signo);
        } break;
        case SIGUSR1:{ // launch reset if inspection receives this signal from the watchdog
            file_logG(my_log,"Received SIGUSR1 from watchdog");
            file_logG(my_log,"Reset activated...");
            kill(pid[0],SIGUSR2);
            file_logG(my_log,"sent SIGUSR2 to console");
            kill(pid[1],SIGUSR2);
            file_logG(my_log,"sent SIGUSR2 to M1");
            kill(pid[2],SIGUSR2);
            file_logG(my_log,"sent SIGUSR2 to M2");
            flag = 1;
        } break;
        default:
            break;
      }
}

int main(int argc, char const *argv[])
{
  // setup to receive SIGINT and SIGTERM
  signal(SIGTERM, sig_handler);
  signal(SIGINT, sig_handler);

  // sigaction is preferred for SIGUSR1 and SIGUSR2 because of the flags options
  struct sigaction sa1;
  /* set sa to zero using the memset() C library function */
  memset(&sa1, 0, sizeof(sa1)); // put zeros in some data area
  sa1.sa_handler = sig_handler;
  sa1.sa_flags = SA_RESTART;
  if(sigaction (SIGUSR1, &sa1, NULL) == -1){
    file_logE(my_log,"sigaction SIGUSR1");
  }

  // rdfs: variable containing the file descriptors to be monitored
  fd_set rfds;
  // tv: variable used to setup a timer for the select
  struct timeval tv;

  // retval: value returned from the select that needs tp be checked
  int retval;
  // the process does not update its action every time on the log file, but only every ten cycles
  int loop = 0;
  // pid to be sent to the watchdog
  pid_t my_pid;

  float pos, ee_x = 0.0, ee_z = 0.0; // End-effector coordinates

  /*
    id: contains the identifier character sent from the real process, so as to know whether
    it is necessary to work with ee_x or ee_y

    str: string sent by real containing id and the update position
  */
  char id, str[20];
  char msg[100];

  // check the correct number of command line parameters
  if(argc != 2){
    file_logE(my_log,"argc");
  }else{
    if(sscanf(argv[1], "%d %d %d", &pid[0],&pid[1], &pid[2]) !=3){
      file_logE(my_log,"sscanf");
    }
  }

  // creation of the named fifo and check for errors

  if(mkfifo(fifoIm, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo fifoIm");
  }

  if(mkfifo(fifoWI, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo fifoWI");
  }

  if(mkfifo(fifoRI, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo");
  }

  //send pid to the master process thru a pipe, check for errors on the sys calls

  fdIm = open(fifoIm,O_WRONLY);
 
  if(fdIm == -1 && errno !=EINTR){
    unlink(fifoIm);
    unlink(fifoWI);
    unlink(fifoRI);
    file_logE(my_log,"open fifoIm");
  }

  my_pid = getpid();

  if(write(fdIm, &my_pid, sizeof(my_pid)) < 0 && errno !=EINTR){
        close(fdIm);
        unlink(fifoIm);
        unlink(fifoWI);
        unlink(fifoRI);
        file_logE(my_log,"write fifoIm");
  }

  close(fdIm);

  //send pid to the watchdog process thru a pipe, check for errors on the sys calls

  fdWI = open(fifoWI,O_WRONLY);
 
  if(fdWI == -1 && errno !=EINTR){
    unlink(fifoRI);
    file_logE(my_log,"open fifoWI");
  }


  if(write(fdWI, &my_pid, sizeof(my_pid)) < 0 && errno !=EINTR){
        close(fdWI);
        unlink(fifoRI);
        file_logE(my_log,"write fifoWI");
  }

  close(fdWI);

  // Utility variable to avoid trigger resize event on launch
  int first_resize = TRUE;
  
  // Initialize User Interface 
  init_console_ui();

  // setup time to let all the processes start their functionalities (variables, pipe ecc...)
  sleep(2);

  // after the setup time, all the needed data should have been acquired and we can make the unlink
  unlink(fifoWI);
  unlink(fifoIm);

  // update log file
  file_logG(my_log,"Program started...");

  while(TRUE){
    // Get mouse/resize commands in non-blocking mode...
    int cmd = getch();

    // If user resizes screen, re-draw UI
    if(cmd == KEY_RESIZE) {
      if(first_resize) 
          first_resize = FALSE;
      else
          reset_console_ui();  
    } 
    
    // Else if mouse has been pressed 
    else if(cmd == KEY_MOUSE){ 
        // Check which button has been pressed...
        if(getmouse(&event) == OK) {
          // STOP button pressed
          if(check_button_pressed(stp_button, &event)) {
            mvprintw(LINES - 1, 1, "STP button pressed");
            refresh();
            sleep(1);
            for(int j = 0; j < COLS; j++){
              mvaddch(LINES - 1, j, ' ');
            }
            file_logG(my_log,"STP button pressed");
            if(kill(pid[0],SIGUSR1) == -1){ // check that the signal was actually sent
              unlink(fifoRI); 
              file_logE(my_log,"kill STP console");
            }else{
              file_logG(my_log,"sent SIGUSR1 to console");
            }
            if(kill(pid[1],SIGUSR1) == -1){
              unlink(fifoRI); 
              file_logE(my_log,"kill STP M1");
            }else{
               file_logG(my_log,"sent SIGUSR1 to M1");
            }
            if(kill(pid[2],SIGUSR1) == -1){
              unlink(fifoRI); 
              file_logE(my_log,"kill STP M2");
            }else{
              file_logG(my_log,"sent SIGUSR1 to M2");
            }
          } else if(check_button_pressed(rst_button, &event)){ // RESET button pressed
                    mvprintw(LINES - 1, 1, "RST button pressed");
                    refresh();
                    sleep(1);
                    for(int j = 0; j < COLS; j++){
                        mvaddch(LINES - 1, j, ' ');
                    }
                    file_logG(my_log,"RST button pressed");
                    if(kill(pid[0],SIGUSR2) == -1){
                      unlink(fifoRI); 
                      file_logE(my_log,"kill RST console");
                    }else{
                      file_logG(my_log,"sent SIGUSR2 to console");
                    }
                    if(kill(pid[1],SIGUSR2) == -1){
                      unlink(fifoRI); 
                      file_logE(my_log,"kill RST M1");
                    }else{
                      file_logG(my_log,"sent SIGUSR2 to M1");
                    }
                    if(kill(pid[2],SIGUSR2) == -1){
                      unlink(fifoRI); 
                      file_logE(my_log,"kill RST M2");
                    }else{
                      file_logG(my_log,"sent SIGUSR2 to M2");
                    }
                    flag = 1;
                    
             }
         }
    }

    // check whether new data from the real process has been received
    fdRI = open(fifoRI,O_RDONLY | O_NONBLOCK); 
    if(fdRI == -1){
      unlink(fifoRI);
      file_logE(my_log,"open");
    } 

    // setup for the select sys call
    FD_ZERO(&rfds);
    FD_SET(fdRI, &rfds);
    
    // timeout for incoming data
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    retval = select(fdRI + 1, &rfds, NULL, NULL, &tv);
    
    if (retval == -1 && errno !=EINTR){
        close(fdRI);
        unlink(fifoRI);
        file_logE(my_log,"select");
    }
    else if(retval){ // new data has been received
        if(read(fdRI, str, strlen(str)+1) == -1){
        close(fdRI);
        unlink(fifoRI); 
        file_logE(my_log,"read");
        }
        close(fdRI); 
        // retrieve from the string received the id of the coordinate and the new position
        sscanf(str, "%c %f", &id, &pos);
      switch (id){ // update position based on the id
        case 'x':
            ee_x = pos;
            break;
        case 'z':
            ee_z = pos;
            break;
        default:
            break;
      }
      // Update UI
      update_console_ui(&ee_x, &ee_z);
      if(loop == 10){ // update the lof file every ten iterations
          sprintf(msg, "Position update x %f z %f",ee_x,ee_z);
          file_logG(my_log,msg);
          loop = 0;
        }else{
          loop++;
        }
      }else{ // if no new data has been sent, close the unused pipe
        close(fdRI);
        sleep(1);
      }
      if(flag == 1 && ee_x == 0 && ee_z == 0){ // update the log at the end of the reset
        file_logG(my_log,"Reset ended");
        flag = 0;
      }
	 }

     // Terminate
    endwin();

    return 0;

}
