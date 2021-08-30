#include "synthizer.h"

#include "synthizer/logging.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <utility>

namespace synthizer {

/* This implementation is not sufficient for production usage; TODO: log from a thread once we have a lockfree/waitfree
 * queue. */

static std::atomic<int> logging_enabled{0};
static std::atomic<enum SYZ_LOG_LEVEL> log_level = SYZ_LOG_LEVEL_ERROR;

static std::atomic<int> next_thread_id = 1;
thread_local static int thread_id = 0;
thread_local static std::string thread_purpose;

void setThreadPurpose(std::string purpose) { thread_purpose = std::move(purpose); }

static int getThreadId() {
  if (thread_id == 0) {
    thread_id = next_thread_id.fetch_add(1, std::memory_order_relaxed);
  }
  return thread_id;
}

static const char *getThreadPurposeAsChar() {
  return thread_purpose.size() > 0 ? thread_purpose.c_str() : "unknown-thread";
}

bool isLoggingEnabled() { return logging_enabled.load() == 1; }

void disableLogging() { logging_enabled.store(0); }

void logToStderr() { logging_enabled.store(1); }

enum SYZ_LOG_LEVEL getLogLevel() { return log_level.load(std::memory_order_relaxed); }

void setLogLevel(enum SYZ_LOG_LEVEL level) { log_level.store(level, std::memory_order_relaxed); }

static const char *logLevelAsString() {
  switch (getLogLevel()) {
  case SYZ_LOG_LEVEL_ERROR:
    return "error";
  case SYZ_LOG_LEVEL_WARN:
    return "warn";
  case SYZ_LOG_LEVEL_INFO:
    return "info";
  case SYZ_LOG_LEVEL_DEBUG:
    return "debug";
  }
  return "";
}

static const std::size_t MAX_LOG_MESSAGE_LENGTH = 1025;
thread_local static char log_message_buf[MAX_LOG_MESSAGE_LENGTH + 1];
void log(enum SYZ_LOG_LEVEL level, const char *format, ...) {
  std::va_list args;

  va_start(args, format);

  if (logging_enabled.load(std::memory_order_relaxed) != 0 && level <= getLogLevel()) {
    auto wrote = std::vsnprintf(log_message_buf, MAX_LOG_MESSAGE_LENGTH, format, args);
    log_message_buf[wrote] = '\0';
    fprintf(stderr, "%s(%s %i) %s\n", logLevelAsString(), getThreadPurposeAsChar(), getThreadId(), log_message_buf);
  }

  va_end(args);
}

} // namespace synthizer

SYZ_CAPI void syz_setLogLevel(enum SYZ_LOG_LEVEL level) { synthizer::setLogLevel(level); }
