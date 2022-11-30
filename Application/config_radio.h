// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#pragma once

// MODEM type: one shall be 1 the other shall be
#define USE_MODEM_LORA  1
#define USE_MODEM_FSK   0

// RADIO FREQ
// The following value shall be used to override ANY hardware
// related settings with respect to radio frequency.
// When `RF_FREQ` is either set to zero or left undefined, the
// configuration will be performed based upon the resisted voltages
// detected on RFSEL_0_Pin (`PB3`) and RFSEL_1_Pin (`PB5`). The Sparrow
// developer boards have dedicated DIP switches, which can be used to
// select a frequency. The frequencies associated with the switches
// can be found in `Framework/appinit.c:212`, and can even be altered
// to any desired/compatible values.
// The developer boards also have exposed pads which allow the DIP
// switces to be disabled or removed and a fixed voltage to be set.
// To learn more, check:
// http://localhost:3001/hardware/sparrow-datasheet#setting-a-frequency-plan
#define RF_FREQ 0

// RSSI
// The Received Signal Strength Indication is the received signal power in milliwatts
// and is measured in dBm. This value can be used as a measurement of how well
// a receiver can "hear" a signal from a sender. LoRa typically operates with RSSI
// values between -30 dBm and -120 dBm.
// - The closer to the LoRa RSSI maximum (-30 dBm) the stronger the signal is.
// - The closer to the LoRa RSSI minimum (-120 dBm) the weaker the signal is.

// SNR
// Signal-to-Noise Ratio is the ratio between the received power signal
// and the noise floor power level.  The noise floor is an area of all unwanted
// interfering signal sources which can corrupt the transmitted signal and
// therefore re-transmissions will occur.
// - If SNR is greater than 0, the received signal operates above the noise floor.
// - If SNR is smaller than 0, the received signal operates below the noise floor.
// Normally the noise floor is the physical limit of sensitivity, however
// LoRa works below the noise level.
// Typical LoRa SNR values are between: -20dB and +10dB
// A value closer to +10dB means the received signal is less corrupted.
// LoRa can demodulate signals which are -7.5 dB to -20 dB below the noise floor.

// Adaptive Transmit Power (ATP) Parameters
#define ATP_ENABLED     true
#define RBO_INITIAL     (RBO_MIN+(((RBO_MAX)-(RBO_MIN))/3))

// (DETERMINATIVE OF MESSAGE_MAX_BODY)
// This defines how long we wait for a radio.Send() to succeed.
#define TX_TIMEOUT_VALUE                            4000

// (DETERMINATIVE OF MESSAGE_MAX_BODY)
// Note that unlike LoRaWAN, our MAC protocol does not vary the datarate but rather uses a single
// datarate and uses a TDMA methodology wherein the gateway assigns slots to the sensors within which
// they both transmit and receive.
// The BW, SF, and coding rate Values chosen based on this document:
// https://www.hindawi.com/journals/wcmc/2018/6931083/
// Bandwidth, SF, and Coding Rate seems to be a good choice for high penetration based on the document.
// NOTE: in L.1153 radio.c, if BW=1 and SF=12 it turns on LowDatarateOptimize (LDRO), which is desirable.
#if (( USE_MODEM_LORA == 1 ) && ( USE_MODEM_FSK == 0 ))
#define LORA_BANDWIDTH                              1         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
#define LORA_SPREADING_FACTOR                       12        // [SF7..SF12] */
#define LORA_CODINGRATE                             1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         5         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#elif (( USE_MODEM_LORA == 0 ) && ( USE_MODEM_FSK == 1 ))

#define FSK_FDEV                                    25000     // Hz
#define FSK_DATARATE                                50000     // bps
#define FSK_BANDWIDTH                               50000     // Hz
#define FSK_AFC_BANDWIDTH                           83333     // Hz
#define FSK_PREAMBLE_LENGTH                         5         // Same for Tx and Rx
#define FSK_FIX_LENGTH_PAYLOAD_ON                   false

#else

#error "Please define a modem in the compiler subghz_phy_app.h."

#endif /* USE_MODEM_LORA | USE_MODEM_FSK */

#define SOLICITED_COMMS_RX_TIMEOUT_VALUE            8000
#define SOLICITED_PROCESSING_RX_TIMEOUT_VALUE       8000
#define UNSOLICITED_RX_TIMEOUT_VALUE                300000
#define TCXO_WORKAROUND_TIME_MARGIN                 50      // 50ms margin

// Pairing beacon automatic repeat period
#define PAIRING_BEACON_SECS                         60

// Amount of time that the sensor will allow itself to stay in pairing mode before
// reverting back to normal mode.
#define PAIRING_BEACON_SENSOR_TIMEOUT_MINS          5

// Address bytes
#define ADDRESS_LEN                 12

// Device name
#define SENSOR_NAME_MAX             50

