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

int main(int argc, char *argv[]) {
    int to_server, from_server;
    if (argc == 3) {
        sscanf(argv[1], "%d", &to_server);
        sscanf(argv[2], "%d", &from_server);
    } else {
        printf("Wrong number of arguments in map\n");
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
    struct pos drone_pos;

    // Setting up ncurses
    initscr();
    // Disabling line buffering
    cbreak();
    // Hide the cursor in order to not see the white carret on the screen
    curs_set(0);
    // Displaying the first intance of the window
    WINDOW *map_window =
        create_map_win(getmaxy(stdscr) - 2, getmaxx(stdscr), 1, 0);

    char received[MAX_STR_LEN];
    // Setting up structures needed for terminal resizing
    while (1) {
        // Updating the drone position by reading from the shared memory, and
        // taking the semaphors

        Write(to_server, "U", 2);
        Read(from_server, received, MAX_STR_LEN);
        sscanf(received, "%f|%f", &drone_pos.x, &drone_pos.y);

        // Displaying the title of the window.
        mvprintw(0, 0, "MAP DISPLAY");
        // Display the menu text
        refresh();

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
        int x = round(1 + drone_pos.x * (getmaxx(map_window) - 3) /
                              SIMULATION_WIDTH);
        int y = round(1 + drone_pos.y * (getmaxy(map_window) - 3) /
                              SIMULATION_HEIGHT);

        // The drone is now displayed on the screen
        mvwprintw(map_window, y, x, "+");

        // The map_window is refreshed
        wrefresh(map_window);
        // The process sleeps for the time needed to have an almost 30fps
        // animation
        usleep(3000);
    }

    /// Clean up
    Close(to_server);
    Close(from_server);
    endwin();
    return EXIT_SUCCESS;
}
