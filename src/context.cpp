#include "synthizer/context.hpp"

#include "synthizer/audio_output.hpp"
#include "synthizer/config.hpp"
#include "synthizer/types.hpp"

#include <functional>
#include <memory>
#include <utility>

namespace synthizer {

Context::Context() {
	this->audio_output = createAudioOutput([&] () {
		this->context_semaphore.signal();
	}, false);

	this->source_panners = createPannerBank();
	this->running.store(1);
	this->context_thread = std::move(std::thread([&] () {
		this->audioThreadFunc();
	}));
}

Context::~Context() {
	if(this->running.load()) {
		this->shutdown();
	}
}

void Context::shutdown() {
	this->running.store(0);
	this->context_semaphore.signal();
	this->context_thread.join();
}

void Context::enqueueInvokable(Invokable *invokable) {
	pending_invokables.enqueue(invokable);
	this->context_semaphore.signal();
}

std::shared_ptr<PannerLane> Context::allocateSourcePannerLane(enum SYZ_PANNER_STRATEGIES strategy) {
	return this->source_panners->allocateLane(strategy);
}

void Context::generateAudio(unsigned int channels, AudioSample *destination) {
	std::fill(destination, destination + channels * config::BLOCK_SIZE, 0.0f);

	for(auto f: this->pregenerate_callbacks) {
		f();
	}

	this->source_panners->run(channels, destination);
}

void Context::audioThreadFunc() {
	while (this->running.load()) {
		AudioSample *write_audio = nullptr;

		this->context_semaphore.wait();

		write_audio = this->audio_output->beginWrite();
		while (write_audio) {
			this->generateAudio(2, write_audio);
			this->audio_output->endWrite();
			write_audio = this->audio_output->beginWrite();
		}

		Invokable *inv = this->pending_invokables.dequeue();
		while (inv) {
			inv->invoke();
			inv = this->pending_invokables.dequeue();
		}
	}

	this->audio_output->shutdown();
}

}