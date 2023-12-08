#include "constants.h"
#include "wrapFuncs/wrapFunc.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Function to spawn the processes
static void spawn(char **arg_list) {
    Execvp(arg_list[0], arg_list);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // Specifying that argc and argv are unused variables
    (void)(argc);
    (void)(argv);

    // Define an array of strings for every process to spawn
    char programs[NUM_PROCESSES][20];
    strcpy(programs[0], "./server");
    strcpy(programs[1], "./drone");
    strcpy(programs[2], "./input");
    strcpy(programs[3], "./map");
    strcpy(programs[4], "./WD");

    // Pids for all children
    pid_t child[NUM_PROCESSES];

    // String to contain all che children pids (except WD)
    char child_pids_str[NUM_PROCESSES - 1][80];

    // Cycle neeeded to fork the correct number of childs

    int input_drone[2];
    int drone_server[2];
    int server_input[2];
    int input_server[2];
    int map_server[2];
    int server_map[2];
    Pipe(input_drone);
    Pipe(drone_server);
    Pipe(server_input);
    Pipe(input_server);
    Pipe(map_server);
    Pipe(server_map);

    for (int i = 0; i < NUM_PROCESSES; i++) {
        child[i] = Fork();
        if (!child[i]) {
            // Spawn the input and map process using konsole
            char *arg_list[]         = {programs[i], NULL, NULL, NULL,
                                        NULL,        NULL, NULL, NULL};
            char *konsole_arg_list[] = {"konsole", "-e", programs[i], NULL,
                                        NULL,      NULL, NULL};
            char input_drone_str[10];
            char drone_server_str[10];
            char server_input_str[10];
            char input_server_str[10];
            char map_server_str[10];
            char server_map_str[10];

            switch (i) {
                case 0:
                    // Server
                    sprintf(drone_server_str, "%d", drone_server[0]);
                    sprintf(input_server_str, "%d", input_server[0]);
                    sprintf(server_input_str, "%d", server_input[1]);
                    sprintf(map_server_str, "%d", map_server[0]);
                    sprintf(server_map_str, "%d", server_map[1]);
                    arg_list[1] = drone_server_str;
                    arg_list[2] = input_server_str;
                    arg_list[3] = server_input_str;
                    arg_list[4] = map_server_str;
                    arg_list[5] = server_map_str;
                    Close(drone_server[1]);
                    Close(input_server[1]);
                    Close(server_input[0]);
                    Close(map_server[1]);
                    Close(server_map[0]);
                    spawn(arg_list);
                    break;
                case 1:
                    // Drone
                    sprintf(input_drone_str, "%d", input_drone[0]);
                    sprintf(drone_server_str, "%d", drone_server[1]);
                    arg_list[1] = input_drone_str;
                    arg_list[2] = drone_server_str;
                    Close(input_drone[1]);
                    Close(drone_server[0]);
                    spawn(arg_list);
                    break;
                case 2:
                    // Input
                    sprintf(input_drone_str, "%d", input_drone[1]);
                    sprintf(input_server_str, "%d", input_server[1]);
                    sprintf(server_input_str, "%d", server_input[0]);
                    konsole_arg_list[3] = input_drone_str;
                    konsole_arg_list[4] = input_server_str;
                    konsole_arg_list[5] = server_input_str;
                    Close(input_drone[0]);
                    Close(input_server[0]);
                    Close(server_input[1]);
                    Execvp("konsole", konsole_arg_list);
                    exit(EXIT_FAILURE);
                    break;
                case 3:
                    // Map
                    sprintf(map_server_str, "%d", map_server[1]);
                    sprintf(server_map_str, "%d", server_map[0]);
                    konsole_arg_list[3] = map_server_str;
                    konsole_arg_list[4] = server_map_str;
                    Close(map_server[0]);
                    Close(server_map[1]);
                    Execvp("konsole", konsole_arg_list);
                    exit(EXIT_FAILURE);
                    break;
            }
            // spawn the last program, so the WD, which needs all the processes
            // PIDs
            if (i == NUM_PROCESSES - 1) {
                for (int i = 0; i < NUM_PROCESSES - 1; i++)
                    sprintf(child_pids_str[i], "%d", child[i]);

                // Sending as arguments to the WD all the processes PIDs
                char *arg_list[] = {programs[i],       child_pids_str[0],
                                    child_pids_str[1], child_pids_str[2],
                                    child_pids_str[3], NULL};
                spawn(arg_list);
            }
        }
    }

    // Printing the pids
    printf("Server pid is %d\n", child[0]);
    printf("Drone pid is %d\n", child[1]);
    printf("Konsole of Input pid is %d\n", child[2]);
    printf("Konsole of Map pid is %d\n", child[3]);
    printf("WD pid is %d\n", child[4]);

    // Value for waiting for the children to terminate
    int res;

    // Wait for all direct children to terminate. Map and the konsole on which
    // it runs on are not direct childs of the master process but of the server
    // one so they will not return here
    for (int i = 0; i < NUM_PROCESSES; i++) {
        int ret = Wait(&res);
        // Getting the exit status
        int status = 0;
        WEXITSTATUS(status);
        printf("Process %d terminated with code: %d\n", ret, status);
    }

    return EXIT_SUCCESS;
}
