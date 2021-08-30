#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/config.hpp"
#include "synthizer/router.hpp"

#include <memory>

namespace synthizer {

/*
 * These classes are BaseObject, but for things that want to participate in routing as either a input or an output.
 *
 * At the moment, Synthizer only supports routing one level, so there's no way to be both.
 * */
class RouteOutput : public BaseObject {
public:
  RouteOutput(const std::shared_ptr<Context> &ctx);

  router::OutputHandle *getOutputHandle() override { return &this->output_handle; }

private:
  router::OutputHandle output_handle;
};

class RouteInput : public BaseObject {
public:
  RouteInput(const std::shared_ptr<Context> &ctx, float *buffer, unsigned int channels);

  router::InputHandle *getInputHandle() override { return &this->input_handle; }

private:
  router::InputHandle input_handle;
};

} // namespace synthizer