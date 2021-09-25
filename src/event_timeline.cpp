#include "synthizer/event_timeline.hpp"

#include "synthizer/context.hpp"
#include "synthizer/events.hpp"

#include <memory>

namespace synthizer {

void EventTimeline::tick(const std::shared_ptr<Context> &ctx, const std::shared_ptr<BaseObject> &source, double time) {
  (void)ctx;

  this->timeline.tick(time, [&](auto *i) { sendUserAutomationEvent(ctx, source, i->param); });
}

} // namespace synthizer
