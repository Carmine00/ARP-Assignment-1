#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h> 
#include <signal.h>
#include "./../include/command_utilities.h"
#include "./../include/log_handle.h"
#define my_log "./logs/console_log.txt"
#define VMAX 3 // maxinum limit for the velocity
#define VMIN -3 // minimum limit for the velocity

int fdCM1, fdCM2, fdCm; // file descriptor for named pipe console(w) - M1(r), console(w) - M2(r), console(w) - master(r)
int vx = 0, vz = 0; // global variable to set the velocities of the hoist
char * fifoCM1 = "/tmp/fifoCM1"; // named fifo console - M1
char * fifoCM2 = "/tmp/fifoCM2"; // named fifo console - M2
char * fifoCm = "/tmp/fifoCm"; // named fifo console - master (just to send the pid of the process)

void sig_handler(int signo){
   switch (signo){
        case SIGTERM:{ // termination signal for the process
            close(fdCM1);
            close(fdCM2);
            /*
            set a sleep time to allow all processes to close the pipes and make the unlink successful, otherwise
            some processes could be unlinking the pipe while others are still closing it
            */
            sleep(1);
            unlink(fifoCM1);
            unlink(fifoCM2);
            file_logG(my_log,"unlinking terminated");
            // write in the log file the signal that was received
            file_logS(my_log,signo);
        } break;
        case SIGINT:{ // termination signal for the process
            close(fdCM1);
            close(fdCM2);
            sleep(1);
            unlink(fifoCM1);
            unlink(fifoCM2);
            unlink(fifoCm); // make this unlink as well in case the process needs to be closed while starting
            file_logG(my_log,"unlinking terminated");
            file_logS(my_log,signo);
        } break;
        case SIGUSR1:{ // signal for the Stop functionality
            // set velocity to the inital values
            vx = 0;
            vz = 0;
            // update log file
            file_logG(my_log,"Received SIGUSR1 (Stop)!");
        } break;
        case SIGUSR2:{ // signal for the Reset functionality
            // reset values for the velocity to have a clean restart when a new command is sent
            vx = 0;
            vz = 0;
            // update log file
            file_logG(my_log,"Received SIGUSR2 (Reset)!");
        } break;
        default:
            break;
      }
}

