#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>

int returnVal = 0;

void invalid_handler(int sig, siginfo_t *siginfo, void *context) {
	printf("The number is even\n");
	exit(returnVal);
}

int main(int argc, char *argv[]) {
	int num, odd, *ptr;
	struct sigaction act;

	ptr = &returnVal;

	if(argc != 2) {
		printf("Usage: %s <num>\n", argv[0]);
		return 1;
	}

	act.sa_sigaction = &invalid_handler;

	act.sa_flags = SA_SIGINFO;

	if(sigaction(SIGSEGV, &act, NULL) < 0) {
		fprintf(stderr, "Cannot set SIGFPE\n");
		return 1;
	}

	num = atoi(argv[1]);

	odd = num % 2;

	ptr = (int *)((long)ptr * odd);

	*ptr = 1;

	printf("The number is odd\n");

	return returnVal;
}
