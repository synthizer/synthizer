#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/context.hpp"
#include "synthizer/events.hpp"
#include "synthizer/memory.hpp"

#include <concurrentqueue.h>

#include <cassert>
#include <memory>
#include <utility>

SYZ_CAPI void syz_eventDeinit(struct syz_Event *event) {
  /*
   * Deinitialize an event by decrementing reference counts as necessary. We'll just
   * go through the C API and not check errors: syz_handleDecRef always succeeds save for programmer error.
   * */

  /* Invalid events are zero-initialized, so there's no need to check. */
  syz_handleDecRef(event->source);
  syz_handleDecRef(event->context);
}