// Message structure definitions
#define MESSAGE_VERSION             1
#define MESSAGE_ALG_CLEAR           0           // Cleartext
#define MESSAGE_ALG_CTR             1           // AES CTR mode, 4 byte padding
#define AES_KEY_LENGTH              256         // bits
#define AES_KEY_BYTES               (AES_KEY_LENGTH/8)
#define AES_PAD_BYTES               4

// Note that calculating this number is a process of trial and error.  You must
// use TRANSMIT_SIZE_TEST below until you calculate the value correctly.  The max
// LoRa message size depends upon the radio parameters (timeout, bandwidth, etc)
// the size of our message header overhead.  The #defines here will enable you
// to repeatedly press the button on the test sensor to send messages of decreasing
// length.  If you start too high, the radio.Send() will fail with the error
// "can't transmit to gateway: request aborted".
// Once you see a transmit succeed, you've discovered the right number below.
// (Note that this does not affect the ability to send long requests and response.
// Rather, this just says that you will be able to be guaranteed that a
// MESSAGE_MAX_BODY request or response will fit into a single packet rather
// than using multiple packets.
#define MESSAGE_MAX_BODY        170      // TIMEOUT:4000 BW:250 SPREAD:12 CODING:4/5 w/LDRO

// For transmit size testing - see below.  Note that high-level packet length
// guidance is found here:
// https://www.rfwireless-world.com/calculators/LoRa-Data-Rate-Calculator.html
#define TRANSMIT_SIZE_TEST false
#define TRANSMIT_SIZE_TEST_BEGIN 182
#define TRANSMIT_SIZE_TEST_DECREMENT 2
#if TRANSMIT_SIZE_TEST
#undef MESSAGE_MAX_BODY
#define MESSAGE_MAX_BODY TRANSMIT_SIZE_TEST_BEGIN
#endif

// The encrypted message (LITTLE-ENDIAN on the wire)
#define MESSAGE_FLAG_ACK        0x01    // This is an ACK message
#define MESSAGE_FLAG_BEACON     0x02    // This is a BEACON message
#define MESSAGE_FLAG_RESPONSE   0x04    // We require a response to this request
#define MESSAGE_SIGNATURE       0xADAD
typedef struct __attribute__((__packed__))
{
    uint16_t Signature;             // Valid message indicator
    uint16_t Millivolts;            // Sender's battery voltage
    int8_t RSSI;                    // Sender's perspective of receiver's TP
    int8_t SNR;                     // Sender's perspective of receiver's TP
    int8_t TXP;                     // Sender's transmit power level
    int8_t LTP;                     // Lowest transmit power level attempted
    uint8_t Flags;                  // Flags describing this message
    uint8_t Len;                    // Length of data chunk in this message
    uint32_t Offset;                // Offset of this chunk
    uint32_t TotalLen;              // Total length of chunk being transferred
    uint32_t RequestID;             // ID of the request owning this message
    uint8_t Body[MESSAGE_MAX_BODY]; // Body of the message
    uint8_t Padding[AES_PAD_BYTES];// To ensure room when Body is RNG-padded
}
wireMessage;

// The unencrypted outer wrapper of a message
typedef struct __attribute__((__packed__))
{
    uint8_t Version;                // Format version number
    uint8_t Algorithm;              // Encryption algorithm
    uint16_t MessageLen;            // Length, always including padding
    uint8_t Sender[ADDRESS_LEN];    // Sender identity
    uint8_t Receiver[ADDRESS_LEN];  // Receiver identity
    wireMessage Message;            // MUST BE ON 32-bit ALIGNED boundary
}
wireMessageCarrier;

// Body of a gateway ACK message (LITTLE-ENDIAN on the wire)
typedef struct __attribute__((__packed__))
{
    uint32_t TWModulusSecs;         // Transmit Window modulus of Time that defines slots
    uint16_t TWModulusOffsetSecs;   // Offset to skew this gateway from others
    uint16_t TWSlotBeginsSecs;      // Start of transmit window
    uint16_t TWSlotEndsSecs;        // End of transmit window
    uint16_t TWListenBeforeTalkMs;  // Granularity of LBT timer
    uint32_t LastProcessedRequestID;// RequestID of last request executed by gateway
    uint32_t BootTime;              // Unix epoch secs
    uint32_t Time;                  // Unix epoch secs
    int16_t ZoneOffsetMins;
    uint8_t ZoneName[3];
    char Name[SENSOR_NAME_MAX];     // Must be at end, and always null-terminated
}
gatewayAckBody;

// Maximum number of cached sensors supported by a gateway, which determines
// how many "transactions in flight" can be supported.
#define MAX_CACHED_SENSORS  25

// Amount of time beyond which we no longer consider a sensor to be "active",
// and thus we no longer reserve a time window slot for it.
#define TW_ACTIVE_SECS              (60*60*24)      // one day
#define TW_LBT_PERIOD_MS            1000            // Granularity of LBT period

// Whether or not to auto-reboot sensors when the gateway reboots
#define REBOOT_SENSORS_WHEN_GATEWAY_REBOOTS true
