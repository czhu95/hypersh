#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#define HYPERCALL_MAGIC 712

static char cmd[128] = "";
void main(int argc, char *argv[])
{
    int c;
    bool wait_for_stdin = true;
    while ((c = getopt(argc, argv, "c:")) != -1) {
        switch (c) {
            case 'c':
            strncpy(cmd, optarg, 127);
            wait_for_stdin = false;
        }
    }
    volatile int rc;
    if (!wait_for_stdin)
        rc = syscall(HYPERCALL_MAGIC, cmd);

    while (wait_for_stdin) {
    printf("hypersh$ ");
    if (fgets(cmd, 128, stdin))
        cmd[strcspn(cmd, "\n")] = '\0';
        rc = syscall(HYPERCALL_MAGIC, cmd);
    }
}
