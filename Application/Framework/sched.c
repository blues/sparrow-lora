// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// App Scheduler
#include "framework.h"

// Current operational state of a scheduled app
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
} schedAppState;

static schedAppState *state = NULL;
static schedAppConfig *config = NULL;
static int apps = 0;

// Forwards
uint32_t secsUntilDue(uint32_t alignmentBaseSecs, uint32_t nowSecs, uint32_t lastSecs, uint32_t periodSecs);
uint32_t nextActivationDueSecs(int i);

// Init the app scheduler
void schedInit()
{

    // Initialize all scheduled apps
    schedAppInit();

    // Start the sensor utility timer so that we get called back to schedule
    sensorTimerStart();

}

// Register an app to be scheduled, returning app ID (or -1 if failure)
int schedRegisterApp(schedAppConfig *appToRegister)
{
    int newAppID = -1;

    // Allocate a new config and state table
    schedAppConfig *newConfig = malloc((apps+1)*sizeof(schedAppConfig));
    if (newConfig == NULL) {
        return newAppID;
    }
    schedAppState *newState = malloc((apps+1)*sizeof(schedAppState));
    if (newState == NULL) {
        free(newConfig);
        return newAppID;
    }

    // switch to, and initialize, new config and state
    if (apps) {
        memcpy(newConfig, config, apps * sizeof(schedAppConfig));
        free(config);
        memcpy(newState, state, apps * sizeof(schedAppState));
        free(state);
    }
    newAppID = apps;
    memcpy(&newConfig[newAppID], appToRegister, sizeof(schedAppConfig));
    config = newConfig;
    memset(&newState[newAppID], 0, sizeof(schedAppConfig));
    state = newState;
    state[newAppID].currentState = STATE_ONCE;
    state[newAppID].completionSuccessState = STATE_UNDEFINED;
    state[newAppID].completionErrorState = STATE_UNDEFINED;

    // We now have coherent config and state tables
    apps++;

    // Done
    return newAppID;
}

// Get the name of the scheduled app
const char *schedAppName(int appID)
{
    return config[appID].name;
}

// Activate ASAP, as if from an ISR
bool schedActivateNowFromISR(int appID, bool interruptIfActive, int nextState)
{
    if (appID < 0) {
        return false;
    }
    if (state[appID].active && !interruptIfActive) {
        return false;
    }
    state[appID].currentState = nextState;
    state[appID].lastActivatedTime = 0;
    sensorTimerWakeFromISR();
    return true;
}

// Disable this app permanently, for example in case of hardware failure
void schedDisable(int appID)
{
    if (!state[appID].disabled) {
        state[appID].disabled = true;
        APP_PRINTF("%s PERMANENTLY DISABLED\r\n", config[appID].name);
    }
}

