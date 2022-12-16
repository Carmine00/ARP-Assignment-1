# ARP-Hoist-Assignment
This is an interactive simulator of a hoist with 2 d.o.f, developed at the University of Genoa in the academic year 2022/2023 during the Advanced and Robotics Programming course.

## Description
A master process spawns six different processes responsible for the implementation of the actual behaviour of the hoist: command console, inspection, M1, M2, real and watchdog.
The command console and the inspection exploit the ncurses library to provide to the user a graphical interface to command the hoist. 
The command console allows to increase, decrease or stop the velocity along the x or z axis; whereas the inspection shows the motion of the hoist, by receiveing the updated coordinates from the real process, and permits to use two emergency buttons: STOP and RESET.
The STOP button immediately sets to zero both velocities, whereas the RESET functionality is used to bring the hoist back to the original position (0,0).
M1 and M2 are responsible to simulate the behaviour of the physical engines and they both send the new coordinates of the hoist to the real process, which adds a fictitious error taking into account problem that might occur in real-life scenarios.
Last but not least, the watchdog process checks what the whole system has been doing in a period of one minute; in case nothing has happened, the watchdog alerts the inspection process and a RESET is activated.


## ncurses installation
In order to run the program is necessary to install the ncurses library, simply open a terminal and type the following command:
```console
sudo apt-get install libncurses-dev
```

## Compiling and running the code
**Assuming you have Konsole installed in your system**, to compile and run the code it is enough to launch the script:
```console
./run.sh
```
## Github repository
https://github.com/Carmine00/ARP-Assignment-1.git

## Author
Carmine Miceli
