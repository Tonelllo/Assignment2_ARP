#include "wrapFuncs/wrapFunc.h"
#include <curses.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int Wait(int *wstatus) {
    int ret = wait(wstatus);
    if (ret < 0) {
        perror("Error on wait execution");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Waitpid(pid_t pid, int *wstatus, int options) {
    int ret = waitpid(pid, wstatus, options);
    if (ret < 0) {
        perror("Error on waitpid execution");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Execvp(const char *file, char **args) {
    int ret = execvp(file, args);
    if (ret < 0) {
        perror("Error on execvp execution");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Fork(void) {
    int ret = fork();
    if (ret < 0) {
        perror("Error on forking");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Read(int fd, void *buf, size_t nbytes) {
    int ret = read(fd, buf, nbytes);
    if (ret < 0) {
        perror("Error on reading from file descriptor");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Write(int fd, void *buf, size_t nbytes) {
    int ret = write(fd, buf, nbytes);
    if (ret < 0) {
        perror("Error on writing to file descriptor");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout) {
    int ret = select(nfds, readfds, writefds, exceptfds, timeout);
    if (ret < 0) {
        // TODO temporary
        // char aux[100];
        // sprintf(aux, "Error on executing select %d", getpid());
        // perror(aux);
        // fflush(stdout);
        // getchar();
        // exit(EXIT_FAILURE);
    }
    return ret;
}

int Select_wmask(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                 struct timeval *timeout) {
    // Temporarily blocking the SIGUSR1 signal to correctly perform the
    // select() syscall without being interrupted Since the time taken from
    // the select to execute is significantly lower than the WD period for
    // sending signals, this mask should not affect the WD behaviour
    // Block SIGUSR1
    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);
    Sigprocmask(SIG_BLOCK, &block_mask, NULL);

    int ret = select(nfds, readfds, writefds, exceptfds, timeout);
    // unblock SIGUSR1
    Sigprocmask(SIG_UNBLOCK, &block_mask, NULL);

    if (ret < 0) {
        char aux[100];
        sprintf(aux, "Error on executing select %d", getpid());
        perror(aux);
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Open(const char *file, int oflag) {
    int ret = open(file, oflag);
    if (ret < 0) {
        perror("Error on executing open");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Pipe(int *pipedes) {
    int ret = pipe(pipedes);
    if (ret < 0) {
        perror("Error on executing pipe");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Close(int fd) {
    int ret = close(fd);
    if (ret < 0) {
        perror("Error on executing close");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Shm_open(const char *name, int flag, mode_t mode) {
    int ret = shm_open(name, flag, mode);
    if (ret < 0) {
        perror("Error on executing shm_open");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Ftruncate(int fd, __off_t length) {
    int ret = ftruncate(fd, length);
    if (ret < 0) {
        perror("Error on executing ftruncate");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) {
    void *ret = mmap(addr, len, prot, flags, fd, offset);
    if (ret == MAP_FAILED) {
        perror("Error on executing mmap");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}
sem_t *Sem_open(const char *name, int oflag, mode_t mode, unsigned int value) {
    sem_t *ret = sem_open(name, oflag, mode, value);
    if (ret == SEM_FAILED) {
        perror("Error on executing sem_open");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Sem_init(sem_t *sem, int pshared, int value) {
    int ret = sem_init(sem, pshared, value);
    if (ret < 0) {
        perror("Error on executing sem_init");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Sem_wait(sem_t *sem) {
    int ret = sem_wait(sem);
    if (ret < 0) {
        perror("Error on executing sem_wait");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Sem_post(sem_t *sem) {
    int ret = sem_post(sem);
    if (ret < 0) {
        perror("Error on executing sem_post");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Sem_close(sem_t *sem) {
    int ret = sem_close(sem);
    if (ret < 0) {
        perror("Error on executing sem_close");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Sem_unlink(const char *name) {
    int ret = sem_unlink(name);
    if (ret < 0) {
        perror("Error on executing sem_unlink");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

int Flock(int fd, int operation) {
    int ret = flock(fd, operation);
    if (ret < 0) {
        perror("Error on executing flock");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

FILE *Fopen(const char *pathname, const char *mode) {
    FILE *ret = fopen(pathname, mode);
    if (ret == NULL) {
        perror("Error on executing fopen");
        fflush(stdout);
        getchar();
        exit(EXIT_FAILURE);
    }
    return ret;
}

void Kill(int pid, int signal) {
    int ret = kill(pid, signal);
    if (ret < 0) {
        perror("Error on executing kill");
        getchar();
        exit(EXIT_FAILURE);
    }
}

int Kill2(int pid, int signal) {
    int ret = kill(pid, signal);
    if (ret < 0)
        return -1;
    return 0;
}

void Mkfifo(const char *fifo_path, int permit) {
    if (access(fifo_path, F_OK) < 0) {
        if (mkfifo(fifo_path, permit) < 0) {
            perror("Error on executing mkfifo");
            getchar();
            exit(EXIT_FAILURE);
        }
    }
}

void Sigaction(int signum, const struct sigaction *act,
               struct sigaction *oldact) {
    int ret = sigaction(signum, act, oldact);
    if (ret < 0) {
        perror("Error on executing sigaction");
        getchar();
        exit(EXIT_FAILURE);
    }
}

void Sigprocmask(int type, const sigset_t *mask, sigset_t *oldset) {
    int ret = sigprocmask(type, mask, oldset);
    if (ret < 0) {
        perror("Error on executing sigprocmask");
        exit(EXIT_FAILURE);
    }
}

void Fclose(FILE *stream) {
    int ret = fclose(stream);
    if (ret == EOF) {
        perror("Error on executing fclose");
        getchar();
        exit(EXIT_FAILURE);
    }
}

void Shm_unlink(const char *name) {
    int ret = shm_unlink(name);
    if (ret < 0) {
        perror("Error on executing shm_unlink");
        getchar();
        exit(EXIT_FAILURE);
    }
}

void Munmap(void *addr, size_t len) {
    int ret = munmap(addr, len);
    if (ret < 0) {
        perror("Error on executing munmap");
        getchar();
        exit(EXIT_FAILURE);
    }
}