// Dispatch the interrupt to all apps' ISRs
void schedDispatchISR(uint16_t pins)
{
    for (int i=0; i<apps; i++) {
        if (!state[i].disabled && config[i].interruptFn != NULL) {
            config[i].interruptFn(i, pins, config[i].appContext);
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

// See if the app is currently active
bool schedIsActive(int appID)
{
    return state[appID].active;
}

// Get the current state for an app
int schedGetState(int appID)
{
    return state[appID].currentState;
}

// Set the current state for an app
void schedSetState(int appID, int newstate, const char *why)
{
    if (state[appID].currentState != newstate) {
        state[appID].currentState = newstate;
        APP_PRINTF("%s now %s", config[appID].name, schedStateName(newstate));
        if (why != NULL) {
            APP_PRINTF(" (%s)\r\n", why);
        } else {
            APP_PRINTF("\r\n");
        }
    }
}

// Set the state that should be assumed after request or response completion
void schedSetCompletionState(int appID, int successstate, int errorstate)
{
    if (state[appID].completionSuccessState != successstate || state[appID].completionErrorState != errorstate) {
        state[appID].completionSuccessState = successstate;
        state[appID].completionErrorState = errorstate;
        APP_PRINTF("%s state will be set to %s on success, or %s on error\r\n",
                   config[appID].name, schedStateName(successstate), schedStateName(errorstate));
    }
}

// Note that we're sending a request, so we can change the state
void schedSendingRequest(bool responseRequested)
{
    for (int i=0; i<apps; i++) {
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
    for (int i=0; i<apps; i++) {
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

// Dispatch a gateway response to the active app's processing method
void schedResponseCompleted(J *rsp)
{
    for (int i=0; i<apps; i++) {
        if (!state[i].disabled && state[i].active) {
            if (state[i].responsePending) {
                state[i].responsePending = false;
                schedSetState(i, state[i].completionSuccessState, "response completed");
                if (config[i].responseFn != NULL) {
                    config[i].responseFn(i, rsp, config[i].appContext);
                }
            }
        }
    }
}

// Process a request or response timeout
void schedRequestResponseTimeout(void)
{
    for (int i=0; i<apps; i++) {
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
    for (int i=0; i<apps; i++) {
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

// Find the next activation time for an app
uint32_t nextActivationDueSecs(int i)
{
    uint32_t now = NoteTimeST();

    // The time when this app was first activated after having a valid time
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
    return secsUntilDue(state[i].activationBaseTime, now, state[i].lastActivatedTime, config[i].activationPeriodSecs);

}

// Primary scheduler poller, returns the time of next scheduling (or 0 to be called back immediately)
uint32_t schedPoll()
{
    static int lastActiveApp = -1;
    uint32_t now = NoteTimeST();

    // Don't poll if we're pairing or if we can't do any work because
    // we don't yet know the gateway's address
    if (ledIsPairInProgress() || ledIsPairMandatory()) {
        return NoteTimeST() + (60*60*24);
    }

    // Poll the active app
    for (int i=0; i<apps; i++) {
        if (!state[i].disabled && state[i].active && config[i].pollFn != NULL) {
            config[i].pollFn(i, state[i].currentState, config[i].appContext);
            if (state[i].currentState == STATE_ONCE) {
                state[i].currentState = STATE_ACTIVATED;
                config[i].pollFn(i, state[i].currentState, config[i].appContext);
            }
            if (state[i].currentState == STATE_DEACTIVATED) {

                // Deactivate and also set state to a plain "activate" for next iteration.  This
                // state can be overridden by an ISR that wakes it for some other reason.
                state[i].active = false;
                state[i].currentState = STATE_ACTIVATED;
                APP_PRINTF("%s deactivated\r\n", config[i].name);
                break;

            }
            return (now + config[i].pollPeriodSecs);
        }
    }

    // There are no active apps, so see what's available to activate
    while (true) {

        // Nothing is active, so we will loop to find the next active app.  However,
        // to be fair, we process them in a round-robin manner by starting with the
        // app immediately after the last-activated app, and by remembering
        // the first one in the case where multiple are due at the same time.
        uint32_t earliestDueSecs = 0;
        int earliestDueApp = -1;
        int nextApp = lastActiveApp;
        for (int s=0; s<apps; s++) {
            int i = (++nextApp) % apps;
            if (!state[i].disabled) {
                uint32_t thisDueSecs = nextActivationDueSecs(i);
                if (earliestDueApp == -1 || thisDueSecs < earliestDueSecs) {
                    earliestDueSecs = thisDueSecs;
                    earliestDueApp = i;
                }
            }
        }

        // Something should be schedulable, even if it's a long time out.  This
        // is just defensive coding to ensure that we have some kind of wakeup.
        if (earliestDueApp == -1) {
            APP_PRINTF("*** no apps enabled ***\r\n");
            return now + 60*60;
        }

        // If something is due but not ready to activate, return the time when it's due.  We
        // add 1 to increase the chance that it will actually be ready when the timer expires.
        if (earliestDueSecs > 0) {
            APP_PRINTF("%s next up in %ds\r\n", config[earliestDueApp].name, earliestDueSecs);
            return now + earliestDueSecs + 1;
        }

        // Mark this as the last app activated, even if the app
        // refuses activation below.  This ensures round-robin behavior.
        lastActiveApp = earliestDueApp;
        state[lastActiveApp].lastActivatedTime = now;
        state[lastActiveApp].active = true;

        // An app is due now.  Activate it and come back quickly.
        if (config[lastActiveApp].activateFn == NULL) {
            break;
        }
        if (config[lastActiveApp].activateFn(lastActiveApp, config[lastActiveApp].appContext)) {
            break;
        }

        // The activation failed, so just move on to the next one
        state[lastActiveApp].active = false;
        APP_PRINTF("%s declined activation\r\n", config[lastActiveApp].name);

    }

    // Mark the app as active
    APP_PRINTF("%s activated with %ds activation period and %ds poll interval\r\n",
               config[lastActiveApp].name, config[lastActiveApp].activationPeriodSecs, config[lastActiveApp].pollPeriodSecs);
    return now;

}
