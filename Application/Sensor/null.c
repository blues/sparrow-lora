// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "sensor.h"

// Poller
void nullPoll(int sensorID, int state)
{

    // Switch based upon state
    switch (state) {

    // On the first poll it's a good place to
    // create a template on the notecard.
    case STATE_ONCE:
        break;

    // Sensor was just activated, so do some work
    // and then set state to DEACTIVATED when done.
    case STATE_ACTIVATED:
        schedSetState(sensorID, STATE_DEACTIVATED, "done");
        break;
    }

}
