#pragma once
#include <algorithm>
#include <cstdint>
// SDL 3.4 GIP encodes motor strength as uint8_t(value / 655).
// Preserve a requested weak effect at the lowest representable nonzero level.
// Zero must remain zero; triggers keep their separate configured strength.
inline uint16_t gipMainLevel(int value) {
    return value <= 0 ? 0 : uint16_t(std::clamp(value, 655, 65535));
}
