// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "board.h"
#include "app.h"

// Housekeeping
uint32_t dbLastUpdateTime = 0;
uint32_t envLastUpdateTime = 0;
uint32_t envLastModifiedTime = 0;
uint32_t envLastPeers = 0;

// Environment variables
uint32_t var_gateway_env_update_mins;
uint32_t var_gateway_pairing_timeout_mins;
uint32_t var_gateway_sensordb_update_mins;
uint32_t var_gateway_sensordb_reset_counts;
uint32_t last_var_gateway_sensordb_reset_counts = 0;
uint32_t time_var_gateway_sensordb_reset_counts = 0;

// Forwards
void gatewayUpdateEnvVar(const char *name, const char *value);

// Process the received message
bool gatewayProcessSensorRequest(uint8_t *sensorAddress, uint8_t *reqJSON, uint32_t reqJSONLen, uint8_t **rspJSON, uint32_t *rspJSONLen)
{

    // Marshal the JSON
    char *reqstr = JAllocString(reqJSON, reqJSONLen);
    J *req = JConvertFromJSONString(reqstr);
    JFree(reqstr);
    J *rsp = NULL;
    if (req == NULL) {
        rsp = JCreateObject();
        JAddStringToObject(rsp, "err", "unable to interpret JSON request");
    }

    // Disallow certain requests
    if (rsp == NULL) {
        char compositeName[SENSOR_NAME_MAX] = {0};
        char sensorName[SENSOR_NAME_MAX] = {0};
        char sensorLocationOLC[16] = {0};
        if (flashConfigFindPeerByAddress(sensorAddress, NULL, NULL, compositeName)) {
            extractNameComponents(compositeName, sensorName, sensorLocationOLC, sizeof(sensorLocationOLC));
        }
        rsp = authRequest(sensorAddress, sensorName, sensorLocationOLC, req);
    }

    // Perform the request
    if (rsp != NULL) {
        if (req != NULL) {
            JDelete(req);
        }
    } else {
        traceLn("processing sensor request:");
        rsp = NoteRequestResponse(req);
        if (rsp == NULL) {
            return false;
        }
    }

    // Send the response back to the sensor
    *rspJSON = (uint8_t *) JConvertToJSONString(rsp);
    JDelete(rsp);
    if (rspJSON == NULL) {
        traceLn("processing sensor request: can't allocate response");
        return false;
    }
    *rspJSONLen = strlen((char *)*rspJSON);
    return true;

}

