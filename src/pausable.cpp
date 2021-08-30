#include "synthizer/pausable.hpp"

#include "synthizer.h"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/memory.hpp"

namespace synthizer {

void Pausable::tickPausable() {
  if (this->pause_state == PauseState::Pausing) {
    this->pause_state = PauseState::Paused;
  } else if (this->pause_state == PauseState::Unpausing) {
    this->pause_state = PauseState::Playing;
  }
}

bool Pausable::isPaused() { return this->pause_state == PauseState::Paused; }

PauseState Pausable::getPauseState() { return this->pause_state; }

bool Pausable::shouldIncorporatePausableGain() {
  return this->pause_state == PauseState::Pausing || this->pause_state == PauseState::Unpausing;
}

float Pausable::getPausableGain() {
  if (this->pause_state == PauseState::Playing || this->pause_state == PauseState::Unpausing) {
    return 1.0f;
  }
  return 0.0f;
}

void Pausable::pause() {
  if (this->pause_state != PauseState::Paused) {
    this->pause_state = PauseState::Pausing;
  }
}

void Pausable::play() {
  if (this->pause_state != PauseState::Playing) {
    this->pause_state = PauseState::Unpausing;
  }
}

} // namespace synthizer

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_pause(syz_Handle object) {
  SYZ_PROLOGUE
  auto p = fromC<Pausable>(object);
  auto b = typeCheckedDynamicCast<BaseObject>(p);
  b->getContextRaw()->enqueueReferencingCallbackCommand(
      true, [&](auto &p) { p->pause(); }, p);
  return 0;
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_play(syz_Handle object) {
  SYZ_PROLOGUE
  auto p = fromC<Pausable>(object);
  auto b = typeCheckedDynamicCast<BaseObject>(p);
  b->getContextRaw()->enqueueReferencingCallbackCommand(
      true, [&](auto &p) { p->play(); }, p);
  return 0;
  SYZ_EPILOGUE
}
