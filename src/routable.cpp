#include "synthizer/routable.hpp"

#include "synthizer/context.hpp"
#include "synthizer/router.hpp"
#include "synthizer/types.hpp"

#include <memory>

namespace synthizer {

RouteWriter::RouteWriter(const std::shared_ptr<Context> &ctx): BaseObject(ctx), writer_handle(ctx->getRouter()) {
}

RouteReader::RouteReader(const std::shared_ptr<Context> &ctx, AudioSample *buffer, unsigned int channels): BaseObject(ctx), reader_handle(ctx->getRouter(), buffer, channels) {
}

}