// Do periodic housekeeping related to the gateway
void gatewayHousekeeping(bool sensorsChanged, uint32_t cachedSensors)
{
    uint32_t now = NoteTimeST();

    // If we've added peers since last time, we need to refresh the environment so that
    // we force the peer table to get the new name for the peer.
    if (envLastPeers != flashConfigPeers()) {
        envLastUpdateTime = 0;
        envLastModifiedTime = 0;
        envLastPeers = flashConfigPeers();
    }

    // See if we need to refresh the environment
    while (envLastUpdateTime == 0 || now >= envLastUpdateTime+(var_gateway_env_update_mins*60)) {
        envLastUpdateTime = now;

        // See if a firmware update is waiting for us.  If we return from this method with true, it
        // means that the update wasn't successfully completed and so we need to restore our modes.
        // A return with false means that no firmware update was available.
        if (noteFirmwareUpdateIfAvailable()) {
            noteSetup();
        }

        // See if env vars need to be checked
        bool refreshEnvVars = false;
        J *rsp = NoteRequestResponse(NoteNewRequest("env.modified"));
        if (rsp == NULL) {
            refreshEnvVars = true;
        } else {
            if (NoteResponseError(rsp)) {
                refreshEnvVars = true;
            } else {
                uint32_t modifiedTime = (uint32_t) JGetNumber(rsp, "time");
                if (envLastModifiedTime != modifiedTime) {
                    refreshEnvVars = true;
                    envLastModifiedTime = modifiedTime;
                }
            }
            NoteDeleteResponse(rsp);
        }
        if (!refreshEnvVars) {
            break;
        }

        // Load the entire set of env vars, to minimize latency.  We need
        // to keep latency to a minimum because for every second we spend in
        // here, it's a second we don't have a receive outstanding.
        rsp = NoteRequestResponse(NoteNewRequest("env.get"));
        if (rsp == NULL) {
            break;
        }

        // Get the body
        J *body = JDetachItemFromObject(rsp, "body");
        NoteDeleteResponse(rsp);

        // Enumerate fields in the environment body
        J *field = NULL;
        JObjectForEach(field, body) {
            char *value = JStringValue(field);
            const char *name = JGetItemName(field);
            gatewayUpdateEnvVar(name, value);

        }

        // Done with body, and done refreshing env vars as a batch
        JDelete(body);

    }

    // Update the last reset time
    if (last_var_gateway_sensordb_reset_counts != 0
        && var_gateway_sensordb_reset_counts != 0
        && last_var_gateway_sensordb_reset_counts != var_gateway_sensordb_reset_counts) {
        time_var_gateway_sensordb_reset_counts = NoteTimeST();
    }
    last_var_gateway_sensordb_reset_counts = var_gateway_sensordb_reset_counts;

    // See if we need to refresh the db
    if (sensorsChanged) {
        dbLastUpdateTime = 0;
    }
    if (dbLastUpdateTime == 0 || now >= dbLastUpdateTime+(var_gateway_sensordb_update_mins*60)) {
        dbLastUpdateTime = now;

        // Load the entire set of configuration notes, to minimize latency.  We need
        // to keep latency to a minimum because for every second we spend in here
        // it's a second we don't have a receive outstanding.
        J *req = NoteNewRequest("note.changes");
        JAddStringToObject(req, "file", CONFIGDB);
        J *rsp = NoteRequestResponse(req);
        if (rsp != NULL) {

            // Get the results
            J *notes = JDetachItemFromObject(rsp, "notes");

            // We no longer need the response
            NoteDeleteResponse(rsp);

            // Enumerate notes within the results
            J *note = NULL;
            bool updateConfig = false;
            JObjectForEach(note, notes) {

                // Get the sensor ID (in hex)
                const char *sensorIDHex = JGetItemName(note);

                // Get the sensor location (encoded in OLC format)
                const char *bodyName = "";
                const char *bodyLoc = "";
                J *body = JGetObject(note, "body");
                if (body != NULL) {
                    bodyName = JGetString(body, "name");
                    bodyLoc = JGetString(body, "loc");
                }

                // Get the sensor name, and create a composite with the location
                char sensorName[256];
                strlcpy(sensorName, bodyName, sizeof(sensorName));
                if (bodyLoc[0] != '\0') {
                    strlcat(sensorName, " [", sizeof(sensorName));
                    strlcat(sensorName, bodyLoc, sizeof(sensorName));
                    strlcat(sensorName, "]", sizeof(sensorName));
                }

                // Convert the sensor ID from hex to binary
                bool validHex = true;
                uint8_t addrbuf[ADDRESS_LEN];
                int addrlen = 0;
                const char *p = sensorIDHex;
                while (*p != '\0' && *(p+1) != '\0') {
                    char ch1 = *p++;
                    char ch2 = *p++;
                    uint8_t value = 0;
                    if (ch1 >= '0' && ch1 <= '9') {
                        value |= (ch1 - '0') << 4;
                    } else if (ch1 >= 'a' && ch1 <= 'f') {
                        value |= ((ch1 - 'a') + 10) << 4;
                    } else if (ch1 >= 'A' && ch1 <= 'F') {
                        value |= ((ch1 - 'A') + 10) << 4;
                    } else {
                        validHex = false;
                        break;
                    }
                    if (ch2 >= '0' && ch2 <= '9') {
                        value |= (ch2 - '0');
                    } else if (ch2 >= 'a' && ch2 <= 'f') {
                        value |= ((ch2 - 'a') + 10);
                    } else if (ch2 >= 'A' && ch2 <= 'F') {
                        value |= ((ch2 - 'A') + 10);
                    } else {
                        validHex = false;
                        break;
                    }
                    if (addrlen >= ADDRESS_LEN) {
                        validHex = false;
                        break;
                    }
                    addrlen++;
                    addrbuf[ADDRESS_LEN-addrlen] = value;
                }

                // If valid hex and the length is at least 2 bytes, set the name
                if (validHex && addrlen >= 2) {
                    if (flashConfigUpdatePeerName(&addrbuf[ADDRESS_LEN-addrlen], addrlen, sensorName)) {
                        trace("config: ");
                        trace(sensorIDHex);
                        trace(" name updated to '");
                        trace(sensorName);
                        trace("'");
                        traceNL();
                        updateConfig = true;
                    } else {
                        trace("config: ");
                        trace(sensorIDHex);
                        trace(" remains ");
                        trace(sensorName);
                        traceNL();
                    }
                }
            }

            // Done with all configured notes
            JDelete(notes);

            // Update the config if something changed
            if (updateConfig) {
                flashConfigUpdate();
            }
        }

        // Now, loop over all sensors, updating them
        uint32_t notesUpdated = 0;
        for (int i=0; i<cachedSensors; i++) {

            // Get the info
            uint8_t sensorAddress[ADDRESS_LEN];
            uint16_t sensorMv;
            int8_t gatewayRSSI, gatewaySNR, sensorRSSI, sensorSNR, sensorTXP, sensorLTP;
            uint32_t lastReceivedTime, requestsProcessed, requestsLost;
            bool valid = appSensorCacheEntry(i, sensorAddress,
                                             &gatewayRSSI, &gatewaySNR,
                                             &sensorRSSI, &sensorSNR,
                                             &sensorTXP, &sensorLTP, &sensorMv,
                                             &lastReceivedTime,
                                             &requestsProcessed, &requestsLost);
            if (!valid) {
                continue;
            }

            // Load the record from the DB, creating the body if it doesn't exist
            J *req = NoteNewRequest("note.get");
            if (req == NULL) {
                continue;
            }
            char noteID[40];
            utilAddressToText(sensorAddress, noteID, sizeof(noteID));
            JAddStringToObject(req, "note", noteID);
            JAddStringToObject(req, "file", SENSORDB);
            J *rsp = NoteRequestResponse(req);
            if (rsp == NULL) {
                continue;
            }
            J *body;
            bool updateRequired = false;
            if (NoteResponseError(rsp)) {
                if (!NoteResponseErrorContains(rsp, "{note-noexist}")) {
                    NoteDeleteResponse(rsp);
                    continue;
                }
                body = JCreateObject();
                updateRequired = true;
            } else {
                body = JDetachItemFromObject(rsp, "body");
                if (body == NULL) {
                    body = JCreateObject();
                    updateRequired = true;
                }
            }
            NoteDeleteResponse(rsp);

            // Update the name in the note if it has changed
            char sensorName[SENSOR_NAME_MAX];
            if (flashConfigFindPeerByAddress(sensorAddress, NULL, NULL, sensorName)) {
                char cleanName[SENSOR_NAME_MAX];
                extractNameComponents(sensorName, cleanName, NULL, 0);
                if (strcmp(JGetString(body, SENSORDB_FIELD_NAME), cleanName) != 0) {
                    JDeleteItemFromObject(body, SENSORDB_FIELD_NAME);
                    JAddStringToObject(body, SENSORDB_FIELD_NAME, cleanName);
                    updateRequired = true;
                }
            }

            // Update error/success counts, or reset them
            if (var_gateway_sensordb_reset_counts != 0
                && time_var_gateway_sensordb_reset_counts != 0
                && JGetInt(body, SENSORDB_FIELD_WHEN) < time_var_gateway_sensordb_reset_counts) {
                JDeleteItemFromObject(body, SENSORDB_FIELD_RECEIVED);
                JAddNumberToObject(body, SENSORDB_FIELD_RECEIVED, 0);
                JDeleteItemFromObject(body, SENSORDB_FIELD_LOST);
                JAddNumberToObject(body, SENSORDB_FIELD_LOST, 0);
                updateRequired = true;
            } else if (requestsProcessed > 0 || requestsLost > 0) {
                requestsProcessed += JGetInt(body, SENSORDB_FIELD_RECEIVED);
                JDeleteItemFromObject(body, SENSORDB_FIELD_RECEIVED);
                JAddNumberToObject(body, SENSORDB_FIELD_RECEIVED, requestsProcessed);
                requestsLost += JGetInt(body, SENSORDB_FIELD_LOST);
                JDeleteItemFromObject(body, SENSORDB_FIELD_LOST);
                JAddNumberToObject(body, SENSORDB_FIELD_LOST, requestsLost);
                updateRequired = true;
            }

            // Update signal strength and quality
            if (lastReceivedTime != JGetInt(body, SENSORDB_FIELD_WHEN)) {
                JDeleteItemFromObject(body, SENSORDB_FIELD_WHEN);
                JAddNumberToObject(body, SENSORDB_FIELD_WHEN, lastReceivedTime);
                if (gatewayRSSI != 0 || gatewaySNR != 0) {
                    JDeleteItemFromObject(body, SENSORDB_FIELD_GATEWAY_RSSI);
                    JAddNumberToObject(body, SENSORDB_FIELD_GATEWAY_RSSI, gatewayRSSI);
                    JDeleteItemFromObject(body, SENSORDB_FIELD_GATEWAY_SNR);
                    JAddNumberToObject(body, SENSORDB_FIELD_GATEWAY_SNR, gatewaySNR);
                }
                if (sensorRSSI != 0 || sensorSNR != 0) {
                    JDeleteItemFromObject(body, SENSORDB_FIELD_SENSOR_RSSI);
                    JAddNumberToObject(body, SENSORDB_FIELD_SENSOR_RSSI, sensorRSSI);
                    JDeleteItemFromObject(body, SENSORDB_FIELD_SENSOR_SNR);
                    JAddNumberToObject(body, SENSORDB_FIELD_SENSOR_SNR, sensorSNR);
                }
                JDeleteItemFromObject(body, SENSORDB_FIELD_SENSOR_TXP);
                JAddNumberToObject(body, SENSORDB_FIELD_SENSOR_TXP, sensorTXP);
                JDeleteItemFromObject(body, SENSORDB_FIELD_SENSOR_LTP);
                JAddNumberToObject(body, SENSORDB_FIELD_SENSOR_LTP, sensorLTP);
                JNUMBER voltage = ((JNUMBER) sensorMv) / 1000;
                if (voltage != 0) {
                    JDeleteItemFromObject(body, SENSORDB_FIELD_VOLTAGE);
                    JAddNumberToObject(body, SENSORDB_FIELD_VOLTAGE, voltage);
                }
                updateRequired = true;
            }

            // If no update required, continue
            if (!updateRequired) {
                JDelete(body);
                continue;
            }

            // Update the note
            req = NoteNewRequest("note.update");
            if (req == NULL) {
                JDelete(body);
                traceLn("sensordb update error");
                continue;
            }
            JAddStringToObject(req, "note", noteID);
            JAddStringToObject(req, "file", SENSORDB);
            JAddItemToObject(req, "body", body);
            bool success = NoteRequest(req);
            if (!success) {
                continue;
            }
            notesUpdated++;

            // Now that we've updated the note, clear the stats in the cache
            appSensorCacheEntryResetStats(i);
            trace("sensordb: updated ");
            trace(noteID);
            traceNL();

        }

        // If we've updated any notes, sync them immediately
        if (notesUpdated > 0) {
            NoteRequest(NoteNewRequest("hub.sync"));
        }

    }

}

