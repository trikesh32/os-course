#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    char buf[20];
    int pipefd[2];
    pipe(pipefd);
    if (fork() == 0){
        read(pipefd[0], buf, 6);
        printf("%d: got %s", getpid(), buf);
        write(pipefd[1], "pong\n", 6);
        close(pipefd[1]);
        exit(0);
    } else {
        write(pipefd[1], "ping\n", 6);
        wait((int *)0);
        read(pipefd[0], buf, 6);
        printf("%d: got %s", getpid(), buf);
        close(pipefd[0]);
        exit(0);
    }
}