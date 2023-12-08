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

    // Father process
    // Initialize semaphores for each drone information
    sem_t *sem_force = Sem_open(SEM_PATH_FORCE, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_t *sem_position =
        Sem_open(SEM_PATH_POSITION, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_t *sem_velocity =
        Sem_open(SEM_PATH_VELOCITY, O_CREAT, S_IRUSR | S_IWUSR, 1);

    // Setting the semaphores value to 1
    Sem_init(sem_force, 1, 1);
    Sem_init(sem_position, 1, 1);
    Sem_init(sem_velocity, 1, 1);

    // Create shared memory object
    int shm = Shm_open(SHMOBJ_PATH, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);

    // Truncate size of shared memory
    Ftruncate(shm, MAX_SHM_SIZE);

    // Map pointer to shared memory area
    void *shm_ptr =
        Mmap(NULL, MAX_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    memset(shm_ptr, 0, MAX_SHM_SIZE);

    // Structs for each drone information
    struct pos drone_current_pos           = {0};
    struct velocity drone_current_velocity = {0};

    // Declaring the logfile aux buffer
    char received[MAX_STR_LEN];
    char to_send[MAX_STR_LEN];

    fd_set reader;
    fd_set master;
    FD_ZERO(&reader);
    FD_ZERO(&master);
    FD_SET(from_drone_pipe, &master);
    FD_SET(from_input_pipe, &master);
    FD_SET(from_map_pipe, &master);
    int maxfd =
        (from_drone_pipe > from_input_pipe) ? from_drone_pipe : from_input_pipe;
    maxfd = (from_map_pipe > maxfd) ? from_map_pipe : maxfd;
    while (1) {
        reader = master;
        Select(maxfd + 1, &reader, NULL, NULL, NULL);
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
                    }else if(i == from_map_pipe){
                        sprintf(to_send, "%f|%f", drone_current_pos.x, drone_current_pos.y);
                        Write(to_map_pipe, to_send, MAX_STR_LEN);
                    }
                }
            }
        }
    }

    /// Clean up
    // Unlinking the shared memory area
    Shm_unlink(SHMOBJ_PATH);
    // Closing the semaphors
    Sem_close(sem_velocity);
    Sem_close(sem_force);
    Sem_close(sem_position);
    // Unlinking the semaphors files
    Sem_unlink(SEM_PATH_FORCE);
    Sem_unlink(SEM_PATH_POSITION);
    Sem_unlink(SEM_PATH_VELOCITY);
    // Unmapping the shared memory pointer
    Munmap(shm_ptr, MAX_SHM_SIZE);
    return EXIT_SUCCESS;
}