int main(int argc, char const *argv[])
{
  // setup to receive SIGINT and SIGTERM
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  // sigaction is preferred for SIGUSR1 and SIGUSR2 because of the flags options
  struct sigaction sa1;
  // set sa to zero using the memset() C library function 
  memset(&sa1, 0, sizeof(sa1)); // put zeros in some data area
  sa1.sa_handler = sig_handler;
  // this flag allow the process to restart a sys call in case it was interrupted because of a signal
  sa1.sa_flags = SA_RESTART;
  if(sigaction (SIGUSR1, &sa1, NULL) == -1){
    file_logE(my_log,"sigaction SIGUSR1");
  }

  struct sigaction sa2;
  /* set sa to zero using the memset() C library function */
  memset(&sa2, 0, sizeof(sa2)); // put zeros in some data area
  sa2.sa_handler = sig_handler;
  sa2.sa_flags = SA_RESTART;
  if(sigaction (SIGUSR2, &sa2, NULL) == -1){
    file_logE(my_log,"sigaction SIGUSR2");
  }

  // msg: message to be sent to other processes or to be written in the log file
  char msg[100];
  // pid to be sent to inspection
  pid_t my_pid;

  // creation of the named fifo and check for errors

  if (mkfifo(fifoCm, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo fifoCm");
  } 

  if (mkfifo(fifoCM1, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo fifoCM1");
  } 
  
  if (mkfifo(fifoCM2, S_IRWXU) != 0 && errno!=EEXIST){
    file_logE(my_log,"mkfifo fifoCM2");
  }

  // send the pid to the master process and for each sys call check for errors
  fdCm = open(fifoCm,O_WRONLY);
 
  if(fdCm == -1 && errno !=EINTR){
    unlink(fifoCM1); 
    unlink(fifoCM2); 
    unlink(fifoCm); 
    file_logE(my_log,"open fifoCm");
    }
  
  my_pid = getpid();

  if(write(fdCm, &my_pid, sizeof(my_pid)) < 0 && errno !=EINTR){
    close(fdCm);
    unlink(fifoCM1); 
    unlink(fifoCM2); 
    unlink(fifoCm); 
    file_logE(my_log,"write fifoCm");
    }

  close(fdCm);
  
  // Utility variable to avoid trigger resize event on launch
  int first_resize = TRUE;

  // Initialize User Interface 
  init_console_ui();

  // setup time to let all the processes start their functionalities (variables, pipe ecc...)
  sleep(2);

   /*
      the command console is responsible to unlink the named pipe with the master and the unlink is not checked
      because every pipe is created such that if it already exists, since an unlink might have failed previously 
      and there was no way to fix that, then the whole system should not fail or be shut down because 
      failing an unlink is not considered a critical problem for the correct behaviour of the hoist
    */
  unlink(fifoCm);

  // update log file
  file_logG(my_log,"Program started...");
    
  while(TRUE){	
    // Get mouse/resize commands in non-blocking mode...
    int cmd = getch();

    // If user resizes screen, re-draw UI
    if(cmd == KEY_RESIZE){
        if(first_resize){
            first_resize = FALSE;
        }
        else{
          reset_console_ui();
        }
    } 
    // Else if mouse has been pressed
    else if(cmd == KEY_MOUSE){ 

        // Check which button has been pressed...
        if(getmouse(&event) == OK) {

          // Vx-- button pressed
          if(check_button_pressed(vx_decr_btn, &event)){
            if(vx != VMIN){ // check minimum limit
              vx--;
              // send new value to M1, check sys calls
              fdCM1 = open(fifoCM1, O_WRONLY); 

              if(fdCM1 == -1){
                unlink(fifoCM1); 
                file_logE(my_log,"open");
              }

              if(write(fdCM1, &vx, sizeof(vx)) < 0){
                close(fdCM1);
                unlink(fifoCM1);
                file_logE(my_log,"write");
              }

              close(fdCM1); // close the pipe once it is not needed anymore
              sprintf(msg, "Horizontal velocity update %d",vx);
              file_logG(my_log,msg);
              mvprintw(LINES - 1, 1, "Horizontal Speed Decreased");
              refresh();
              sleep(1);
              for(int j = 0; j < COLS; j++){
                mvaddch(LINES - 1, j, ' ');
              }
            }else{
              mvprintw(LINES - 1, 1, "Minimum horizontal velocity reached");
              refresh();
              sleep(1);
              for(int j = 0; j < COLS; j++){
                mvaddch(LINES - 1, j, ' ');
              } // limit reached
              file_logG(my_log,"Minimum horizontal velocity reached");
            }
          }

          // Vx++ button pressed
          else if(check_button_pressed(vx_incr_btn, &event)){ 
                    if(vx != VMAX){ // check maxinum limit
                      vx++;
                      fdCM1 = open(fifoCM1, O_WRONLY);

                      if (fdCM1 == -1){
                        unlink(fifoCM1); 
                        file_logE(my_log,"open");

                      }

                      if(write(fdCM1, &vx, sizeof(vx)) < 0){
                        close(fdCM1);
                        unlink(fifoCM1); 
                        file_logE(my_log,"write");
                      }

                      close(fdCM1); // close the pipe once it is not needed anymore
                      sprintf(msg, "Horizontal velocity update %d",vx);
                      file_logG(my_log,msg);
                      mvprintw(LINES - 1, 1, "Horizontal speed increased");
                      refresh();
                      sleep(1);
                      for(int j = 0; j < COLS; j++){
                         mvaddch(LINES - 1, j, ' ');
                      }
                    }else{
                      mvprintw(LINES - 1, 1, "Maxinum horizontal velocity reached");
                      refresh();
                      sleep(1);
                      for(int j = 0; j < COLS; j++){
                       mvaddch(LINES - 1, j, ' ');
                      } // limit reached
                      file_logG(my_log,"Maximum horizontal velocity reached");
                    } 
           }

          // Vx stop button pressed
          else if(check_button_pressed(vx_stp_button, &event)){ 
                      vx = 0;
                      fdCM1 = open(fifoCM1, O_WRONLY );

                      if(fdCM1 == -1){
                        unlink(fifoCM1); 
                        file_logE(my_log,"open");
                      }

                      if(write(fdCM1, &vx, sizeof(vx)) < 0){
                        close(fdCM1);
                        unlink(fifoCM1); 
                        file_logE(my_log,"write");
                      }

                      close(fdCM1); // close the pipe once it is not needed anymore
                      file_logG(my_log,"Horizontal velocity stopped");
                      mvprintw(LINES - 1, 1, "Horizontal Motor Stopped");
                      refresh();
                      sleep(1);
                      for(int j = 0; j < COLS; j++){
                       mvaddch(LINES - 1, j, ' ');
                      }
           }

          else if(check_button_pressed(vz_decr_btn, &event)){ // Vz-- button pressed
                    if(vz != VMIN){
                      vz--;
                      fdCM2 = open(fifoCM2, O_WRONLY);

                      if (fdCM2 == -1){
                        unlink(fifoCM2); 
                        file_logE(my_log,"open");
                      }

                      if(write(fdCM2, &vz, sizeof(vz)) < 0){
                        close(fdCM2);
                        unlink(fifoCM2); 
                        file_logE(my_log,"write");
                      }
                      
                      close(fdCM2); // close the pipe once it is not needed anymore
                      sprintf(msg, "Vertical velocity update %d",vz);
                      file_logG(my_log,msg);
                      mvprintw(LINES - 1, 1, "Vertical speed decreased");
                      refresh();
                      sleep(1);
                      for(int j = 0; j < COLS; j++){
                         mvaddch(LINES - 1, j, ' ');
                      }
                    }else{
                      mvprintw(LINES - 1, 1, "Minimum vertical velocity reached");
                      refresh();
                      sleep(1);
                      for(int j = 0; j < COLS; j++){
                       mvaddch(LINES - 1, j, ' ');
                      }
                      file_logG(my_log,"Minimum vertical velocity reached");
                    }
          }
          
          // Vz++ button pressed
          else if(check_button_pressed(vz_incr_btn, &event)){ 
                    if(vz != VMAX){
                      vz++;
                      fdCM2 = open(fifoCM2, O_WRONLY );  

                      if(fdCM2 == -1){
                        unlink(fifoCM2); 
                        file_logE(my_log,"open");
                      }

                      if(write(fdCM2, &vz, sizeof(vz)) < 0){
                        close(fdCM2);
                        unlink(fifoCM2); 
                        file_logE(my_log,"write");
                      }

                      close(fdCM2); // close the pipe once it is not needed anymore
                      sprintf(msg, "Vertical velocity update %d",vz);
                      file_logG(my_log,msg);
                      mvprintw(LINES - 1, 1, "Vertical speed increased");
                      refresh();
                      sleep(1);
                      for(int j = 0; j < COLS; j++){
                         mvaddch(LINES - 1, j, ' ');
                      }
                      
                    }else{
                      mvprintw(LINES - 1, 1, "Maximum vertical velocity reached");
                      refresh();
                      sleep(1);
                      for(int j = 0; j < COLS; j++){ 
                       mvaddch(LINES - 1, j, ' ');
                      }
                      file_logG(my_log,"Maximum vertical velocity reached");
                    }

          }
          
          // Vz stop button pressed
          else if(check_button_pressed(vz_stp_button, &event)){ 
                    vz = 0;
                    fdCM2 = open(fifoCM2, O_WRONLY ); 

                    if(fdCM2 == -1){
                      unlink(fifoCM2);
                      file_logE(my_log,"open");
                    }

                    if(write(fdCM2, &vz, sizeof(vz)) < 0){
                      close(fdCM2);
                      unlink(fifoCM2); 
                      file_logE(my_log,"write");
                    }

                    close(fdCM2); // close the pipe once it is not needed anymore
                    file_logG(my_log,"Vertical velocity stopped");
                    mvprintw(LINES - 1, 1, "Vertical Motor Stopped");
                    refresh();
                    sleep(1);
                    for(int j = 0; j < COLS; j++){
                        mvaddch(LINES - 1, j, ' ');
                    }
           }
       }
      }

      refresh();
  }

  // Terminate
  endwin();
  
  return 0;
}


