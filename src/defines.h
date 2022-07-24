#pragma once

#include <cstdint>

#define KB(x) (static_cast<uint64_t>(1024 * x))
#define MB(x) (1024 * KB(x))
#define GB(x) (1024 * MB(x))

#define global_variable static;