#include <iostream>
#include <cstdlib>

using std::atoi;
using std::cout;
using std::endl;

int main(int argc, char *argv[]) {
	if(argc != 2) {
		cout << "Usage: " << argv[0] << " <factorial>" << endl;
		return 1;
	}

	long factorial = atol(argv[1]);

	double result = 1.0;
	for(int i = 1; i <= factorial; i++) {
		result *= i;
	}

	cout << result << endl;

	return 0;
}
