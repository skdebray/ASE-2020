#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>

unsigned char code[] = "\x90\x90\xc3";

void invalid_handler(int sig, siginfo_t *siginfo, void *context) {
	printf("The number is odd\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int num, odd, size = sizeof(code);
	struct sigaction act;
	void (*func)();
	void *buf;

	if(argc != 2) {
		printf("Usage: %s [num]\n", argv[0]);
		return 1;
	}

	act.sa_sigaction = &invalid_handler;

	act.sa_flags = SA_SIGINFO;

	if(sigaction(SIGILL, &act, NULL) < 0) {
		fprintf(stderr, "Cannot set SIGILL\n");
		return 1;
	}
	num = atoi(argv[1]);
	odd = num % 2;

	code[0] += (0x0F - code[0]) * odd;
	code[1] += (0x0B - code[1]) * odd;
	buf = mmap (0,size,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANON,-1,0);
	memcpy(buf, code, size);
	func = (void (*)())buf;
	(void)(*func)();

	printf("The number is even\n");
	return 0;
}
