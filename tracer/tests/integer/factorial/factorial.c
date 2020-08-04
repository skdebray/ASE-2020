#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	if(argc != 2) {
		printf("Usage %s <factorial>\n", argv[0]);
		return 1;
	}

	long factorial = atol(argv[1]);

	unsigned long result = 1;
	unsigned long i;
	for(i = 1; i <= factorial; i++) {
		result *= i;
	}

	printf("%ld\n", result);

	return 0;
}
