#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>

#include "hypersh.h"

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
        hypersh_exec(cmd);

    while (wait_for_stdin) {
    printf("hypersh$ ");
    if (fgets(cmd, 128, stdin))
        cmd[strcspn(cmd, "\n")] = '\0';
        hypersh_exec(cmd);
    }
}
