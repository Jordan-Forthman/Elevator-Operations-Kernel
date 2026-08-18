/*
 * empty -- the baseline for the strace comparison. It does nothing, so its
 * trace contains only the syscalls the C runtime makes to start and stop a
 * process. part1.c should show exactly five more than this.
 */
int main(void)
{
	return 0;
}
