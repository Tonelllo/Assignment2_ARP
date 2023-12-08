#include "utils/utils.h"
#include "constants.h"
#include "wrapFuncs/wrapFunc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>

#define MAX_PROCESSES 5
#define MAX_VALUES_FOR_PROCESS 30
#define MAX_PROCESS_NAME_LEN 30
#define MAX_LEN_PROCESS_ATTR 50

// Struct in which to store the process and the various parameters
struct kv {
    int line;
    char process[MAX_PROCESS_NAME_LEN];
    char keys[MAX_VALUES_FOR_PROCESS][MAX_LEN_PROCESS_ATTR];
    float values[MAX_VALUES_FOR_PROCESS];
};

int read_parameter_file(struct kv *params) {
    // Function that parses the parameter file and stores everyting in
    // the passed struct
    // Auxiliar struct in which to save the read data before pushing it
    // into the params array
    struct kv aux;
    // Pointer to the paramter file
    FILE *f;
    // Pointer to the current position in the line read by getline
    // By setting line to null getline automatically allocs the needed
    // amount of memory to read from the file
    char *line = NULL;
    // Size of the line read by getline
    size_t len = 0;
    // Number of read bytes
    ssize_t read;
    // Index that indicates the number of values that has been read for the
    // process taken into consideration
    int values_index = 0;
    // Pointer used by strtok_r in order to save the position of the current
    // token
    char *token;
    // Saveptr is an utility pointer used by strtok_r
    char *saveptr;
    // Index that saves the current number of processes that have been processed
    int process_index = 0;

    // Try to open the configuration file. If it fails outputs an error message
    f = Fopen("conf/drone_parameters.conf", "r");
    // Lock the file in case there will be any concurrent access, but
    // considering that all the accesses will be in reading it won't be a big
    // concern
    Flock(fileno(f), LOCK_SH);

    // counter that keeps track of the current analyzed line. It's used at the
    // end to display at what line there is a syntax error or misconfiguration
    int line_counter = 0;

    while (1) {
        // Read a line from the file
        read = getline(&line, &len, f);
        line_counter++;
        // Please note that this function has not a wrapper function because
        // -1 is returned both in case of an error and EOF. The
        // EOF condition is used later in the function so no wrapper function
        // could be craeted

        // If an actual line is read than remove the last character in the
        // string by assigning to it the null terminator. The last character
        // in the string at least in a linux environment will always be \n
        if (read != -1)
            line[strlen(line) - 1] = 0;
        // If the line is empty or we have reached the end of the file than
        // we have completed the read of the parameters for the current
        // processed process so it is saved in the _params array and everyting
        // is resetted
        if (read == -1 || strlen(line) == 0) {
            params[process_index++] = aux;
            values_index            = 0;
            memset(aux.process, 0, sizeof(aux.process));
            memset(aux.values, 0, sizeof(aux.values));
            memset(aux.keys, 0, sizeof(aux.keys));
            // After saving and resetting if getline read EOF than this function
            // has finished its purpouse
            if (read == -1)
                break;
        } else if (line[0] == '#') {
            // Continue if a comment is read
            continue;
        } else if (line[0] == '[') {
            // If a square bracket is detected than this is the beginnig of a
            // new process identifier, so it needs to be stored into aux.process
            // and some processing is needed in order to remove the square
            // brackets. In aux.process is copied the read line starting from
            // the 1 index meaning that the beginning '[' is ignored. Then to
            // ignore the closing
            // ']' -2 is removed from the size of the line. -2 instead of -1
            // because If the dimension of the string is 3 the maximum array
            // index will be 2.
            strncpy(aux.process, &line[1], strlen(line) - 2);
        } else {
            // If none of the previous conditions is met then we have a
            // key-value pair that needs to be tokenized
            // Firstly tokenize the key
            token = strtok_r(line, "=", &saveptr);
            strcpy(aux.keys[values_index], token);
            // Then tokenize the value by setting null to the line value
            // in strtok_r. This tells the function to continue with the
            // processing of the previous line
            token = strtok_r(NULL, "=", &saveptr);
            // The value token is now converted into float and saved
            // in the values array
            for (unsigned int i = 0; i < strlen(token); i++) {
                if (!(isdigit(token[i]) || token[i] == '.')) {
                    if (line)
                        free(line);
                    // Logging
                    char logmsg[100];
                    sprintf(logmsg,
                            "Parameter must be float at line %d in "
                            "config file\n",
                            line_counter);
                    logging(LOG_ERROR, logmsg);
                    exit(EXIT_FAILURE);
                }
            }
            float aux_value;
            sscanf(token, "%f", &aux_value);
            aux.values[values_index++] = aux_value;
        }
    }

    // Remove the lock from the file f
    Flock(fileno(f), LOCK_UN);
    // Close the file
    Fclose(f);
    // If line has still some memory allocated by getline free it
    if (line)
        free(line);

    // Return the number of read processes
    return process_index;
}

float get_param(char *process, char *param) {
    // This function returns the value of the parameter specified
    // for the specified process
    struct kv read_values[MAX_PROCESSES];
    // The values are read from the file
    int processes = read_parameter_file(read_values);
    // First the correct process is searched
    for (int i = 0; i < processes; i++) {
        if (!strcmp(read_values[i].process, process)) {
            // Once the correct process is identified the value corresponding
            // to the provided key is searched
            for (int j = 0; strlen(read_values[i].keys[j]) != 0; j++) {
                if (!strcmp(read_values[i].keys[j], param)) {
                    // If it is found it is returned
                    return read_values[i].values[j];
                }
            }
        }
    }
    // In case of error the process is killed, since the misconfiguration
    // or the parsing error of a parameter would cause irreparable damage
    // to the system

    // Logging
    char logmsg[300];
    sprintf(logmsg,
            "Parameter %s of process %s not found, check for syntax "
            "errors or mispells in "
            "config file\n",
            param, process);
    logging(LOG_ERROR, logmsg);
    exit(EXIT_FAILURE);
}

void logging(char *type, char *message) {
    FILE *F;
    F = Fopen(LOGFILE_PATH, "a");
    // Locking the logfile
    Flock(fileno(F), LOCK_EX);
    fprintf(F, "[%s] - %s\n", type, message);
    // Unlocking the file so that the server can access it again
    Flock(fileno(F), LOCK_UN);
    Fclose(F);
}
