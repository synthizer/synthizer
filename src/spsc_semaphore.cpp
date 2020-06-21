#include "synthizer/spsc_semaphore.hpp"

#include "sema.h"

#include <vector>

namespace synthizer {

thread_local std::vector<Semaphore*> SPSCSemaphore::sema_pool{};

}
