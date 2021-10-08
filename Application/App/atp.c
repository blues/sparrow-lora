// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// TUNING LOG:
// 3/2 Wasn't responding quickly enough, so changed samples from 5 to 3
// 3/6 Working well, but 1 got stuck losing many packets and didn't increase quickly.
//     Changed INCREASE_POWER_IF_LOSS_EXCEEDS from 10 to 5.
// 3/8 Only count a packet loss as a true 'request loss' rather than just a correctable
//     failed ack, given how many factors can cause single-message losses.
// 3/9 Increase SNR even if instantaneous SNR is too low, because
//     it wasnt reacting quickly enough. Also, when dropping, drop by an extra
//     1db for each 10db of excess power.
// 3/10 Move packet loss test and RSSI increase to atpGatewayMessageLost because
//     if the txp is too low, we won't receive any further acks and thus
//     atpGatewayMessageReceived will never get called to re-adjust it upward.
// 3/12 Bump to 3 lost requests before power increase. (Big improvement in adaptation.)
// 3/13 Power increase if quality is below -5db, not -10db, because of observed loss.
// 4/12 Increase power when prev packets lost, just to add a bit of breathing room
// 4/13 Update TX power variables when message is lost, so once we've lost a
//      message there's a hope of increasing the power to get it out.
// 4/15 Increase sensitivity to loss by increasing power if 2, not 4, packets are lost
// 4/16 Only allow a certain number of failure resets, to prevent oscillation at
//      the boundary between "good" and "bad" signal levels (as observed).
// 4/19 Don't kill packet statistics as we're going up and down, and do
//      loss checks directly as we're about to decrease power as opposed to at the top

#include "main.h"
#include "app.h"
#include "radio.h"

// Everything in this source file deals with power levels in terms
// of index into the array of possible power levels, not in terms
// of dBm level which is mapped just before the call to radioSetTxPower();
#define initialLevel ((RBO_LEVELS - 1) - (RBO_MAX - RBO_INITIAL))
static int8_t currentLevel = initialLevel;
static int8_t lowestLevel = initialLevel;

// Key parameters for averaging and decision-making
#define SAMPLES                                 3           // Packets before ATP decision-making
#define NUM_PACKETS_MINIMUM_FOR_SUCCESS_CALC    25
#define ALLOWED_FAIL_RESETS                     2           // Soft failure becomes hard failure
#define RECONSIDER_DECREASE_IF_FAIL_PCT_LESS_THAN 5         // If very successful, reconsider a decrease
#define DONT_DECREASE_POWER_IF_PRIOR_LOSS_EXCEEDS 5         // Can't back down
#define INCREASE_POWER_INCREMENT                1           // Increment of db when loss exceeds
#define INCREASE_POWER_IF_QUALITY_BELOW         -20         // SNR (-20dB to +10dB)
#define INCREASE_POWER_IF_SIGNAL_BELOW          -120        // RSSI (-120dBm to -30dBm)
#define DECREASE_POWER_IF_SIGNAL_ABOVE          -100

// Total number of packets, and packet losses, per power level
static uint32_t packetsSent[RBO_LEVELS] = {0};
static uint32_t packetsLost[RBO_LEVELS] = {0};
static uint32_t failResets[RBO_LEVELS] = {0};

// Recent history, for computing average
uint8_t pastSamples = 0;
static int8_t pastRSSI[SAMPLES];
static int8_t pastSNR[SAMPLES];

// Forwards
void atpUpdate(bool useSignal, int8_t rssi, int8_t snr);
bool powerLevelIsLossy(int level);

// Received a gateway message, so adjust TXP.  Note that rssi and snr passed-in are
// the gateway's view of OUR signal strength (because the gateway is the Receiver in RSSI).
void atpGatewayMessageReceived(int8_t rssi, int8_t snr, int8_t rssiGateway, int8_t snrGateway)
{

    // Buffer the new sample
    if (pastSamples < SAMPLES) {

        // Insufficient samples to make decisions based on samples
        pastRSSI[pastSamples] = rssi;
        pastSNR[pastSamples] = snr;
        pastSamples++;
        APP_PRINTF("ATP: buffered sample %d\r\n", pastSamples);

    } else {

        // Enough samples to make decisions
        memmove(&pastRSSI[1], &pastRSSI[0], sizeof(pastRSSI)-sizeof(pastRSSI[0]));
        pastRSSI[0] = rssi;
        memmove(&pastSNR[1], &pastSNR[0], sizeof(pastSNR)-sizeof(pastSNR[0]));
        pastSNR[0] = snr;

    }

    // Update the tx power
    atpUpdate(true, rssi, snr);

}

