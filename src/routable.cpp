#include "synthizer/routable.hpp"

#include "synthizer/context.hpp"
#include "synthizer/router.hpp"
#include "synthizer/types.hpp"

#include <memory>

namespace synthizer {

RouteOutput::RouteOutput(const std::shared_ptr<Context> &ctx): BaseObject(ctx), output_handle(ctx->getRouter()) {
}

RouteInput::RouteInput(const std::shared_ptr<Context> &ctx, float *buffer, unsigned int channels): BaseObject(ctx), input_handle(ctx->getRouter(), buffer, channels) {
}

}
