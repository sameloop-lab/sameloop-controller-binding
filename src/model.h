// SPDX-License-Identifier: MIT
#ifndef SAMELOOP_CONTROL_MODEL_H
#define SAMELOOP_CONTROL_MODEL_H
#include <stdbool.h>
#include <math.h>
static inline bool sample_available(bool valid, double t, double stamp, double now) {
    return valid && isfinite(t) && t >= -40 && t <= 150 && isfinite(stamp)
        && stamp > 0 && now >= stamp && now - stamp < 1.0;
}
static inline double control_duty(bool automatic, bool available, double t, double setpoint) {
    if (!automatic || !available) return 0;
    return fmax(0,fmin(100,20*(t-setpoint)));
}
#endif
