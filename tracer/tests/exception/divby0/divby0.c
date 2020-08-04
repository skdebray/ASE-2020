#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>

void div_handler(int sig, siginfo_t *siginfo, void *context) {
	printf("Cannot divide by 0\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int result = 0, num, den;
	struct sigaction act;

	if(argc != 3) {
		printf("Usage: %s <num> <den>\n", argv[0]);
		return 1;
	}

	act.sa_sigaction = &div_handler;

	act.sa_flags = SA_SIGINFO;

	if(sigaction(SIGFPE, &act, NULL) < 0) {
		fprintf(stderr, "Cannot set SIGFPE\n");
		return 1;
	}

	num = atoi(argv[1]);
	den = atoi(argv[2]);

	result = num / den;

	printf("Result: %d\n", result);

	return 0;
}
