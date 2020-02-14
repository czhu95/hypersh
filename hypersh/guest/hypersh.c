#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>

#define HYPERCALL_MAGIC 712

static char cmd[128] = "";
void main()
{
    volatile int rc;
    while (true) {
	printf("hypersh$ ");
	if (fgets(cmd, 128, stdin))
            rc = syscall(HYPERCALL_MAGIC, cmd);
    }
}
