#include <cstdio>
#include "synthizer/delay_line.hpp"

int main() {
	auto dl = new synthizer::DelayLine<int, 32>();
	for (int i = 0; i < 10000; i++) {
		auto read = dl->read(5);
		if (i < 5 && read != 0 || i > 5 && i-read != 5) {
			printf("Expected %i but got %i\n", i-5, read);
			return 1;
		}
		dl->write(i);
		dl->advance();
	}
	delete dl;
	return 0;
}
