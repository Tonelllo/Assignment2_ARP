# Table of contents
- [Creators](#creators)
- [ARP: Assignment 2](#arp-assignment-2)
  * [How to run](#how-to-run)
    + [Building dependencies](#building-dependencies)
    + [Command to run the program](#command-to-run-the-program)
  * [How does it work](#how-does-it-work)
    + [Architecture](#architecture)
    + [Active components](#active-components)
      - [Server](#server)
      - [Map](#map)
      - [Drone](#drone)
      - [Input](#input)
      - [Target](#target)
      - [Obstacles](#obstacles)
      - [Watchdog](#watchdog)
    + [Configuration file](#configuration-file)
  * [Other components, directories and files](#other-components-directories-and-files)
  * [Known error cases](#known-error-cases)

<small><i><a href='http://ecotrust-canada.github.io/markdown-toc/'>Table of contents generated with markdown-toc</a></i></small>

# Creators 
Davide Tonelli - S6424332

Valentina Condorelli - S4945679

Group name: ConTo

# ARP: Assignment 2
## How to run
### Building dependencies
To build this project the following dependencies are needed:
+ make
+ CMake version > 3.6
+ c compiler
+ libncurses
### Command to run the program
Simply execute the run.sh script in the main folder by typing in the shell:

    ./run.sh

## How does it work
### Architecture
![architecture-image-placeholder](docs/Schema_assignment2_ARP.png?raw=true)
### Active components
The active components of this project are:
- Server
- Map
- Drone
- Input
- Watchdog (WD)
- Target
- Obstacles
#### Server
As it can be seen in the architecture scheme, the main role of the **server** process is to read from the pipes coming from all the processes and distribute messages and information accordingly. Moreover, by means of a *fork()*, it spawns the **map** process. 

The primitives used by the server are:
- Kill(): used to send a signal to the WD to tell that it's alive
- Sigaction(): used to initialize the signal handler to handle the signal sent by the WD
- Fork(): used to create a child process which can spawn the map
- Select_wmask(): select() used to check the pipes with data and select one of them to read from. It additionally implements a temporary sigmask for SIGUSR1, so that the syscall can be executed without interrupts. Since the execution time of the select() is significantly lower than the WD period for sending signals, this mask should not affect the WD behaviour
- Fopen(), Fclose(): used to open, close and lock/unlock a file located in a specific path (in this case, the log file). Included in our self-defined `logging()` function, whose details can be found in the `utils.c` file
- Write(), Read(): used to write to and read from the pipes
- Execvp(): used to spawn the map
- Close(): used to safely close the pipes before exiting the process

#### Map
The **map** process reads the position data of the drone and the coordinates of the targets and obstacles set from the server through a pipe and displays it on the screen. As a consequence, the drone can be seen moving on the map among attractive targets and repulsive obstacles. The movement is dictated by its dynamics, with the borders as limits considered like obstacles.
Additionally, the score increment for the current game is calculated with the following formula:
```math
score\_increment =
\begin{cases}
    20 - ⌊t⌋, & \text{if } t < 20 \\
    1, & \text{if } t \geq 20
\end{cases}
```
where ⌊t⌋ is the time taken by the drone to reach the target starting from the instant when the targets are spawned in the map.

The primitives used by the map are:
- Kill(): used to send a signal to the WD to tell that it's alive
- Sigaction(): used to initialize the signal handler to handle the signal sent by the WD
- Mkfifo(): used to create a FIFO (named pipe) to send its PID to the WD
- Open(), Close(): used to open and close the file descriptor associated to the FIFO
- Select(): used to check the server pipe and read from it when new data are available
- Fopen(), Fclose(), Flock(): used to open, close and lock/unlock a file located in a specific path (in this case, the log file). Included in our self-defined `logging()` function, whose details can be found in the `utils.c` file
- Write(), Read(): used to write to and read from the pipes
- Close(): also used to safely close the pipes before exiting the process

#### Drone
The **drone** process is the one responsible for calculating:
- the current position of the drone, based on the old position;
- the current applied forces, as given by the input process;
- the repulsive forces of the walls;
- the current speed of the drone. 

The formula used to calculate the next position of the drone is the following:
```math
x = \frac{
    W_x + D_x - \frac{M}{T^2}\cdot(x(t-2) - 2\cdot x(t-1)) + \frac{K}{T}\cdot x(t-1)
}{
    \Big(\frac{M}{T^2} + \frac{K}{T}\Big)
}
```
Where:
+ $W_x$ is the x component of the repulsive force from the walls
+ $D_x$ is the x component of the force acting on the drone due to user input
+ $M$ is the mass of the drone
+ $T$ is the time interval with which the formula is calculated
+ $x$, $x(t-1)$, $x(t-2)$ are the position of the drone at different time instants
+ $K$ is the viscous coefficient

The same formula is used for the y coordinate.

On the other hand, the formula used to calculate the repulsive (and attractive, by means of a +/- sign) force applied to the drone is the Latombe repulsive force:
```math
\begin{align}
    F_{rep}(q) = \begin{cases}
        \eta \cdot (\frac{1}{\rho(q)} - \frac{1}{\rho_0}) \cdot \frac{1}{\rho^2(q)} \cdot vel & \text{if } \rho(q) \leq \rho_0 \land \rho(q) > min \\
        \text{0} & \text{else}
    \end{cases}
\end{align}
```
Where:
+ $\eta$ is a constant to scale the intensity of the force
+ $\rho(q)$ is the distance between the drone and the closest obstacle/target
+ $\rho_0$ is the area of effect of the force
+ $vel$ is the velocity of the drone
+ $min$ is the minimum threshold for $\rho(q)$; if it's lower, the function nullifies, so that situations in which an obstacle is spawned in the exact same position as the drone are correctly handled

The primitives used by the drone are:
- Kill(): used to send a signal to the WD to tell that it's alive
- Sigaction(): used to initialize the signal handler to handle the signal sent by the WD
- Select_wmask(): select() used to check the pipes with data and select one of them to read from. It additionally implements a temporary sigmask for SIGUSR1, so that the syscall can be executed without interrupts. Since the execution time of the select() is significantly lower than the WD period for sending signals, this mask should not affect the WD behaviour
- Fopen(), Fclose(), Flock(): used to open, close and lock/unlock a file located in a specific path (in this case, the log file). Included in our self-defined `logging()` function, whose details can be found in the `utils.c` file
- Write(), Read(): used to write to and read from the pipes
- Close(): used to safely close the pipes before exiting the process

#### Input
The **input** process takes the input from the user keyboard and calculates the current forces acting on the drone based on those inputs. The resulting forces are then written to the server though a pipe, in order to be available for the drone process, which uses those forces to compute its dynamics.
The input is also responsible for displaying all the parameters of the drone, such as position, velocity and forces acting on it. The displayed forces do not take into account repulsive forces of the walls, but only the ones that the user is applying through the input.
Finally, the input has the task of sending a 'STOP' string if the 'P' key is pressed, so that all the processes will be safely closed. 

The keys available for the user are:
```
+-+-+---+---+        +---+
| q | w | e |        | p |
+---+---+---+        +---+
| a | s | d |  --->  
+---+---+---+
| z | x | c |
+-+-+---+---+
```
The eight external keys can be used to move the drone by adding a force in the respective direction (top, top-right, right and so on). On the other hand, the 'S' key in used to instantly zero all the forces, in order to see the inertia on the drone. The space key does the same thing as the s key. Lastly, the 'P' key can be used to safely close the program.
Please note that, for the keys to work, you should select the Input window when it appears.

The primitives used by the input are:
- Kill(): used to send a signal to the WD to tell that it's alive
- Sigaction(): used to initialize the signal handler to handle the signal sent by the WD
- Mkfifo(): used to create a FIFO (named pipe) to send its PID to the WD
- Open(), Close(): used to open and close the file descriptor associated to the FIFO
- Fopen(), Fclose(), Flock(): used to open, close and lock/unlock a file located in a specific path (in this case, the log file). Included in our self-defined `logging()` function, whose details can be found in the `utils.c` file
- Write(), Read(): used to write to and read from the pipes
- Close(): also used to safely close the pipes before exiting the process

#### Target
The **target** process generates a new set of targets to be spawned in the map. This happens in two different occasion: when the program in launched and whenever the server notifies, by sending the string 'GE' through a pipe, that all the targets have been reached by the drone. The new set of target coordinates is sent to the server, in order to be available for the other processes.

The primitives used by the target are:
- Kill(): used to send a signal to the WD to tell that it's alive
- Sigaction(): used to initialize the signal handler to handle the signal sent by the WD
- Fopen(), Fclose(), Flock(): used to open, close and lock/unlock a file located in a specific path (in this case, the log file). Included in our self-defined `logging()` function, whose details can be found in the `utils.c` file
- Write(), Read(): used to write to and read from the pipes
- Close(): also used to safely close the pipes before exiting the process

#### Obstacles
The **obstacles** process generates a new set of obstacles to be spawned in the map every period. The new set of obstacles coordinates is sent to the server, in order to be available for the other processes.

The primitives used by the target are:
- Kill(): used to send a signal to the WD to tell that it's alive
- Sigaction(): used to initialize the signal handler to handle the signal sent by the WD
- Select(): used to check the server pipe and read from it when new data are available
- Fopen(), Fclose(), Flock(): used to open, close and lock/unlock a file located in a specific path (in this case, the log file). Included in our self-defined `logging()` function, whose details can be found in the `utils.c` file
- Write(), Read(): used to write to and read from the pipes
- Close(): also used to safely close the pipes before exiting the process

#### Watchdog
The **watchdog** is responsible for checking the correct execution of all the other processes. For this purpose, the WD sends the SIGUSR1 signal to all the processes. Then, two checks are made. Firstly, it verifies that the processes are alive by checking if the *kill* syscall returns an error and immediately kills all the processes if this happens.
Secondly, it verifies that the processes are not frozen by waiting for a SIGUSR2 reply by each process. In case that this signal is not received, meaning
that the process is frozen, the WD kills all the processes.

### Configuration file
The configuration file `drone_parameters.toml` contains all the necessary parameters for the drone dynamics and for the correct behaviour of the other processes.
It has been built by following the [TOML standard](https://toml.io/en/)

## Other components, directories and files
The project is structured as follows:
```
.
├── bin
│   ├── conf
│   │   └── drone_parameters.conf -------------> Contains the parameter file for the drone dynamics and setting parameters for the processes
│   └── log -----------------------------------> Contains the logfile
│       └── log.log
├── build -------------------------------------> Destination folder of all the built file after running the run.sh script
├── CMakeLists.txt ----------------------------> Main cmake file
├── docs --------------------------------------> Contains a few documents to better understand the project
│   ├── assignmentsv2.1.pdf
│   ├── function.png
│   └── Schema_assignment1_ARP.png
│   └── Schema_assignment2_ARP.png
├── headers
│   ├── CMakeLists.txt
│   ├── constants.h ---------------------------> Contains all the constants used in the project
│   ├── dataStructs.h
│   ├── utils ---------------------------------> Contains headers and .c file for utility functions 
│   │   ├── utils.c
│   │   └── utils.h
│   └── wrapFuncs -----------------------------> Contains headers and .c file that implement wrappers for all the syscall that return a perror used in the project with errors checking included
│       ├── wrapFunc.c
│       └── wrapFunc.h
├── README.md
├── run.sh ------------------------------------> Script to run the project
└── src ---------------------------------------> Contains all active components
    ├── CMakeLists.txt
    ├── WD.c
    ├── drone.c
    ├── input.c
    ├── map.c
    ├── master.c
    ├── obstacles.c
    ├── server.c
    └── target.c
```
## Known error cases
By killing some processes it's possible to trigger a Broken pipe error signal
that is caught from the `wrapfunc` file and printed on console. The most notable
instance of this is when the map process is closed. The server process will
throw a broken pipe error because the drone tries to inform the map passing
through the server of the
updated position but the `map-end` of the `server-map` pipe has been closed. This
cannot be avoided in a "crash" event meaning when a process closes unexpectedly
like in this case. We wanted to add this section in order to emphasize that this
is not a not handled error but a known and not avoidable situation in this case.
