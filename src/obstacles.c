#include "constants.h"
#include "dataStructs.h"
#include "wrapFuncs/wrapFunc.h"
#include <curses.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utils/utils.h>

//loop for sending new obstacles to spawn
#define PERIOD 10

// WD pid
pid_t WD_pid;

// Once the SIGUSR1 is received send back the SIGUSR2 signal
void signal_handler(int signo, siginfo_t *info, void *context) {
    // Specifying thhat context is not used
    (void)(context);

    if (signo == SIGUSR1) {
        WD_pid = info->si_pid;
        Kill(WD_pid, SIGUSR2);
    }
}

int main(int argc, char *argv[]) {
    // Specifying that argc and argv are unused variables
    int to_server_pipe;

    if (argc == 2) {
        sscanf(argv[1], "%d", &to_server_pipe);
    } else {
        printf("Wrong number of arguments in obstacles\n");
        getchar();
        exit(1);
    }

    // Signal declaration
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    // Setting the signal handler
    sa.sa_sigaction = signal_handler;
    // Resetting the mask
    sigemptyset(&sa.sa_mask);
    // Setting flags
    // The SA_RESTART flag has been added to restart all those syscalls that can
    // get interrupted by signals
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    Sigaction(SIGUSR1, &sa, NULL);

    //coordinates of obstacles
    float obstacle_x, obstacle_y;
    //string for the communication with server
    char to_send[MAX_STR_LEN];
    //string for logging
    char logmsg[100];

    //seeding the random nymber generator with the current time, so that it starts with
    //a different state every time the programs is executed
    srandom((unsigned int)time(NULL));

    while (1) {
        //spawn random coordinates in map field range and send it to the server, so that they can be spawned in the map
        for (int i = 0; i < N_OBSTACLES; i++) {
            obstacle_x = random() % MAX_NUMBER;
            obstacle_y = random() % MAX_NUMBER;
            sprintf(to_send, "%f,%f", obstacle_x, obstacle_y);
            Write(to_server_pipe, to_send, MAX_STR_LEN);
        }
        sprintf(logmsg, "Obstacles process generated a new set of obstacles");
        logging(LOG_INFO, logmsg);
        sleep(PERIOD);
    }

    // Cleaning up
    Close(to_server_pipe);
    return 0;
}
