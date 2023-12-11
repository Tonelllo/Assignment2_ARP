#include "constants.h"
#include "dataStructs.h"
#include "utils/utils.h"
#include "wrapFuncs/wrapFunc.h"
#include <fcntl.h>
#include <signal.h>
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
    int from_drone_pipe, from_input_pipe, to_input_pipe, from_map_pipe,
        to_map_pipe, from_target_pipe, from_obstacles_pipe;

    if (argc == 8) {
        sscanf(argv[1], "%d", &from_drone_pipe);
        sscanf(argv[2], "%d", &from_input_pipe);
        sscanf(argv[3], "%d", &to_input_pipe);
        sscanf(argv[4], "%d", &from_map_pipe);
        sscanf(argv[5], "%d", &to_map_pipe);
        sscanf(argv[6], "%d", &from_target_pipe);
        sscanf(argv[7], "%d", &from_obstacles_pipe);
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

    int maxfd = max(max(max(from_drone_pipe, from_input_pipe),
                        max(from_map_pipe, from_obstacles_pipe)),
                    from_target_pipe);

    char targets_str[MAX_MSG_LEN]   = "T";
    char obstacles_str[MAX_MSG_LEN] = "O";
    while (1) {
        // Temporarily blocking the SIGUSR1 signal to correctly perform the
        // select() syscall without being interrupted Since the time taken from
        // the select to execute is significantly lower than the WD period for
        // sending signals, this mask should not affect the WD behaviour
        sigset_t block_mask;
        sigemptyset(&block_mask);
        sigaddset(&block_mask, SIGUSR1);
        Sigprocmask(SIG_BLOCK, &block_mask, NULL);

        // perform the select
        reader = master;
        Select(maxfd + 1, &reader, NULL, NULL, NULL);

        // unblock SIGUSR1
        Sigprocmask(SIG_UNBLOCK, &block_mask, NULL);

        // check the value returned by the select and perform actions
        // consequently
        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &reader)) {
                int ret = Read(i, received, MAX_STR_LEN);
                if (ret == 0) {
                    logging(LOG_WARN, "Pipe to server closed");
                } else {
                    if (i == from_input_pipe) {
                        sprintf(to_send, "%f,%f|%f,%f", drone_current_pos.x,
                                drone_current_pos.y,
                                drone_current_velocity.x_component,
                                drone_current_velocity.y_component);
                        Write(to_input_pipe, to_send, MAX_STR_LEN);
                    } else if (i == from_drone_pipe) {
                        sscanf(received, "%f,%f|%f,%f", &drone_current_pos.x,
                               &drone_current_pos.y,
                               &drone_current_velocity.x_component,
                               &drone_current_velocity.y_component);
                    } else if (i == from_map_pipe) {
                        sprintf(to_send, "D%f|%f", drone_current_pos.x,
                                drone_current_pos.y);
                        Write(to_map_pipe, to_send, MAX_STR_LEN);
                        printf("tsnehu: %s\n", obstacles_str);
                        Write(to_map_pipe, targets_str, MAX_MSG_LEN);
                        Write(to_map_pipe, obstacles_str, MAX_MSG_LEN);
                    } else if (i == from_obstacles_pipe) {
                        strcpy(obstacles_str, received);
                    } else if (i == from_target_pipe) {
                        strcpy(targets_str, received);
                    }
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
