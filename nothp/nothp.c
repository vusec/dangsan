#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0) != 0) {
        perror("nothp: prctl failed");
        return -1;
    }

    execvp(argv[1], argv + 1);
    perror("nothp: exec failed");
    return -1;
}
