#pragma once

namespace synthizer {

/*
 * A background thread for doing things that might block, for example some forms of memory freeing or callbacks.
 * 
 * This serializes everything on it into one thread and shouldn't be used for intensive work. The point is to be able to defer
 * stuff that talks to the kernel or the user's code so that it's not running on threads that do actually do the work; in future this abstraction
 * might be replaced with something better, but for now it fulfills our needs.
 * 
 * This can allocate in callInBackground, but asymptotically doesn't depending on load, at the cost of permanent ram usage.
 * TODO: do we need a high watermark?
 * */

/* Start/stop should only be called as part of library initialization/deinitialization for now. These functions are not threadsafe, nor can the thread be safely started more than once. */
void startBackgroundThread();
void stopBackgroundThread();

typedef void (*BackgroundThreadCallback)(void *);
void callInBackground(BackgroundThreadCallback cb, void *arg);

template<typename T>
void _deferredBackgroundDelete(void *ptr) {
	delete (T*)ptr;
}

/*
 * A deleter for a shared_ptr, which will defer the delete to the background thread.
 * */
template<typename T>
void deleteInBackground(T *ptr) {
	callInBackground(_deferredBackgroundDelete<T>, (void *)ptr);
}

}