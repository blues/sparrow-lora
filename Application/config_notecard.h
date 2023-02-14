// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#pragma once

#ifndef NOTECARD_PRODUCT_UID
#define NOTECARD_PRODUCT_UID                ""          // YOUR PRODUCT UID GOES HERE
#endif

#ifndef NOTECARD_CONNECTION_MODE
#define NOTECARD_CONNECTION_MODE            "continuous"
#endif

#ifndef NOTECARD_CONTINUOUS_SYNC
#define NOTECARD_CONTINUOUS_SYNC            true        // Allows gateway to respond immediately to env changes
#endif

#ifndef NOTECARD_OUTBOUND_PERIOD_MINS
#define NOTECARD_OUTBOUND_PERIOD_MINS       (15)
#endif

#ifndef NOTECARD_INBOUND_PERIOD_MINS
#define NOTECARD_INBOUND_PERIOD_MINS        (60*24)
#endif

// Configuration database
#define CONFIGDB                            "config.db"

// Sensor database
#define SENSORDB                            "sensors.db"
#define SENSORDB_FIELD_NAME                 "name"
#define SENSORDB_FIELD_VOLTAGE              "voltage"
#define SENSORDB_FIELD_RECEIVED             "received"
#define SENSORDB_FIELD_LOST                 "lost"
#define SENSORDB_FIELD_WHEN                 "when"
#define SENSORDB_FIELD_GATEWAY_RSSI         "gateway_rssi"
#define SENSORDB_FIELD_GATEWAY_SNR          "gateway_snr"
#define SENSORDB_FIELD_SENSOR_RSSI          "sensor_rssi"
#define SENSORDB_FIELD_SENSOR_SNR           "sensor_snr"
#define SENSORDB_FIELD_SENSOR_TXP           "sensor_txp"
#define SENSORDB_FIELD_SENSOR_LTP           "sensor_ltp"

// Amount of time that we should assume that it takes to transmit a request, process it,
// and receive the response, given the type of application running on the sensors.  If,
// for example, many requests or responses are desirable within a given time window,
// then this can be made large.  The downside of making this smaller is that a given
// node will "step on top of" the next sensor's window.  However, sensors do a
// listen-before-talk, which mitigates this to a certain extent.
#define RADIO_TIME_WINDOW_SECS                          (17)

// The very nature of our protocol is that every message sent from the sensor to the
// gateway, and from the gateway to the sensor, is ACK'ed.  Unfortunately, some
// microcontrollers are not super-fast in rescheduling the MCU after a radio.Send(),
// and there can be a delay between the completion of that send and the issuance
// of the radio.Rx().  As such, we may impose a delay before the radio.Send() to
// increase the probability that the *other side* has gotten to the point where
// there is a radio.Rx() outstanding to "hear" the transmit.
#define RADIO_TURNAROUND_ALLOWANCE_MS                   750

// The number of times we'll retry a request upon some kind of failure
#define GATEWAY_REQUEST_FAILURE_RETRIES                 5

// Environment variables
extern uint32_t var_gateway_env_update_mins;
#define VAR_GATEWAY_ENV_UPDATE_MINS                     "env_update_mins"
#define DEFAULT_GATEWAY_ENV_UPDATE_MINS                 (5)
extern uint32_t var_gateway_pairing_timeout_mins;
#define VAR_PAIRING_BEACON_GATEWAY_TIMEOUT_MINS         "pairing_timeout_mins"
#define DEFAULT_PAIRING_BEACON_GATEWAY_TIMEOUT_MINS     (60)
extern uint32_t var_gateway_sensordb_update_mins;
#define VAR_GATEWAY_SENSORDB_UPDATE_MINS                "sensordb_update_mins"
#define DEFAULT_GATEWAY_SENSORDB_UPDATE_MINS            (60)
extern uint32_t var_gateway_sensordb_reset_counts;
#define VAR_GATEWAY_SENSORDB_RESET_COUNTS               "sensordb_reset_counts"
#define DEFAULT_GATEWAY_SENSORDB_RESET_COUNTS           0