// Update the transmit power based on current knowledge
void atpUpdate(bool useSignal, int8_t rssi, int8_t snr)
{
    bool mustIncreaseTxPower = false;
    bool mustNotDecreaseTxPower = false;
    bool shouldIncreaseTxPower = false;
    bool shouldTryDecreasingTxPower = false;
    int8_t decreasePowerLevelByDb = 1;

    // At certain intervals, if we're at the top power level, reset all parameters and
    // try ATP from scratch.  This is to correct for the fact that during the first several
    // days of a system's operation people tend to be messing with it, and sometimes there
    // is an extended outage wherein all the sensors' levels incrementally climb to the top.
    bool resetATPState = false;
    static int64_t prevMs = 0;
    int64_t elapsedSinceBootMs = TIMER_IF_GetTimeMs() - appBootMs;
    if (currentLevel == (RBO_LEVELS-1)) {
        int64_t thresholdMs;
        int64_t ms1Day = 1000LL * 60LL * 60LL * 24LL;

        thresholdMs = 3 * ms1Day;
        if (prevMs < thresholdMs && elapsedSinceBootMs > thresholdMs) {
            resetATPState = true;
        }

        thresholdMs = 7 * ms1Day;
        if (prevMs < thresholdMs && elapsedSinceBootMs > thresholdMs) {
            resetATPState = true;
        }

    }
    prevMs = elapsedSinceBootMs;

    if (resetATPState) {
        currentLevel = initialLevel;
        lowestLevel = initialLevel;
        pastSamples = 0;
        memset(packetsSent, 0, sizeof(packetsSent));
        memset(packetsLost, 0, sizeof(packetsLost));
        memset(failResets, 0, sizeof(failResets));
        APP_PRINTF("ATP: state reset to %ddb so that we may try again\r\n", currentLevel);
    }

    // If we've sent many packets at this level and have had great success, clear out the
    // packet loss of the prior level so that we give it reconsideration.  The case that this
    // is trying to cover is this:  If the gateway goes offline for an extended period of time,
    // all sensors will ratchet their way up to the maximum power because of the sustained loss.
    // This code ensures that after the gateway comes back, and after we've then sent a number
    // of successful packets, that we will once again start reconsidering a decrease in
    // power level.  (The rule of thumb should be that the longer the gateway is offline,
    // the longer it will take to get the sensors to settle back down to a good power level.)
    if (currentLevel > 0
            && packetsSent[currentLevel] > NUM_PACKETS_MINIMUM_FOR_SUCCESS_CALC
            && failResets[currentLevel-1] < ALLOWED_FAIL_RESETS) {
        if (((packetsLost[currentLevel]*100)/packetsSent[currentLevel]) < RECONSIDER_DECREASE_IF_FAIL_PCT_LESS_THAN) {
            if (packetsLost[currentLevel-1] > DONT_DECREASE_POWER_IF_PRIOR_LOSS_EXCEEDS) {
                packetsSent[currentLevel-1] = 0;
                packetsLost[currentLevel-1] = 0;
                failResets[currentLevel-1]++;
                APP_PRINTF("ATP: resetting %ddb level because of low fail pct at current level\r\n", (currentLevel-1)+RBO_MIN);
            }
        }
    }

    // If the instantaneous SNR as perceived by the gateway is too low, it is
    // nothing but danger.  If this is an anomaly it will be corrected later.
    if (useSignal && snr < INCREASE_POWER_IF_QUALITY_BELOW) {
        mustIncreaseTxPower = true;
        APP_PRINTF("ATP: must increase power because snr is %ddb\r\n", snr);
    }

    // Add it to samples
    int32_t averageRSSI = 0;
    int32_t averageSNR = 0;
    if (pastSamples >= SAMPLES) {

        // Compute the average of recents
        for (int i=0; i<SAMPLES; i++) {
            averageRSSI += pastRSSI[i];
            averageSNR += pastSNR[i];
        }
        averageRSSI /= SAMPLES;
        averageSNR /= SAMPLES;

        // Make decisions
        if (averageSNR < INCREASE_POWER_IF_QUALITY_BELOW) {
            shouldIncreaseTxPower = true;
            APP_PRINTF("ATP: would increase power because avg snr is %ddb\r\n", averageSNR);
        } else if (averageRSSI < INCREASE_POWER_IF_SIGNAL_BELOW) {
            shouldIncreaseTxPower = true;
            APP_PRINTF("ATP: would increase power because avg rssi is %ddb\r\n", averageRSSI);
        } else if (averageRSSI > DECREASE_POWER_IF_SIGNAL_ABOVE) {

            // Attempt to decrease power by a big step
            shouldTryDecreasingTxPower = true;
            decreasePowerLevelByDb += 2 * ((averageRSSI - DECREASE_POWER_IF_SIGNAL_ABOVE) / 10);
            if (decreasePowerLevelByDb > currentLevel) {
                decreasePowerLevelByDb = currentLevel;
                APP_PRINTF("ATP: would decrease power but it's already bottomed-out at %ddb\r\n", decreasePowerLevelByDb);
            } else {
                APP_PRINTF("ATP: should decrease power by %ddb because avg rssi is %ddb\r\n", decreasePowerLevelByDb, averageRSSI);
            }

            // If that step would cause loss, try a single step
            if (powerLevelIsLossy(currentLevel-decreasePowerLevelByDb)) {
                APP_PRINTF("ATP: can't decrease power that much because %d/%d lost\r\n",
                           packetsLost[currentLevel-decreasePowerLevelByDb],
                           packetsSent[currentLevel-decreasePowerLevelByDb]);
                decreasePowerLevelByDb = 1;
                if (decreasePowerLevelByDb > currentLevel) {
                    decreasePowerLevelByDb = currentLevel;
                    APP_PRINTF("ATP: would decrease power but it's already bottomed-out at %ddb\r\n", averageRSSI);
                } else {
                    APP_PRINTF("ATP: trying to decrease power by %ddb\r\n", decreasePowerLevelByDb);
                }

                // If that step would cause loss, give up
                if (powerLevelIsLossy(currentLevel-decreasePowerLevelByDb)) {
                    APP_PRINTF("ATP: can't decrease power at all because %d/%d lost\r\n",
                               packetsLost[currentLevel-decreasePowerLevelByDb],
                               packetsSent[currentLevel-decreasePowerLevelByDb]);
                    shouldTryDecreasingTxPower = false;
                }

            }

        }

    }

    // Bump the level as appropriate
#if ATP_ENABLED
    if (currentLevel < (RBO_LEVELS-1) && (mustIncreaseTxPower || shouldIncreaseTxPower)) {
        currentLevel++;
        pastSamples = 0;
        radioSetTxPower(atpPowerLevel());
        APP_PRINTF("ATP: increased power to %d dBm\r\n", currentLevel+RBO_MIN);
    } else if (currentLevel > 0 && !mustNotDecreaseTxPower && shouldTryDecreasingTxPower) {
        currentLevel -= decreasePowerLevelByDb;
        if (currentLevel < lowestLevel) {
            lowestLevel = currentLevel;
        }
        pastSamples = 0;
        radioSetTxPower(atpPowerLevel());
        APP_PRINTF("ATP: decreased power to %d dbm (%d/%d lost at this level)\r\n",
                   currentLevel+RBO_MIN, packetsLost[currentLevel], packetsSent[currentLevel]);
    } else {

        // Display signal
        if (useSignal) {
            if (averageRSSI != 0 || averageSNR != 0) {
                APP_PRINTF("ATP: rssi/snr:%d/%d avg:%d/%d txp:%d\r\n", rssi, snr, averageRSSI, averageSNR, atpPowerLevel());
            } else {
                APP_PRINTF("ATP: rssi/snr:%d/%d txp:%d\r\n", rssi, snr, atpPowerLevel());
            }
        } else {
            if (averageRSSI != 0 || averageSNR != 0) {
                APP_PRINTF("ATP: avg:%d/%d txp:%d\r\n", averageRSSI, averageSNR, atpPowerLevel());
            } else {
                APP_PRINTF("ATP: txp:%d\r\n", atpPowerLevel());
            }
        }

        // Display failure stats
        char msg[256] = {0};
        strlcat(msg, "ATP: ", sizeof(msg));
        for (int i=0; i<RBO_LEVELS; i++) {
            char this[16];
            if (i == lowestLevel) {
                strlcat(msg, "| ", sizeof(msg));
            }
            if (i == currentLevel) {
                strlcat(msg, "[", sizeof(msg));
            }
            if (packetsSent[i] == 0) {
                strlcat(msg, "-", sizeof(msg));
            } else {
                if (packetsLost[i] != 0) {
                    JItoA(packetsLost[i], this);
                    strlcat(msg, this, sizeof(msg));
                    strlcat(msg, "/", sizeof(msg));
                }
                JItoA(packetsSent[i], this);
                strlcat(msg, this, sizeof(msg));
            }
            if (i == currentLevel) {
                strlcat(msg, "]", sizeof(msg));
            }
            strlcat(msg, " ", sizeof(msg));
        }
        APP_PRINTF("%s\r\n", msg);

    }
#endif

}

