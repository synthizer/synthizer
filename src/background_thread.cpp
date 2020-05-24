#include "synthizer/background_thread.hpp"

#include "synthizer/logging.hpp"
#include "synthizer/queues/vyukov.hpp"

#include "sema.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <thread>

namespace synthizer {

class BackgroundThreadCommand: public VyukovHeader<BackgroundThreadCommand> {
	public:
	BackgroundThreadCallback callback;
	void *arg;
};

static VyukovQueue<BackgroundThreadCommand> background_queue, background_free_queue;
Semaphore background_thread_semaphore;
std::atomic<int> background_thread_running = 0;

void backgroundThreadFunc() {
	while(background_thread_running.load(std::memory_order_relaxed)) {
		auto *possible = background_queue.dequeue();
		while (possible) {
			try {
				possible->callback(possible->arg);
			} catch(...) {
				logError("Exception on background thread. This should never happen");
			}
			background_free_queue.enqueue(possible);
			possible = background_queue.dequeue();
		}
		background_thread_semaphore.wait();
	}

	/*
	 * At exit, drain the queue.
	 * This prevents things like the context from having threads running past the end of main.
	 * */
	auto *cmd = background_queue.dequeue();
	while (cmd) {
		cmd->callback(cmd->arg);
		cmd = background_queue.dequeue();
	}
}

std::thread background_thread;
void startBackgroundThread() {
	background_thread_running.store(1, std::memory_order_relaxed);
	background_thread = std::move(std::thread(backgroundThreadFunc));
}

void stopBackgroundThread() {
	background_thread_running.store(0, std::memory_order_relaxed);
	background_thread_semaphore.signal();
	background_thread.join();
}

/* We allocate BackgroundThreadCommands as blocks; how big is that block? */
const std::size_t BACKGROUND_THREAD_COMMAND_BLOCK_SIZE = 32;
void callInBackground(BackgroundThreadCallback cb, void *arg) {
	static std::mutex free_mutex;
	std::lock_guard l{free_mutex};
	auto *cmd = background_free_queue.dequeue();
	if (cmd == nullptr) {
		BackgroundThreadCommand *block = new BackgroundThreadCommand[BACKGROUND_THREAD_COMMAND_BLOCK_SIZE];
		cmd = &block[0];
		for(std::size_t i = 1; i < BACKGROUND_THREAD_COMMAND_BLOCK_SIZE; i++) {
			background_free_queue.enqueue(&block[i]);
		}
	}
	cmd->callback = cb;
	cmd->arg = arg;
	background_queue.enqueue(cmd);
	background_thread_semaphore.signal();
}

}