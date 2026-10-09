#ifndef BDB_POWER_STATE_H
#define BDB_POWER_STATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#define BDB_INLINE constexpr
#else
#define BDB_INLINE static inline
#endif

// A crashed logical state can be reported after the modem is already off.
// Neither logical state is proof of power-off without all physical checks.
BDB_INLINE bool BDBVerifiedPowerOff(bool powerRead, bool powerOn,
    uint8_t radioPowerOn, bool gpioInput, uint64_t driverState) {
    return powerRead && !powerOn && radioPowerOn == 0 && gpioInput &&
        (driverState == 1 || driverState == 11);
}

#undef BDB_INLINE
#endif
