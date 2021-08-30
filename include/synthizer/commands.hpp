#pragma once

#include "synthizer/config.hpp"

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace synthizer {

/*
 * Commands are a way to provide logic to execute on the audio thread which only require allocation, deallocation, etc.
 * if the implementation of the command does.  This is done through what is essentially a hand-rolled vtable
 * implementation, so that the vtable can be stored inline in the base command, then allocating objects in
 * aligned_storage.
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
template <typename T> void execute_cb(void *dest) {
  T *dest_t = reinterpret_cast<T *>(dest);
  dest_t->execute();
}

template <typename T> void destructor_cb(void *dest) {
  T *dest_t = reinterpret_cast<T *>(dest);
  dest_t->~T();
}

/**
 * A type-erased, inline allocated unit of execution.
 * */
class Command {
public:
  Command() : initialized(false) {}
  Command(const Command &other) = delete;
  Command(Command &&other) = delete;

  ~Command() { this->deinitialize(); }

  /*
   * Construct this command so that it erases T. args are passed to T's constructor.
   *
   * T must provide an execute method.
   * */
  template <typename T, typename... ARGS> void initialize(ARGS... args) {
    static_assert(sizeof(T) <= config::MAX_COMMAND_SIZE);

    this->deinitialize();

    new (&data) T(std::forward<ARGS>(args)...);
    this->vtable.destructor = destructor_cb<T>;
    this->vtable.execute = execute_cb<T>;
    this->initialized = true;
  }

  /*
   * Used to deinitialize the command in place. Allows for placing these in queues that don't copy/destruct, by getting
   * rid of the wrapped data.
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
  std::aligned_storage<config::MAX_COMMAND_SIZE>::type data;
  CommandVtable vtable;
  bool initialized = false;
};

/**
 * The most common kind of command calls a callable with a set of arguments.
 * */
template <typename CB, typename... ARGS> class CallbackCommandPayload {
public:
  CallbackCommandPayload(CB callback, ARGS... args) : callback(callback), args(args...) {}

  void execute() { std::apply(this->callback, this->args); }

private:
  CB callback;
  std::tuple<ARGS...> args;
};

/**
 * Take a pointer to a command and configure it to call a callable.
 *
 * NOTE: it is very important that the following aren't references, including rvalue references. If they are, things
 * don't copy and we get dangling references.
 * */
template <typename CB, typename... ARGS> void _initCallbackCommandNoref(Command *cmd, CB callback, ARGS... args) {
  cmd->initialize<CallbackCommandPayload<CB, ARGS...>, CB, ARGS...>(callback, args...);
}

template <typename... ARGS> void initCallbackCommand(Command *cmd, ARGS... args) {
  _initCallbackCommandNoref<std::remove_reference_t<ARGS>...>(cmd, args...);
}

/*
 * The following implements an equivalent to initCallbackCommand, except that any shared_ptr arguments are
 * weakened to weak_ptr equivalents.  They're repromoted to shared_ptr at execution time, and the command is optionally
 * short-circuited if any of them are null.
 * */
namespace referencing_cmd_details {
template <typename T> T weaken(T &&v) { return v; }

template <typename T> std::weak_ptr<T> weaken(std::shared_ptr<T> v) { return std::weak_ptr<T>(v); }

/*
 * The first parameter is a boolean, written to if the template
 * successfully promoted to shared_ptr.  This is done as:
 *
 * *success = *success && new_success
 *
 * So that the higher level template can tell if *any* of these failed, not just the current one.
 * */
template <typename T> T strengthen(bool *success, T v) {
  *success = *success && true;
  return v;
}

template <typename T> std::shared_ptr<T> strengthen(bool *success, std::weak_ptr<T> v) {
  auto strengthened = v.lock();
  *success = *success && strengthened != nullptr;
  return strengthened;
}

template <typename CB, typename... ARGS>
void initReferencingCallbackCommandAlreadyWeakened(Command *cmd, bool short_circuit, CB callback, ARGS... args) {
  initCallbackCommand(
      cmd,
      [=](auto &cb_arg) mutable {
        bool did_strengthen = true;
        auto strong_tuple = std::make_tuple(referencing_cmd_details::strengthen(&did_strengthen, args)...);
        if (did_strengthen == false && short_circuit == true) {
          return;
        }
        std::apply(cb_arg, strong_tuple);
      },
      callback);
}
} // namespace referencing_cmd_details

/**
 * Initialize a command to a callback which will be called with references to other objects.
 * This command will store the references internally as weak_ptr as to not prolong the lifetimes of the
 * referenced objects, and convert them back to shared_ptr before invocation.
 * if any of the objects no longer exists and short_circuit is true, the command won't execute
 * if one of the referenced objects is no longer alive.
 *
 * This is used for, e.g., adding generators to sources, where we can't proceed if either the source or generator no
 * longer exists.
 * */
template <typename CB, typename... ARGS>
void initReferencingCallbackCommand(Command *cmd, bool short_circuit, CB callback, ARGS... args) {
  /* Important to remove the reference on CB, if any; for the rest, weaken already handles it. */
  referencing_cmd_details::initReferencingCallbackCommandAlreadyWeakened<std::remove_reference_t<CB>>(
      cmd, short_circuit, callback, referencing_cmd_details::weaken(args)...);
}

} // namespace synthizer
