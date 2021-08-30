#pragma once

#include "base_object.hpp"

#include <memory>

namespace synthizer {
/**
 * Machinery to allow for pausing.  To support pausing, inherit from Pausable.
 * then, in the derived class, call shouldIncorporatePausableGain and getPausableGain to incorporate the gain fades
 * that Pausables need to introduce, isPaused() to determine whether or not to skip processing, and tickPausable
 * at the end of every audio block to drive the state machine.
 *
 * See documentation on the member methods for more details as to what's going on.
 *
 * because of the base classes for routers, which currently need to be inserted into the hierarchy between
 * baseObject and the object in question, Pausable is used as a mixin.
 * */

class Context;

/**
 * The state machine goes:
 *
 * playing -> pausing -> paused for pausing
 * paused -> unpausing -> playing for unpausing
 *
 * Where it's possible to transition from pausing to unpausing if the user pounds on pause/play really fast.
 *
 * the two intermediate states are used to introduce gain fades.
 * */
enum class PauseState {
  Playing,
  Pausing,
  Paused,
  Unpausing,
};

class Pausable {
public:
  virtual ~Pausable() {}

  /**
   * Update the state machine. Should be called at the end of every audio tick, after any use of
   * shouldIncorporatePauseGain and getPauseGain.
   * */
  void tickPausable();

  /**
   * If this returns true, the object should short-circuit processing.
   * */
  bool isPaused();

  PauseState getPauseState();

  /**
   * If this returns true, the Pausable wishes to introduce some sort of fade in addition to the gain
   * of the object itself.  To determine what the final gain of this fade is, call getPausableGain.
   * */
  bool shouldIncorporatePausableGain();

  /**
   * Return the gain the Pausable wishes the object to move to.  Essentially, what should happen is:
   *
   * float effective_gain = my_gain * this->getPausableGain()
   *
   * if shouldIncorporatePausableGain returned true.  note that
   * in the case where shouldIncorporatePausableGain returns false, this just returns 1.  This allows one to do:
   *
   * if (this->acquireGain(new_gain) || this->shouldIncorporatePausableGain()) { update_gain(); }
   * */
  float getPausableGain();

  /**
   * Control whether or not this Pausable is paused or playing. Used from the C API to expose
   * pausing.
   * */
  void play();
  void pause();

private:
  PauseState pause_state = PauseState::Playing;
};

} // namespace synthizer