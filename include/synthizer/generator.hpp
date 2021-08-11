#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/pausable.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/types.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace synthizer {

class Context;

/*
 * A generator: an abstraction over the concept of producing audio.
 * 
 * Examples of things that could be generators include noise, basic sine waves, and reading from streams.
 * 
 * Generators have two pieces of functionality:
 * 
 * - They output a block of samples, of up to config::MAX_CHANNELS channels (truncating if more).
 * - They adopt to pitch bends in a generator-defined fashion to participate in doppler for moving sources, and/or if asked by the user.
 * 
 * NOTE: users of generators should immediately convert the generator to a `GeneratorRef` (below).
 * This is necessary to avoid "leaking" generators in complex scenarios, primarily around linger behavior, and also allows
 * existant generators to know if they are in use.
 * */
class Generator: public Pausable, public BaseObject {
	public:
	Generator(const std::shared_ptr<Context> &ctx): BaseObject(ctx) {}

	/* Return the number of channels this generator wants to output on the next block. */
	virtual unsigned int getChannels() = 0;

	/**
	 * Non-virtual internal callback. Implements pausing, gain, etc.
	 * 
	 * Derived classes should override generateBlock.
	 * */
	void run(float *output);

	/**
	 * Output a complete block of audio of config::BLOCK_SIZE. Is expected to add to the output, not replace.
	 * 
	 * Should respect the passed in FadeDriver for gain.
	 * */
	virtual void generateBlock(float *output, FadeDriver *gain_driver) = 0;

	/**
	 * Returns whether something is using this generator still.
	 * */
	bool isInuse() {
		return this->use_count.load(std::memory_order_relaxed) != 0;
	}

	bool wantsLinger() override;
	std::optional<double> startLingering(const std::shared_ptr<CExposable> &ref, double configured_timeout) override final;

	/**
	 * All generators should support linger.  This function allows the base class to do extra
	 * generator-specific logic like never lingering when paused, and so derived classes implement it instead of
	 * startLingering.
	 * 
	 * The return value has the same meaning as startLingering, except that it may be modified
	 * by the generator.
	 * */
	virtual std::optional<double> startGeneratorLingering() = 0;

	#define PROPERTY_CLASS Generator
	#define PROPERTY_LIST GENERATOR_PROPERTIES
	#define PROPERTY_BASE BaseObject
	#include "synthizer/property_impl.hpp"

	private:
	friend class GeneratorRef;

	FadeDriver gain_driver{1.0, 1};

	/**
	 * Maintained by GeneratorRef on our behalf.
	 * */
	std::atomic<std::size_t> use_count = 0;
};

/**
 * Like a weak_ptr, but lets us hook into various functionality
 * when no weak references exist but strong ones do. Used ot let generators
 * know when they're not being used, which is important for linger behaviors and various
 * optimizations.
 * 
 * This is fundamentally not a threadsafe object. Though it maintains the generator's `use_count` atomically,
 * when the `use_count` reaches zero logic can occur which must execute only
 * on the audio thread.
 * */
class GeneratorRef {
	public:
	GeneratorRef();
	GeneratorRef(const std::shared_ptr<Generator> &generator);
	GeneratorRef(const std::weak_ptr<Generator> &weak);
	GeneratorRef(const GeneratorRef &other);
	GeneratorRef(GeneratorRef &&other);

	GeneratorRef &operator=(const GeneratorRef &other);
	GeneratorRef &operator=(GeneratorRef &&other);

	~GeneratorRef();

	std::shared_ptr<Generator> lock() const;
	bool expired() const;

	private:
	/**
	 * Internal fucntion used to decrement the reference counts in the various
	 * assignment operators.
	 * */
	void decRef();

	std::weak_ptr<Generator> ref;
};

}