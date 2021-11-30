// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// Sensor Scheduler
#include "sensor.h"

// Current operational state of a sensor
typedef struct {
    bool disabled;
    bool active;
    bool requestPending;
    bool responsePending;
    uint32_t requestSentTimeValid;
    uint32_t requestSentTime;
    uint32_t activationBaseTime;
    uint32_t lastActivatedTime;
    int currentState;
    int completionSuccessState;
    int completionErrorState;
} sensorState;

static sensorState *state = NULL;
static sensorConfig *sensor = NULL;
static int sensors = 0;

// Forwards
uint32_t secsUntilDue(uint32_t alignmentBaseSecs, uint32_t nowSecs, uint32_t lastSecs, uint32_t periodSecs);
uint32_t nextActivationDueSecs(int i);

// Init the sensor package
void schedInit()
{

    // Initialize all sensors
    initSensors();

    // Start the sensor timer so that we get called back to schedule
    sensorTimerStart();

}

// Register a sensor, returning sensor ID (or -1 if failure)
int schedRegisterSensor(sensorConfig *sensorToRegister)
{
    int newSensorID = -1;

    // Allocate a new sensor and state table
    sensorConfig *newConfig = malloc((sensors+1)*sizeof(sensorConfig));
    if (newConfig == NULL) {
        return newSensorID;
    }
    sensorState *newState = malloc((sensors+1)*sizeof(sensorState));
    if (newState == NULL) {
        free(newConfig);
        return newSensorID;
    }

    // switch to, and initialize, new config and state
    if (sensor != NULL) {
        memcpy(newConfig, sensor, sensors * sizeof(sensorConfig));
        free(sensor);
        memcpy(newState, state, sensors * sizeof(sensorState));
        free(state);
    }
    memcpy(&newConfig[newSensorID], sensorToRegister, sizeof(sensorConfig));
    sensor = newConfig;
    memset(&newState[newSensorID], 0, sizeof(sensorConfig));
    state = newState;
    state[newSensorID].currentState = STATE_ONCE;
    state[newSensorID].completionSuccessState = STATE_UNDEFINED;
    state[newSensorID].completionErrorState = STATE_UNDEFINED;

    // We now have coherent config and state tables
    sensors++;

    // Done
    return newSensorID;
}

// Get the sensor name
const char *schedSensorName(int sensorID)
{
    return sensor[sensorID].name;
}

// Activate ASAP, as if from an ISR
bool schedActivateNowFromISR(int sensorID, bool interruptIfActive, int nextState)
{
    if (sensorID < 0) {
        return false;
    }
    if (state[sensorID].active && !interruptIfActive) {
        return false;
    }
    state[sensorID].currentState = nextState;
    state[sensorID].lastActivatedTime = 0;
    sensorTimerWakeFromISR();
    return true;
}

// Disable this sensor permanently, for example in case of hardware failure
void schedDisable(int sensorID)
{
    if (!state[sensorID].disabled) {
        state[sensorID].disabled = true;
        APP_PRINTF("%s PERMANENTLY DISABLED\r\n", sensor[sensorID].name);
    }
}

// Dispatch the interrupt to all sensors' ISRs
void schedDispatchISR(uint16_t pins)
{
    for (int i=0; i<sensors; i++) {
        if (!state[i].disabled && sensor[i].interruptFn != NULL) {
            sensor[i].interruptFn(i, pins);
        }
    }
}

// Translate a state ID to a state name
char *schedStateName(int state)
{
    switch (state) {
    case STATE_UNDEFINED:
        return "UNDEFINED";
    case STATE_ONCE:
        return "ONCE";
    case STATE_ACTIVATED:
        return "ACTIVATED";
    case STATE_DEACTIVATED:
        return "DEACTIVATED";
    case STATE_SENDING_REQUEST:
        return "SENDING_REQUEST";
    case STATE_RECEIVING_RESPONSE:
        return "RECEIVING_RESPONSE";
    }
    static char other[20];
    JItoA(state, other);
    return other;
}

// See if the sensor is currently active
bool schedIsActive(int sensorID)
{
    return state[sensorID].active;
}

// Get the current state for a sensor
int schedGetState(int sensorID)
{
    return state[sensorID].currentState;
}

// Set the current state for a sensor
void schedSetState(int sensorID, int newstate, const char *why)
{
    if (state[sensorID].currentState != newstate) {
        state[sensorID].currentState = newstate;
        APP_PRINTF("%s now %s", sensor[sensorID].name, schedStateName(newstate));
        if (why != NULL) {
            APP_PRINTF(" (%s)\r\n", why);
        } else {
            APP_PRINTF("\r\n");
        }
    }
}