// Return TRUE if the previous level is a lossy power level
bool powerLevelIsLossy(int level)
{

    // If we haven't sent any packets yet, it's fine to use it
    if (packetsSent[level] == 0) {
        return false;
    }

    // If the sheer number lost is too high, don't use it
    if (packetsLost[level] > DONT_DECREASE_POWER_IF_PRIOR_LOSS_EXCEEDS) {
        return true;
    }

    // If more than half the packets sent were lost, don't use it (but forgive a single loss)
    if (packetsLost[level] > 0) {
        if ((100*(packetsLost[level]-1))/packetsSent[level] >= 50) {
            return true;
        }
    }

    // Doesn't appear to be lossy
    return false;

}

// Record that we've failed to receive an ACK
void atpGatewayMessageLost()
{

    // Bump the stat, which impacts txp decrease decisions
    packetsLost[currentLevel]++;

    // If we truly lost a message (which means multiple packets lost), take action
    // by increasing power.
#if ATP_ENABLED
    uint32_t lost = packetsLost[currentLevel];
    if (lost == 1) {
        APP_PRINTF("ATP: first packet lost at %d dBm\r\n", currentLevel+RBO_MIN);
    } else {
        int8_t newLevel = currentLevel + INCREASE_POWER_INCREMENT;
        if (newLevel >= RBO_LEVELS) {
            newLevel = RBO_LEVELS-1;
        }
        if (currentLevel != newLevel) {
            currentLevel = newLevel;
            pastSamples = 0;
            radioSetTxPower(atpPowerLevel());
            APP_PRINTF("ATP: increased power to %d dBm because %d lost\r\n", currentLevel+RBO_MIN, lost);
        }
    }
#endif

    // Update the tx power
    atpUpdate(false, 0, 0);

}

