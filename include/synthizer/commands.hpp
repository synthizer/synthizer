#pragma once

#include "synthizer/config.hpp"

#include <tuple>
#include <type_traits>
#include <utility>

namespace synthizer {

/*
 * Commands are a way to provide logic to execute on the audio thread which only require allocation, deallocation, etc. if the implementation of the command
 * does.  This is done through what is essentially a hand-rolled vtable implementation, so that the vtable can be stored inline in the base command,
 * then allocating objects in aligned_storage.
 * */

class CommandVtable {
	public:
	/**
	 * Destruct dest.
	 * */
	void (*destructor)(void *dest);
	/**
	 * Actually execute the command.
	 * */
	void (*execute)(void *dest);
};

/**
 * Template methods which can be used to fill out the vtable.
 * */
template<typename T>
void execute_cb(void *dest) {
	T *dest_t = reinterpret_cast<T *>(dest);
	dest_t->execute();
}

template<typename T>
void destructor_cb(void *dest) {
	T *dest_t = reinterpret_cast<T *>(dest);
	dest_t->~T();
}

/**
 * A type-erased, inline allocated unit of execution.
 * */
class Command {
	public:
	Command(): initialized(false) {}
	Command(const Command &other) = delete;
	Command(Command &&other) = delete;

	~Command() {
		this->deinitialize();
	}

	/*
	 * Construct this command so that it erases T. args are passed to T's constructor.
	 * 
	 * T must provide an execute method.
	 * */
	template<typename T, typename... ARGS>
	void initialize(ARGS... args) {
		static_assert(alignof(T) <= config::ALIGNMENT);
		static_assert(sizeof(T) <= config::MAX_COMMAND_SIZE);

		this->deinitialize();

		new(&data) T(std::forward<ARGS>(args)...);
		this->vtable.destructor = destructor_cb<T>;
		this->vtable.execute = execute_cb<T>;
		this->initialized = true;
	}

	/*
	 * Used to deinitialize the command in place. Allows for placing these in queues that don't copy/destruct, by getting rid of the
	 * wrapped data.
	 * 
	 * May be called safely even when the command is deinitialized already.
	 * */
	void deinitialize() {
		if (initialized) {
			this->vtable.destructor(&this->data);
			this->initialized = false;
		}
	}

	void execute() {
		if (this->initialized) {
			this->vtable.execute(&this->data);
		}
	}

	private:
	std::aligned_storage<config::MAX_COMMAND_SIZE, config::ALIGNMENT>::type data;
	CommandVtable vtable;
	bool initialized = false;
};

/**
 * The most common kind of command calls a callable with a set of arguments.
 * */
template<typename CB, typename... ARGS>
class CallbackCommandPayload {
	public:
	CallbackCommandPayload(CB callback, ARGS ...args): callback(callback), args(args...) {}

	void execute() {
		std::apply(this->callback, this->args);
	}
	
	private:
	CB callback;
	std::tuple<ARGS...> args;
};

/**
 * Take a pointer to a command and configure it to call a callable.
 * 
 * NOTE: it is very important that the following aren't references, including rvalue references. If they are, template argument deduction will infer everything as references,
 * then the command can end up referring to things on the stack.
 * */
template<typename CB, typename ...ARGS>
void initCallbackCommand(Command *cmd, CB callback, ARGS ...args) {
	cmd->initialize<CallbackCommandPayload<CB, ARGS...>, CB, ARGS...>(callback, args...);
}

}