// Set the state that should be assumed after request or response completion
void schedSetCompletionState(int sensorID, int successstate, int errorstate)
{
    if (state[sensorID].completionSuccessState != successstate || state[sensorID].completionErrorState != errorstate) {
        state[sensorID].completionSuccessState = successstate;
        state[sensorID].completionErrorState = errorstate;
        APP_PRINTF("%s state will be set to %s on success, or %s on error\r\n",
                   sensor[sensorID].name, schedStateName(successstate), schedStateName(errorstate));
    }
}

// Note that we're sending a request, so we can change the state
void schedSendingRequest(bool responseRequested)
{
    for (int i=0; i<sensors; i++) {
        if (!state[i].disabled && state[i].active) {
            state[i].requestSentTime = NoteTimeST();
            state[i].requestSentTimeValid = NoteTimeValidST();
            state[i].requestPending = true;
            state[i].responsePending = responseRequested;
            state[i].completionSuccessState = STATE_DEACTIVATED;
            state[i].completionErrorState = STATE_DEACTIVATED;
            schedSetState(i, STATE_SENDING_REQUEST, NULL);
            break;
        }
    }
}

// Notify that a request has been completed
void schedRequestCompleted(void)
{
    for (int i=0; i<sensors; i++) {
        if (!state[i].disabled && state[i].active) {
            if (state[i].requestPending) {
                state[i].requestPending = false;
                if (state[i].responsePending) {
                    schedSetState(i, STATE_RECEIVING_RESPONSE, "waiting for response");
                } else {
                    schedSetState(i, state[i].completionSuccessState, "request completed");
                }
            }
            break;
        }
    }
}

// Dispatch a gateway response to the active sensor's processing method
void schedResponseCompleted(J *rsp)
{
    for (int i=0; i<sensors; i++) {
        if (!state[i].disabled && state[i].active) {
            if (state[i].responsePending) {
                state[i].responsePending = false;
                schedSetState(i, state[i].completionSuccessState, "response completed");
                if (sensor[i].responseFn != NULL) {
                    sensor[i].responseFn(i, rsp);
                }
            }
        }
    }
}

// Process a request or response timeout
void schedRequestResponseTimeout(void)
{
    for (int i=0; i<sensors; i++) {
        if (!state[i].disabled && state[i].active) {
            if (state[i].requestPending || state[i].responsePending) {
                schedSetState(i, state[i].completionErrorState, "error/timeout");
                state[i].requestPending = false;
                state[i].responsePending = false;
            }
        }
    }
}

// Process a request or response timeout
void schedRequestResponseTimeoutCheck(void)
{

    // We can't really check timeouts until we have a valid time
    if (!NoteTimeValidST()) {
        return;
    }

    // See if any of the pending responses have timed out
    for (int i=0; i<sensors; i++) {
        if (!state[i].disabled && state[i].active) {
            if (state[i].requestPending || state[i].responsePending) {
                if (!state[i].requestSentTimeValid) {
                    state[i].requestSentTimeValid = true;
                    state[i].requestSentTime = NoteTimeST();
                }
                if (NoteTimeST() > state[i].requestSentTime + appTransmitWindowWaitMaxSecs()) {
                    schedSetState(i, state[i].completionErrorState, "timeout");
                    state[i].requestPending = false;
                    state[i].responsePending = false;
                }
            }
        }
    }

}

// Compute seconds until due, optionally aligned to a base.  If the event is currently or past-due, 0 is returned
uint32_t secsUntilDue(uint32_t alignmentBaseSecs, uint32_t nowSecs, uint32_t lastSecs, uint32_t periodSecs)
{

    // If we'd like aligned periods, use the boot time as the base seconds for these calculations.
    // However, because the host app may also be aligning its own calculations exactly to boot time,
    // we offset the boot time by an arbitrary number of minutes as way of desynchronizing
    // the point at which we sample and the point at which time we upload, so that they don't
    // draw power at exactly the same moment.
    uint32_t baseSecs = alignmentBaseSecs;

    // If the event has never occurred, don't delay it.  This is simply a policy decision
    // favoring promptness rather than having events wait until the first aligned boundary.
    if (lastSecs == 0) {
        return 0;
    }

    // Compute the unadjusted due time
    uint32_t dueSecs = lastSecs + periodSecs;

    // If we don't have base seconds, we can't do any rounding
    if (baseSecs != 0) {

        // Defensive coding in case arguments are invalid
        if (baseSecs > lastSecs) {
            baseSecs = lastSecs;
        }
        if (periodSecs == 0) {
            periodSecs = 1;
        }

        // Compute the start of the next period
        dueSecs = baseSecs + ((((lastSecs - baseSecs) / periodSecs) + 1) * periodSecs);

    }

    // Return 0 if due, else the number of seconds until due
    if (nowSecs >= dueSecs) {
        return 0;
    }
    return (dueSecs - nowSecs);

}

