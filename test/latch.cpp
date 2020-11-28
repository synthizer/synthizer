#include "synthizer/cells.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <random>
#include <thread>

using namespace synthizer;

/* Makes partial writes visible, by setting the fields and sleeping in the middle. */
struct Partial {
	bool a;
	bool b;
	bool c;
};

int main() {
	LatchCell<Partial> cell{{ false, false, false }};
	std::atomic<int> running = 1;

	auto worker_thread = std::thread([&]() {
		std::mt19937 engine{10};
		std::uniform_int_distribution<int> dist{5, 10};
		while (running.load()) {
			cell.writeWithCallback([&](Partial &dest) {
				dest.a = !dest.a;
				std::this_thread::sleep_for(std::chrono::milliseconds(dist(engine)));
				dest.b = !dest.b;
				std::this_thread::sleep_for(std::chrono::milliseconds(dist(engine)));
				dest.c = !dest.c;
			});
		}
	});

	for (unsigned int i = 0; i < 1000; i++) {
		Partial v = cell.read();
		assert(v.a == v.b && v.b == v.c);
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	running.store(0);
	worker_thread.join();
}