// Set defaults for env vars
void gatewaySetEnvVarDefaults()
{
    NoteSetEnvDefaultInt(VAR_GATEWAY_ENV_UPDATE_MINS, DEFAULT_GATEWAY_ENV_UPDATE_MINS);
    NoteSetEnvDefaultInt(VAR_PAIRING_BEACON_GATEWAY_TIMEOUT_MINS, DEFAULT_PAIRING_BEACON_GATEWAY_TIMEOUT_MINS);
    NoteSetEnvDefaultInt(VAR_GATEWAY_SENSORDB_UPDATE_MINS, DEFAULT_GATEWAY_SENSORDB_UPDATE_MINS);
    NoteSetEnvDefaultInt(VAR_GATEWAY_SENSORDB_RESET_COUNTS, DEFAULT_GATEWAY_SENSORDB_RESET_COUNTS);
}

// Update local vars
void gatewayUpdateEnvVar(const char *name, const char *value)
{
    if (strcmp(VAR_GATEWAY_ENV_UPDATE_MINS, name) == 0) {
        var_gateway_env_update_mins = JAtoI(value);
    }
    if (strcmp(VAR_PAIRING_BEACON_GATEWAY_TIMEOUT_MINS, name) == 0) {
        var_gateway_pairing_timeout_mins = JAtoI(value);
    }
    if (strcmp(VAR_GATEWAY_SENSORDB_UPDATE_MINS, name) == 0) {
        var_gateway_sensordb_update_mins = JAtoI(value);
    }
    if (strcmp(VAR_GATEWAY_SENSORDB_RESET_COUNTS, name) == 0) {
        var_gateway_sensordb_reset_counts = JAtoI(value);
    }
}

// Execute console command
void gatewayCmd(char *cmd)
{
    if (strcmp(cmd, "refresh") == 0 || strcmp(cmd, "r") == 0) {
        trace("REFRESH DB");
        traceNL();
        dbLastUpdateTime = 0;
    } else if (strcmp(cmd, "counts") == 0 || strcmp(cmd, "count") == 0 || strcmp(cmd, "c") == 0) {
        trace("RESET COUNTS");
        traceNL();
        time_var_gateway_sensordb_reset_counts = NoteTimeST();
        dbLastUpdateTime = 0;
    } else {
        trace("??");
        traceNL();
    }
}