// Find the next activation time for a sensor
uint32_t nextActivationDueSecs(int i)
{
    uint32_t now = NoteTimeST();

    // The time when this sensor was first activated after having a valid time
    if (state[i].activationBaseTime == 0 && NoteTimeValidST()) {
        state[i].activationBaseTime = now;
    }

    // If we need to get back on track for one of several conditions, schedule it now
    if (state[i].lastActivatedTime == 0
            || state[i].lastActivatedTime < state[i].activationBaseTime
            || state[i].lastActivatedTime > now) {
        return 0;
    }

    // Compute the next period
    return secsUntilDue(state[i].activationBaseTime, now, state[i].lastActivatedTime, sensor[i].activationPeriodSecs);

}

// Primary scheduler poller, returns the time of next scheduling (or 0 to be called back immediately)
uint32_t schedPoll()
{
    static int lastActiveSensor = -1;
    uint32_t now = NoteTimeST();

    // Don't poll if we're pairing or if we can't do any work because
    // we don't yet know the gateway's address
    if (ledIsPairInProgress()) {
        return NoteTimeST() + (60*60*24);
    }

    // Poll the active sensor
    for (int i=0; i<sensors; i++) {
        if (!state[i].disabled && state[i].active && sensor[i].pollFn != NULL) {
            sensor[i].pollFn(i, state[i].currentState);
            if (state[i].currentState == STATE_ONCE) {
                state[i].currentState = STATE_ACTIVATED;
                sensor[i].pollFn(i, state[i].currentState);
            }
            if (state[i].currentState == STATE_DEACTIVATED) {

                // Deactivate and also set state to a plain "activate" for next iteration.  This
                // state can be overridden by an ISR that wakes it for some other reason.
                state[i].active = false;
                state[i].currentState = STATE_ACTIVATED;
                APP_PRINTF("%s deactivated\r\n", sensor[i].name);
                break;

            }
            return (now + sensor[i].pollIntervalSecs);
        }
    }

    // There are no active sensors, so see what's available to activate
    while (true) {

        // Nothing is active, so we will loop to find the next active sensor.  However,
        // to be fair, we process them in a round-robin manner by starting with the
        // sensor immediately after the last-activated sensor, and by remembering
        // the first one in the case where multiple are due at the same time.
        uint32_t earliestDueSecs = 0;
        int earliestDueSensor = -1;
        int nextSensor = lastActiveSensor;
        for (int s=0; s<sensors; s++) {
            int i = (++nextSensor) % sensors;
            if (!state[i].disabled) {
                uint32_t thisDueSecs = nextActivationDueSecs(i);
                if (earliestDueSensor == -1 || thisDueSecs < earliestDueSecs) {
                    earliestDueSecs = thisDueSecs;
                    earliestDueSensor = i;
                }
            }
        }

        // Something should be schedulable, even if it's a long time out.  This
        // is just defensive coding to ensure that we have some kind of wakeup.
        if (earliestDueSensor == -1) {
            APP_PRINTF("*** no sensors enabled ***\r\n");
            return now + 60*60;
        }

        // If something is due but not ready to activate, return the time when it's due.  We
        // add 1 to increase the chance that it will actually be ready when the timer expires.
        if (earliestDueSecs > 0) {
            APP_PRINTF("%s next up in %ds\r\n", sensor[earliestDueSensor].name, earliestDueSecs);
            return now + earliestDueSecs + 1;
        }

        // Mark this as the last sensor activated, even if the sensor
        // refuses activation below.  This ensures round-robin behavior.
        lastActiveSensor = earliestDueSensor;
        state[lastActiveSensor].lastActivatedTime = now;
        state[lastActiveSensor].active = true;

        // A sensor is due now.  Activate it and come back quickly.
        if (sensor[lastActiveSensor].activateFn == NULL) {
            break;
        }
        if (sensor[lastActiveSensor].activateFn(lastActiveSensor)) {
            break;
        }

        // The activation failed, so just move on to the next one
        state[lastActiveSensor].active = false;
        APP_PRINTF("%s declined activation\r\n", sensor[lastActiveSensor].name);

    }

    // Mark the sensor as active
    APP_PRINTF("%s activated with %ds activation period and %ds poll interval\r\n",
               sensor[lastActiveSensor].name, sensor[lastActiveSensor].activationPeriodSecs, sensor[lastActiveSensor].pollIntervalSecs);
    return now;

}
