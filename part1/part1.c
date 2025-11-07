#include <unistd.h>
#include <stdio.h>

int main() {
	pid_t pid = getpid();	//call1: get process ID
	pid_t ppid = getppid();	//call2: get parent process ID
	sleep(1);		//call3: sleep for 1 second
	write(1, "Hello\n", 6);	//call4: write to stdout
	char buf[1];
	read(0, buf, 1);	//call5: read from stdin
	return 0;
}
