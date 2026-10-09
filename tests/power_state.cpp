#include "../Sources/Helper/PowerState.h"

static_assert(BDBVerifiedPowerOff(true, false, 0, true, 1), "Normal off state");
static_assert(BDBVerifiedPowerOff(true, false, 0, true, 11),
    "Regression: crashed driver state must not hide verified physical off");
static_assert(!BDBVerifiedPowerOff(false, false, 0, true, 11), "Power read is required");
static_assert(!BDBVerifiedPowerOff(true, true, 0, true, 11), "PMU on is never off");
static_assert(!BDBVerifiedPowerOff(true, false, 1, true, 11), "Radio power flag must be zero");
static_assert(!BDBVerifiedPowerOff(true, false, 2, true, 11), "Invalid radio flag is refused");
static_assert(!BDBVerifiedPowerOff(true, false, 0, false, 11), "Backpower GPIO must be input");
static_assert(!BDBVerifiedPowerOff(true, false, 0, true, 0), "Unknown driver state is refused");
static_assert(!BDBVerifiedPowerOff(true, false, 0, true, 5), "Other driver states are refused");
static_assert(!BDBVerifiedPowerOff(true, false, 0, true, UINT64_MAX), "Failed state read is refused");
static_assert(!BDBVerifiedPowerOff(true, true, 1, false, 1),
    "Logical Off alone cannot override physical power-on");
