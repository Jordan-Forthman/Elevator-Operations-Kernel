/*
 * consumer -- starts or stops the elevator.
 *
 *     ./consumer --start
 *     ./consumer --stop
 *
 * Uses the start_elevator() (548) and stop_elevator() (550) syscalls when the
 * kernel provides them, and falls back to writing /proc/elevator_ctl when it
 * does not, so one binary works against either build of the module.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYS_START_ELEVATOR 548
#define SYS_STOP_ELEVATOR  550
#define CTL_PATH           "/proc/elevator_ctl"

/* Write one command line to the module's control file. Returns 0 on success. */
static int ctl_command(const char *cmd)
{
    int fd = open(CTL_PATH, O_WRONLY);
    ssize_t n;

    if (fd < 0) {
        fprintf(stderr, "consumer: cannot open %s: %s\n",
                CTL_PATH, strerror(errno));
        fprintf(stderr, "consumer: is the elevator module loaded?\n");
        return -1;
    }
    n = write(fd, cmd, strlen(cmd));
    close(fd);
    return (n < 0) ? -1 : 0;
}

/*
 * Run one elevator operation, preferring the syscall and falling back to the
 * proc interface on a kernel without it.
 */
static int elevator_op(long nr, const char *cmd)
{
    long ret = syscall(nr);

    if (ret >= 0) {
        /* the syscalls report 1 when the request was a no-op */
        if (ret == 1)
            printf("consumer: elevator already in that state\n");
        return 0;
    }
    if (errno != ENOSYS) {
        fprintf(stderr, "consumer: syscall %ld failed: %s\n",
                nr, strerror(errno));
        return -1;
    }

    fprintf(stderr, "consumer: syscall %ld unavailable, using %s instead\n",
            nr, CTL_PATH);
    return ctl_command(cmd);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s --start | --stop\n", argv[0]);
        return 1;
    }

    if (!strcmp(argv[1], "--start"))
        return elevator_op(SYS_START_ELEVATOR, "start") == 0 ? 0 : 1;

    if (!strcmp(argv[1], "--stop"))
        return elevator_op(SYS_STOP_ELEVATOR, "stop") == 0 ? 0 : 1;

    fprintf(stderr, "Usage: %s --start | --stop\n", argv[0]);
    return 1;
}
