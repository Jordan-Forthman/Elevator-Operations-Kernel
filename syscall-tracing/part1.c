/*
 * part1 -- makes exactly five system calls beyond the empty baseline, so that
 * diffing part1.trace against empty.trace shows precisely those five:
 *
 *     getpid, getppid, nanosleep (via sleep), write, read
 *
 * stdio is deliberately not used: printf and friends issue syscalls of their
 * own and would obscure the comparison.
 */
#include <unistd.h>

int main(void)
{
	char buf[1];
	ssize_t n;

	(void)getpid();			/* call 1: get process ID */
	(void)getppid();		/* call 2: get parent process ID */
	sleep(1);			/* call 3: nanosleep for 1 second */
	n = write(1, "Hello\n", 6);	/* call 4: write to stdout */
	n = read(0, buf, 1);		/* call 5: read from stdin */
	(void)n;

	return 0;
}
