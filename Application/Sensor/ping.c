// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "sensor.h"

// States for the local state machine
#define STATE_BUTTON                0

// Special request IDs
#define REQUESTID_MANUAL_PING       1
#define REQUESTID_TEMPLATE          2

// The filename of the test database.  Note that * is replaced by the
// gateway with the sensor's ID, while the # is a special character
// reserved by the notecard and notehub for a Sensor ID that is
// appended to the device ID within Events.
#define SENSORDATA_NOTEFILE         "*#data.qo"

// TRUE if we've successfully registered the template
static bool templateRegistered = false;

// Forwards
static void addNote(uint32_t count);
static bool registerNotefileTemplate(void);
static bool sendHealthLogMessage(bool immediate);

// Poller
void pingPoll(int sensorID, int state)
{

    // Switch based upon state
    switch (state) {

        // Sensor was just activated, so simulate the
        // sensor sampling something and adding a note
        // to the notefile.
    case STATE_ACTIVATED:

        // If the template isn't registered, do so
        if (!templateRegistered) {
            registerNotefileTemplate();
            schedSetCompletionState(sensorID, STATE_ACTIVATED, STATE_DEACTIVATED);
            traceLn("ping: template registration request");
            break;
        }

        // Add a note to the file
        static int notecount = 0;
        addNote(++notecount);
        schedSetCompletionState(sensorID, STATE_DEACTIVATED, STATE_DEACTIVATED);
        traceLn("ping: note queued");
        break;

        // When a button is pressed, send a log message
        // and wait for confirmation response all the
        // way from the notecard.
    case STATE_BUTTON:
        ledIndicateAck(1);
        sendHealthLogMessage(true);
        schedSetCompletionState(sensorID, STATE_DEACTIVATED, STATE_DEACTIVATED);
        traceLn("ping: sent health update");
        break;

    }

}

// Interrupt handler
void pingISR(int sensorID, uint16_t pins)
{

    // Set the state to button, and immediately schedule
    if ((pins & BUTTON1_Pin) != 0) {
        schedActivateNowFromISR(sensorID, true, STATE_BUTTON);
        return;
    }

}

// Send a note to the health log, and request a reply just
// as a validation of bidirectional communications continuity.
// Note that this method uses "sensorIgnoreTimeWindow()" which
// is NOT AT ALL a good practice because it can step on other
// devices' communications, however because this message is
// being sent by a button-press there is benefit in an
// immediate request/reply.
bool sendHealthLogMessage(bool immediate)
{

    // Create the new request
    J *req = NoteNewRequest("hub.log");
    if (req == NULL) {
        return false;
    }

    // Create a body for the request
    J *body = JCreateObject();
    if (body == NULL) {
        JDelete(req);
        return false;
    }

    // Format the health message
    char message[80];
    utilAddressToText(ourAddress, message, sizeof(message));
    if (sensorName[0] != '\0') {
        strlcat(message, " (", sizeof(message));
        strlcat(message, sensorName, sizeof(message));
        strlcat(message, ")", sizeof(message));
    }
    strlcat(message, " says hello", sizeof(message));
    JAddStringToObject(req, "text", message);

    // Add an ID to the request, which will be echo'ed
    // back in the response by the notecard itself.  This
    // helps us to identify the asynchronous response
    // without needing to have an additional state.
    JAddNumberToObject(req, "id", REQUESTID_MANUAL_PING);

    // If immediate send is requested, ignore the
    // time window and just send it now.  Also, set
    // the sync flag so that when it arrives on the
    // gateway it is synced immediately to the notehub.
    if (immediate) {
        sensorIgnoreTimeWindow();
        JAddBoolToObject(req, "sync", true);
    }

    // Send the request with the "true" argument meaning
    // that the notecard's response should be sent
    // all the way back from the gateway to us, rather
    // than discarding the response.
    noteSendToGatewayAsync(req, true);
    return true;

}

// Register the notefile template for our data
static bool registerNotefileTemplate()
{

    // Create the request
    J *req = NoteNewRequest("note.template");
    if (req == NULL) {
        return false;
    }

    // Create the body
    J *body = JCreateObject();
    if (body == NULL) {
        JDelete(req);
        return false;
    }

    // Add an ID to the request, which will be echo'ed
    // back in the response by the notecard itself.  This
    // helps us to identify the asynchronous response
    // without needing to have an additional state.
    JAddNumberToObject(req, "id", REQUESTID_TEMPLATE);

    // Fill-in request parameters.  Note that in order to minimize
    // the size of the over-the-air JSON we're using a special format
    // for the "file" parameter implemented by the gateway, in which
    // a "file" parameter beginning with * will have that character
    // substituted with the textified sensor address.
    JAddStringToObject(req, "file", SENSORDATA_NOTEFILE);

    // Fill-in the body template
    JAddNumberToObject(body, "count", TINT32);
    JAddStringToObject(body, "sensor", TSTRING(40));

    // Attach the body to the request, and send it to the gateway
    JAddItemToObject(req, "body", body);
    noteSendToGatewayAsync(req, true);
    return true;

}

// Send the periodic ping
static void addNote(uint32_t count)
{

    // Create the request
    J *req = NoteNewRequest("note.add");
    if (req == NULL) {
        return;
    }

    // Create the body
    J *body = JCreateObject();
    if (body == NULL) {
        JDelete(req);
        return;
    }

    // Set the target notefile
    JAddStringToObject(req, "file", SENSORDATA_NOTEFILE);

    // Fill-in the body
    JAddNumberToObject(body, "count", count);
    if (sensorName[0] != '\0') {
        JAddStringToObject(body, "sensor", sensorName);
    }

    // Attach the body to the request, and send it to the gateway
    JAddItemToObject(req, "body", body);
    noteSendToGatewayAsync(req, false);

}

// Gateway Response handler
void pingResponse(int sensorID, J *rsp)
{

    // If this is a response timeout, indicate as such
    if (rsp == NULL) {
        traceLn("ping: response timeout");
        return;
    }

    // See if there's an error
    char *err = JGetString(rsp, "err");
    if (err[0] != '\0') {
        trace("sensor error response: ");
        trace(err);
        traceNL();
        return;
    }

    // Flash the LED if this is a response to this specific ping request
    switch (JGetInt(rsp, "id")) {

    case REQUESTID_MANUAL_PING:
        ledIndicateAck(2);
        traceLn("ping: SUCCESSFUL response");
        break;

    case REQUESTID_TEMPLATE:
        templateRegistered = true;
        traceLn("ping: SUCCESSFUL template registration");
        break;
    }

}
