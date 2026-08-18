/*
 * producer -- generates pets and issues them to the elevator.
 *
 *     ./producer [number_of_pets]
 *
 * Each pet gets a random type, a random start floor, and a random destination
 * floor that differs from its start. Requests go through the issue_request()
 * syscall (549) when the kernel provides it, and fall back to writing
 * /proc/elevator_ctl when it does not -- so the same binary drives both the
 * patched-kernel build and the stock-kernel build of the module.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define SYS_ISSUE_REQUEST 549
#define CTL_PATH          "/proc/elevator_ctl"

#define MIN_FLOOR 1
#define MAX_FLOOR 5
#define NUM_TYPES 4

static const char *type_names[NUM_TYPES] = {
    "Chihuahua", "Pug", "Pughuahua", "Dachshund"
};

/* Write one command line to the module's control file. Returns 0 on success. */
static int ctl_command(const char *cmd)
{
    int fd = open(CTL_PATH, O_WRONLY);
    ssize_t n;

    if (fd < 0)
        return -1;
    n = write(fd, cmd, strlen(cmd));
    close(fd);
    return (n < 0) ? -1 : 0;
}

/*
 * Issue one request. Tries the syscall first; if the running kernel has no
 * such syscall, transparently switches to the proc interface for the rest of
 * the run (use_proc is sticky, so the failed syscall is paid for only once).
 */
static int issue_request(int start, int dest, int type, int *use_proc)
{
    char cmd[64];
    long ret;

    if (!*use_proc) {
        ret = syscall(SYS_ISSUE_REQUEST, start, dest, type);
        if (ret >= 0)
            return (int)ret;
        if (errno != ENOSYS)
            return -1;
        /* kernel is not patched -- fall back for this and later requests */
        *use_proc = 1;
        fprintf(stderr,
                "producer: syscall %d unavailable, using %s instead\n",
                SYS_ISSUE_REQUEST, CTL_PATH);
    }

    snprintf(cmd, sizeof(cmd), "request %d %d %d", start, dest, type);
    return ctl_command(cmd);
}

int main(int argc, char **argv)
{
    int count, i, issued = 0, use_proc = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [number_of_pets]\n", argv[0]);
        return 1;
    }

    count = atoi(argv[1]);
    if (count <= 0) {
        fprintf(stderr, "%s: number_of_pets must be a positive integer\n",
                argv[0]);
        return 1;
    }

    srand((unsigned int)(time(NULL) ^ getpid()));

    for (i = 0; i < count; i++) {
        int type  = rand() % NUM_TYPES;
        int start = MIN_FLOOR + rand() % MAX_FLOOR;
        int dest;

        /* a pet never travels to the floor it is already standing on */
        do {
            dest = MIN_FLOOR + rand() % MAX_FLOOR;
        } while (dest == start);

        if (issue_request(start, dest, type, &use_proc) != 0) {
            fprintf(stderr, "%s: request %d->%d (%s) rejected: %s\n",
                    argv[0], start, dest, type_names[type],
                    errno ? strerror(errno) : "invalid");
            continue;
        }

        printf("%-10s floor %d -> floor %d\n", type_names[type], start, dest);
        issued++;
    }

    printf("issued %d of %d requests\n", issued, count);
    return (issued == count) ? 0 : 1;
}
