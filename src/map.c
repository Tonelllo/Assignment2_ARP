#include "constants.h"
#include "dataStructs.h"
#include "ncurses.h"
#include "utils/utils.h"
#include "wrapFuncs/wrapFunc.h"
#include <curses.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

// WD pid
pid_t WD_pid;

// This array keeps the position of all the targets and obstacles in order to
// perform collision checking
int target_obstacles_screen_position[N_TARGETS + N_OBSTACLES][2];

// Index for keeping track of the previous array that is handled like a stack
int tosp_top = -1;

// Once the SIGUSR1 is received send back the SIGUSR2 signal
void signal_handler(int signo, siginfo_t *info, void *context) {
    // Specifying that context is unused
    (void)(context);

    if (signo == SIGUSR1) {
        WD_pid = info->si_pid;
        Kill(WD_pid, SIGUSR2);
    }
}

// Create the outer border of the window
WINDOW *create_map_win(int height, int width, int starty, int startx) {
    WINDOW *local_win;

    local_win = newwin(height, width, starty, startx);
    box(local_win, 0, 0); /* 0, 0 gives default characters
                           * for the vertical and horizontal
                           * lines			*/
    return local_win;
}

// Destroy the map window. Useful to refresh the window once the terminal is
// resized
void destroy_map_win(WINDOW *local_win) {
    wborder(local_win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    /* The parameters taken are
     * 1. win: the window on which to operate
     * 2. ls: character to be used for the left side of the window
     * 3. rs: character to be used for the right side of the window
     * 4. ts: character to be used for the top side of the window
     * 5. bs: character to be used for the bottom side of the window
     * 6. tl: character to be used for the top left corner of the window
     * 7. tr: character to be used for the top right corner of the window
     * 8. bl: character to be used for the bottom left corner of the window
     * 9. br: character to be used for the bottom right corner of the window
     */
    wrefresh(local_win);
    delwin(local_win);
}

void tokenization(struct pos *arr_to_fill, char *to_tokenize,
                  int *objects_num) {
    int index_of;
    char *char_pointer;
    char *aux_ptr;
    char *token;
    char_pointer = strchr(to_tokenize, ']');
    index_of     = (int)(char_pointer - to_tokenize);
    token        = strtok_r(to_tokenize + index_of + 1, "|", &aux_ptr);
    int index    = 1;
    if (token != NULL) {
        float aux_x, aux_y;
        index = 1;
        sscanf(token, "%f,%f", &aux_x, &aux_y);
        arr_to_fill[0].x = aux_x;
        arr_to_fill[0].y = aux_y;
        logging(LOG_INFO, token);
        while ((token = strtok_r(NULL, "|", &aux_ptr)) != NULL) {
            sscanf(token, "%f,%f", &aux_x, &aux_y);
            logging(LOG_INFO, token);
            arr_to_fill[index].x = aux_x;
            arr_to_fill[index].y = aux_y;
            index++;
        }
    }
    *objects_num = index;
}

void remove_target(int index, struct pos *target_arr, int target_num) {
    for (int i = index; i < target_num - 1; i++) {
        target_arr[index].x = target_arr[index + 1].x;
        target_arr[index].y = target_arr[index + 1].y;
    }
}

bool is_overlapping(int y, int x) {
    // This is not strictly overlapping checking but here allows for less code
    // repetition
    if (y < 1 || x < 1 || y > LINES - 3 || x > COLS - 2)
        return true;

    // Checking for overlapping
    for (int i = 0; i <= tosp_top; i++) {
        if (y == target_obstacles_screen_position[i][0] &&
            x == target_obstacles_screen_position[i][1])
            return true;
    }
    return false;
}

void find_spot(int *old_y, int *old_x) {
    int x = *old_x;
    int y = *old_y;

    for (int index = 1;; index++) {
        y = *old_y - index;
        for (int x = *old_x - index; x <= *old_x + index; x++) {
            if (!is_overlapping(y, x)) {
                *old_y = y;
                *old_x = x;
                return;
            }
        }
        y = *old_y + index;
        for (int x = *old_x - index; x <= *old_x + index; x++) {
            if (!is_overlapping(y, x)) {
                *old_y = y;
                *old_x = x;
                return;
            }
        }
        x = *old_x - index;
        for (int y = *old_y - index + 1; y <= *old_y + index - 1; y++) {
            if (!is_overlapping(y, x)) {
                *old_y = y;
                *old_x = x;
                return;
            }
        }
        x = *old_x + index;
        for (int y = *old_y - index + 1; y <= *old_y + index - 1; y++) {
            if (!is_overlapping(y, x)) {
                *old_y = y;
                *old_x = x;
                return;
            }
        }
        if (index > 100) {
            logging(LOG_ERROR,
                    "Unable to find a spot on the screen to print on map");
            exit(EXIT_FAILURE);
        }
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

    int to_server, from_server;
    if (argc == 3) {
        sscanf(argv[1], "%d", &to_server);
        sscanf(argv[2], "%d", &from_server);
    } else {
        printf("Wrong number of arguments in map\n");
        getchar();
        exit(1);
    }

    //score variables
    int score = 0;
    int score_increment = 0;
    //time when the target are spawned
    time_t start_time;

    // Named pipe (fifo) to send the pid to the WD
    Mkfifo(FIFO1_PATH, 0666);

    // Getting the map pid
    int map_pid = getpid();
    char map_pid_str[10];

    // Here the pid of the map process is passed to the WD
    sprintf(map_pid_str, "%d", map_pid);

    // Writing to the fifo the previously formatted string
    int fd;
    fd = Open(FIFO1_PATH, O_WRONLY);
    Write(fd, map_pid_str, strlen(map_pid_str) + 1);
    Close(fd);

    // Setting up the struct in which to store the position of the drone
    // in order to calculate the current position on the screen of the drone
    struct pos drone_pos = {0, 0};
    struct pos targets_pos[N_TARGETS];
    int target_num = 0;
    struct pos obstacles_pos[N_OBSTACLES];
    int obstacles_num = 0;

    // Setting up ncurses
    initscr();
    // Disabling line buffering
    cbreak();
    // Hide the cursor in order to not see the white carret on the screen
    curs_set(0);
    // Displaying the first intance of the window
    WINDOW *map_window =
        create_map_win(getmaxy(stdscr) - 2, getmaxx(stdscr), 1, 0);

    char received[MAX_MSG_LEN];
    // Setting up structures needed for terminal resizing

    struct timeval select_timeout;
    select_timeout.tv_sec  = 5;
    select_timeout.tv_usec = 0;
    fd_set reader, master;
    FD_ZERO(&reader);
    FD_ZERO(&master);
    FD_SET(from_server, &master);
    while (1) {
        // Updating the drone position by reading from the shared memory, and
        // taking the semaphores
        reader = master;
        int ret;
        do {
            ret = Select_wmask(from_server + 1, &reader, NULL, NULL, &select_timeout);
            // The only reason to get an erorr is if Select gets interrupted by
            // a signal. In that case the function should be restarted if the
            // SA_RESTART flag didn't do its job
        } while (ret == -1);

        select_timeout.tv_sec  = 5;
        select_timeout.tv_usec = 0;

        if (FD_ISSET(from_server, &reader)) {
            int read_ret = Read(from_server, received, MAX_MSG_LEN);
            if (read_ret == 0) {
                Close(from_server);
                FD_CLR(from_server, &master);
                logging(LOG_WARN, "Pipe to map closed");
            } else {
                char aux[100];
                if (!strcmp(received, "STOP")) {
                    break;
                }
                switch (received[0]) {
                    case 'D':
                        sscanf(received, "D%f|%f", &drone_pos.x, &drone_pos.y);
                        break;
                    case 'O':
                        logging(LOG_INFO,
                                "vvvvvvvvvvvOBSTACLESvvvvvvvvvvvvvvv");
                        tokenization(obstacles_pos, received, &obstacles_num);
                        logging(LOG_INFO,
                                "^^^^^^^^^^^OBSTACLES^^^^^^^^^^^^^^^");
                        sprintf(aux, "Obtacles %d", obstacles_num);
                        logging(LOG_INFO, aux);
                        break;
                    case 'T':
                        logging(LOG_INFO, "vvvvvvvvvvvTARGETSvvvvvvvvvvvvvvv");
                        tokenization(targets_pos, received, &target_num);
                        logging(LOG_INFO, "^^^^^^^^^^^TARGETS^^^^^^^^^^^^^^^");
                        sprintf(aux, "Targets %d", target_num);
                        logging(LOG_INFO, aux);
                        start_time = time(NULL);
                        break;
                }
            }
        }

        // Display the menu text
        refresh();
        // Displaying the title of the window.
        mvprintw(0, 0, "MAP DISPLAY");

        mvprintw(0, COLS / 3, "Score: %d", score);

        // Deleting the old window that is encapsulating the map in order to
        // create the animation, and to allow the resizing of the window in case
        // of terminal resize
        delwin(map_window);
        // Redrawing the window. This is useful if the screen is resized
        map_window = create_map_win(LINES - 1, COLS, 1, 0);

        // In order to correctly handle the drone position calculation all
        // the simulations are done in a 500x500 square. Then the position of
        // the drone is converted to the dimension of the window by doing a
        // proportion. The computation done in the following line could by
        // expressed as the following in the x axis:
        // drone_position_in_terminal = simulated_drone_pos_x * term_width/500
        // Now for the terminal width and height it needs to be taken into
        // consideration the border of the window itself. Considering for
        // example the x axis we have a border on the left and one on the right
        // so we need to subtract 2 from the width of the main_window. Now the
        // extra -1 is due to the fact that the index starts from 0 and not
        // from 1. With this we mean that if we have an array of dimension 3.
        // The highest index of an element in the arry will be 2, not 3. This
        // explains why -3 instead of -2.
        int drone_x = round(1 + drone_pos.x * (getmaxx(map_window) - 3) /
                                    SIMULATION_WIDTH);
        int drone_y = round(1 + drone_pos.y * (getmaxy(map_window) - 3) /
                                    SIMULATION_HEIGHT);

        int target_x, target_y;
        bool to_decrease = false;
        char to_send[MAX_MSG_LEN];
        for (int i = 0; i < target_num; i++) {
            target_x = round(1 + targets_pos[i].x * (getmaxx(map_window) - 3) /
                                     SIMULATION_WIDTH);
            target_y = round(1 + targets_pos[i].y * (getmaxy(map_window) - 3) /
                                     SIMULATION_HEIGHT);
            if (target_x == drone_x && target_y == drone_y) {
                time_t impact_time = time(NULL) - start_time;
                //If a target is reached in the first 20 seconds, the score increases of
                //20 - the number of seconds taken to reach it. For example, if a target is reached
                //in 5 or more seconds, but less than 6, the score increases by 20-5=15.
                //In general, the score increases of:
                //- 20 - (integer part of impact_time), if impact_time < 20
                //- 1, if impact_time >= 20
                if (impact_time < 20.0)
                    score_increment = 20 - (int)ceil(impact_time);
                else
                    score_increment = 1;
                score = score + score_increment;

                sprintf(to_send, "TH|%d|%.3f,%.3f", i, targets_pos[i].x,
                        targets_pos[i].y);
                remove_target(i, targets_pos, target_num);
                Write(to_server, to_send, MAX_MSG_LEN);
                to_decrease = true;
            } else {
                if (is_overlapping(target_y, target_x))
                    find_spot(&target_y, &target_x);
                target_obstacles_screen_position[++tosp_top][0] = target_y;
                target_obstacles_screen_position[tosp_top][1]   = target_x;
                mvwprintw(map_window, target_y, target_x, "T");
            }
        }
        // check whether all the targets have been hit
        if (to_decrease) {
            if(!--target_num)
                Write(to_server, "GE", MAX_MSG_LEN);
        }

        int obst_x, obst_y;
        bool can_display_drone = true;
        for (int i = 0; i < obstacles_num; i++) {
            obst_x = round(1 + obstacles_pos[i].x * (getmaxx(map_window) - 3) /
                                   SIMULATION_WIDTH);
            obst_y = round(1 + obstacles_pos[i].y * (getmaxy(map_window) - 3) /
                                   SIMULATION_HEIGHT);
            if (is_overlapping(obst_y, obst_x))
                find_spot(&obst_y, &obst_x);
            target_obstacles_screen_position[++tosp_top][0] = obst_y;
            target_obstacles_screen_position[tosp_top][1] = obst_x;
            mvwprintw(map_window, obst_y, obst_x, "O");

            if (obst_y == drone_y && obst_x == drone_x)
                can_display_drone = false;
        }

        // The drone is now displayed on the screen
        if (can_display_drone)
            mvwprintw(map_window, drone_y, drone_x, "+");

        // The map_window is refreshed
        wrefresh(map_window);
        tosp_top = -1;
    }

    /// Clean up
    Close(to_server);
    Close(from_server);
    endwin();
    return EXIT_SUCCESS;
}