// Record that we've sent a message for which we expect an ACK
void atpGatewayMessageSent(void)
{

    // Bump the number of packets sent, resetting the
    // stats when we ultimately wrap the packet counter.
    if (++packetsSent[currentLevel] == 0) {
        packetsSent[currentLevel] = 1;
        packetsLost[currentLevel] = 0;
    }

}

// Get the lowest power level attempted
int8_t atpLowestPowerLevel()
{
    return lowestLevel + RBO_MIN;
}

// Set the ATP power level to match the level of the last received message, which
// is most critical for devices that are very close to the gateway so that we
// don't overload their front-end.
void atpMatchPowerLevel(int level)
{

    // Match power level, or transmit at full power, depending upon preference
#if 1

    // Transmit at full power
    currentLevel = RBO_LEVELS - 1;

#else

    // Transmit at a slightly higher power level than what was received,
    // just to ensure that it's able to see our response.
    level += 5;
    if (level >= RBO_MIN && level <= RBO_MAX) {
        currentLevel = (level - RBO_MIN);
    } else {
        currentLevel = RBO_LEVELS - 1;
    }

#endif

    // Set the radio config at that level
    radioSetTxPower(atpPowerLevel());

}

// Set the power level to the maximum, for cases where we have no idea where
// we are and we just want to make sure a message gets out
void atpMaximizePowerLevel()
{
    currentLevel = RBO_LEVELS - 1;
}

// Get the current power level
int8_t atpPowerLevel()
{
#ifdef ATP_DISABLED_POWER_LEVEL
    return ATP_DISABLED_POWER_LEVEL;
#else
    return currentLevel + RBO_MIN;
#endif
}
