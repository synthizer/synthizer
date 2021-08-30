#include "synthizer/shared_object.hpp"

#include <stddef.h>
#include <stdint.h>

/**
 * Code for loading shared objects/dlls.
 *
 * The following was taken from miniaudio, which is in the public domain, then modified for our use case.
 *
 * As a consequence it's a bit ugly.
 * */

/* Platform/backend detection. */
#ifdef _WIN32
#define MA_WIN32
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP || WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
#define MA_WIN32_UWP
#else
#define MA_WIN32_DESKTOP
#endif
#else
#define MA_POSIX
#ifdef __unix__
#define MA_UNIX
#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define MA_BSD
#endif
#endif
#ifdef __linux__
#define MA_LINUX
#endif
#ifdef __APPLE__
#define MA_APPLE
#endif
#ifdef __ANDROID__
#define MA_ANDROID
#endif
#ifdef __EMSCRIPTEN__
#define MA_EMSCRIPTEN
#endif
#endif

#ifdef MA_WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace synthizer {
static void *sharedObjectOpen(const char *filename) {
  void *handle;

#ifdef _WIN32
  /* Note: miniaudio doesn't handle unicode paths properly; if we ever update this code we need to be careful to patch.
   */
  WCHAR filenameW[4096];
  if (MultiByteToWideChar(CP_UTF8, 0, filename, -1, filenameW, sizeof(filenameW)) == 0) {
    return nullptr;
  }
#ifdef MA_WIN32_DESKTOP
  handle = (void *)LoadLibraryA(filename);
#else
  handle = (void *)LoadPackagedLibrary(filenameW, 0);
#endif
#else
  handle = (void *)dlopen(filename, RTLD_NOW);
#endif
  return handle;
}

static void sharedObjectClose(void *handle) {
#ifdef _WIN32
  FreeLibrary((HMODULE)handle);
#else
  dlclose((void *)handle);
#endif
}

static void *sharedObjectGetSymbol(void *handle, const char *symbol) {
  void *proc;

#ifdef _WIN32
  proc = (void *)GetProcAddress((HMODULE)handle, symbol);
#else
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
  proc = (void *)dlsym(handle, symbol);
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic pop
#endif
#endif

  return proc;
}

SharedObject::SharedObject(const char *path) {
  this->handle = sharedObjectOpen(path);
  if (this->handle == nullptr) {
    throw SharedObjectOpenError{};
  }
}

SharedObject::~SharedObject() { sharedObjectClose(this->handle); }

void *SharedObject::getSymbol(const char *symbol) {
  void *ret = sharedObjectGetSymbol(this->handle, symbol);
  if (ret == nullptr) {
    throw MissingSymbolError(symbol);
  }
  return ret;
}

} // namespace synthizer
