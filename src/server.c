#include "constants.h"
#include "dataStructs.h"
#include "utils/utils.h"
#include "wrapFuncs/wrapFunc.h"
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// WD pid
pid_t WD_pid;

// Once the SIGUSR1 is received send back the SIGUSR2 signal
void signal_handler(int signo, siginfo_t *info, void *context) {
    // Specifying that context is unused
    (void)(context);

    if (signo == SIGUSR1) {
        WD_pid = info->si_pid;
        Kill(WD_pid, SIGUSR2);
    }
}

int main(int argc, char *argv[]) {
    // Specifying that argc and argv are unused variables
    int from_drone_pipe, from_input_pipe, to_input_pipe, from_map_pipe,
        to_map_pipe;

    if (argc == 6) {
        sscanf(argv[1], "%d", &from_drone_pipe);
        sscanf(argv[2], "%d", &from_input_pipe);
        sscanf(argv[3], "%d", &to_input_pipe);
        sscanf(argv[4], "%d", &from_map_pipe);
        sscanf(argv[5], "%d", &to_map_pipe);
    } else {
        printf("Wrong number of arguments in input\n");
        getchar();
        exit(1);
    }

    // Signal declaration
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    // Setting the signal handler
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);
    // Setting flags
    // The SA_RESTART flag is used to restart all those syscalls that can get
    // interrupted by signals
    sa.sa_flags = SA_SIGINFO | SA_RESTART;

    // Enabling the handler with the specified flags
    Sigaction(SIGUSR1, &sa, NULL);

    // Specifying that argc and argv are unused variables
    int from_drone_pipe, to_drone_pipe, from_input_pipe, to_input_pipe,
        from_map_pipe, to_map_pipe, from_target_pipe, to_target_pipe,
        from_obstacles_pipe, to_obstacle_pipe;

    if (argc == 11) {
        sscanf(argv[1], "%d", &from_drone_pipe);
        sscanf(argv[2], "%d", &to_drone_pipe);
        sscanf(argv[3], "%d", &from_input_pipe);
        sscanf(argv[4], "%d", &to_input_pipe);
        sscanf(argv[5], "%d", &from_map_pipe);
        sscanf(argv[6], "%d", &to_map_pipe);
        sscanf(argv[7], "%d", &from_target_pipe);
        sscanf(argv[8], "%d", &to_target_pipe);
        sscanf(argv[9], "%d", &from_obstacles_pipe);
        sscanf(argv[10], "%d", &to_obstacle_pipe);
    } else {
        printf("Server: wrong number of arguments in input\n");
        getchar();
        exit(1);
    }

    // Structs for each drone information
    struct pos drone_current_pos           = {0};
    struct velocity drone_current_velocity = {0};

    // Declaring the logfile aux buffer
    char received[MAX_MSG_LEN];
    char to_send[MAX_MSG_LEN];

    fd_set reader;
    fd_set master;
    FD_ZERO(&reader);
    FD_ZERO(&master);
    FD_SET(from_drone_pipe, &master);
    FD_SET(from_input_pipe, &master);
    FD_SET(from_map_pipe, &master);
    FD_SET(from_obstacles_pipe, &master);
    FD_SET(from_target_pipe, &master);
    FD_SET(from_map_pipe, &master);

    int maxfd = max(max(max(from_drone_pipe, from_input_pipe),
                        max(from_map_pipe, from_obstacles_pipe)),
                    max(from_target_pipe, from_map_pipe));


    bool to_exit = false;
    while (1) {
        // perform the select
        reader = master;
        Select_wmask(maxfd + 1, &reader, NULL, NULL, NULL);

        // check the value returned by the select and perform actions
        // consequently
        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &reader)) {
                int ret = Read(i, received, MAX_MSG_LEN);
                if (ret == 0) {
                    printf("Pipe to server closed\n");
                    Close(i);
                    FD_CLR(i, &master);
                } else {
                    if (i == from_input_pipe) {
                        if (!strcmp(received, "STOP")) {
                            Write(to_drone_pipe, "STOP", MAX_MSG_LEN);
                            Write(to_map_pipe, "STOP", MAX_MSG_LEN);
                            Write(to_obstacle_pipe, "STOP", MAX_MSG_LEN);
                            Write(to_target_pipe, "STOP", MAX_MSG_LEN);
                            to_exit = true;
                            break;
                        } else {
                            sprintf(to_send, "%f,%f|%f,%f", drone_current_pos.x,
                                    drone_current_pos.y,
                                    drone_current_velocity.x_component,
                                    drone_current_velocity.y_component);
                            Write(to_input_pipe, to_send, MAX_MSG_LEN);
                        }
                    } else if (i == from_drone_pipe) {
                        sscanf(received, "%f,%f|%f,%f", &drone_current_pos.x,
                               &drone_current_pos.y,
                               &drone_current_velocity.x_component,
                               &drone_current_velocity.y_component);
                        sprintf(to_send, "D%f|%f", drone_current_pos.x,
                                drone_current_pos.y);
                        Write(to_map_pipe, to_send, MAX_MSG_LEN);
                    } else if (i == from_map_pipe) {
                        logging(LOG_INFO, received);
                        if (!strcmp(received, "GE")) {
                            Write(to_target_pipe, "GE", MAX_MSG_LEN);
                        } else if (received[0] == 'T' && received[1] == 'H') {
                            logging(LOG_INFO, received);
                            Write(to_drone_pipe, received, MAX_MSG_LEN);
                        }
                    } else if (i == from_obstacles_pipe) {
                        Write(to_map_pipe, received, MAX_MSG_LEN);
                        Write(to_drone_pipe, received, MAX_MSG_LEN);
                    } else if (i == from_target_pipe) {
                        Write(to_map_pipe, received, MAX_MSG_LEN);
                        Write(to_drone_pipe, received, MAX_MSG_LEN);
                    }
                }
            }
        }
        if(to_exit)
            break;
    }
    // TODO close pipes
    Close(from_drone_pipe);
    Close(from_input_pipe);
    Close(from_map_pipe);
    Close(from_obstacles_pipe);
    Close(from_target_pipe);
    Close(to_drone_pipe);
    Close(to_map_pipe);
    Close(to_obstacle_pipe);
    Close(to_target_pipe);
    Close(to_input_pipe);
    return EXIT_SUCCESS;
}
