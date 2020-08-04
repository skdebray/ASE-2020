// fibonacci.cpp : Defines the entry point for the console application.
//

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	int target, i, prev, cur, temp;
	if(argc != 2) {
		printf("Usage: %s <fibonacci # position>\n", argv[0]);
		return 1;
	}

	target = atoi(argv[1]);

	if(target == 0) {
		if(argv[1][0] == '0') {
			printf("0\n");
			return 0;
		}
		else {
			printf("The input must be a number\n");
			return 1;
		}
	}

	if(target < 0) {
		printf("The input number must be positive\n");
		return 1;
	}

	prev = 0;
	cur = 1;
	for(i = 1; i < target; i++) {
		temp = cur;
		cur += prev;
		prev = temp;
	}

	printf("%d\n", cur);

	return 0;
}

