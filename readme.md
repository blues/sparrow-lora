
COMMON INSTRUCTIONS FOR BOTH GATEWAY AND SENSOR

- Both the sensor and gateway have one button and three LEDS which are BLUE GREEN RED in that order.  They should be labelled "PAIR", "RX", and "TX" on the enclosure to assist the color blind. 

- BUTTON HOLD for 30 sec during wild LED flashing, will reset to factory settings.  When all three LEDs flash ON/OFF three times in succession, it means that the factory reset is completed

- Green means that the unit is in "receive mode" waiting for a sensor to send it some data.

- Red means that the unit is in "transmit mode" sending some data.

- If both Red and Green are on, that means that it is attempting to send something but it is waiting for its turn to send, "listening" before "talking".

- Sensor and Gateways must be "paired".  This is not only so that sensors can operate in an environment in which multiple gateways may be "in range" (such as an apartment building), but also because all communication between Sensors and Gateways is encrypted with a 256-bit AES key that is unique to each sensor/gateway pair, and the act of pairing allows the sensor to send its key to the gateway.

- During software development, if the appropriate options are enabled in config_sys.h, the developer may connect a serial terminal to the RX/TX pins at 9600 bps. This trace console can be enabled by holding down the buttton when the product is being reset or powered-on, else the trace output will be suppressed,


GATEWAY INSTRUCTIONS

- If you factory-reset the gateway, it will lose all pairings with sensors and the sensors will need to be re-paired.

- BUTTON PRESS toggles "allow incoming pairing" mode, with BLUE LED signalling the mode.  If left on it will time out in 60m (PAIRING_BEACON_GATEWAY_TIMEOUT_MINS or "pairing_timeout_mins" env var).  If the blue LED is off, it will refuse incoming pairing requests from sensors.

- BUTTON PRESS-and-HOLD for several seconds brings up the wifi "soft access point" if you are using a WiFi Notecard.  If you're not using a WiFi notecard, this function does nothing.  If it brings up the SoftAP, the LEDs will "crawl" in a rotating pattern until the Notecard is back in normal mode.

- At startup, the "crawl" of the LEDs from left to right to left to right indicates that the gateway is trying to connect over cellular to the network.  It will not proceed in its gateway role until it can successfully connect.  If it stays in this mode for an indefinite period of time, it most likely means that you don't have acceptable cellular coverage in that location.

- The env vars update every 15 minutes (DEFAULT_GATEWAY_ENV_UPDATE_MINS), which is controllable by the env var "env_update_mins".  This is most relevant because sensors can be named by creating env vars with their hexadecimal sensor ID, and this controls the gateway's responsiveness to those changes.

- The gateway maintains a sensor database "sensors.db" (SENSORDB).  This contains sensor RSSI and message statistics.  It is updated every 24 hours (DEFAULT_GATEWAY_SENSORDB_UPDATE_MINS) which can be further controlled by using the env var "sensordb_update_mins".

- Sometimes you will see all three LEDs flash in unison.  This is usually acknowledging some operation that you perform with the button.


SESNSOR INSTRUCTIONS

- If the unit powers-on and the BLUE LED is lit, it means that the sensor has not yet paired with a gateway.  It will send out an automatic pairing request to the gateway every 60 seconds (PAIRING_BEACON_SECS) for up to 5 minutes (PAIRING_BEACON_SENSOR_TIMEOUT_MINS).  You can also send out a pairing request manually by tapping the button.  If multiple gateways are concurrently in pairing mode, the sensor will end up being paired with one of them randomly.

- If the unit has been paired with a gateway, you can re-pair it with a different gateway by performing a factory reset.

- Tapping the button will send out a test message to the gateway, in the form of a messaging that goes to "_health.qo" that indicates the sensor's ID.


GATEWAY ENVIRONMENT VARIABLES

"env_update_mins"
(config_notecard.h VAR_GATEWAY_ENV_UPDATE_MINS, default is currently 5)

Because the sparrow firmware is using a single-threaded operating environment, whenever it is doing some "overhead" task such as reading and processing and updating environment variables on the Notecard, it is "offline" with respect to LoRa and thus will miss any LoRa message that is sent to it during that period.  As such, we try to do housekeeping tasks as seldomly as we can, and we try to do it when we seem to be idle.

This variable specifies the number of minutes between asking the Notecard "has any update to the environment variables been received from the Notehub?"  And if an update has occurred, the updated env vars (below) are pulled-in from the Notecard and are processed.

"pairing_timeout_mins"
(config_notecard.h VAR_PAIRING_BEACON_GATEWAY_TIMEOUT_MINS, default is currently 60)

When the user walks over to the gateway and taps the blue button, it toggles Pairing mode on/off.  It stays "on" for a while simply because you are likely to be walking around your home pairing one device after another.

However, if you left the gateway indefinitely in pairing mode, it might inadvertently allow someone else (your neighbor) to pair their devices with your gateway.  This is likely not what you intend - and so pairing mode turns "off" after this timeout just in case you forget to do so. 

"sensordb_update_mins"
(config_notecard.h VAR_GATEWAY_SENSORDB_UPDATE_MINS, default is currently 60)

The sensor database (sensors.db) contains statistics about the operations of each sensor, with each NoteID being the ID of the sensor whose stats it contains.

The upside of keeping this database up-to-date is that the Notehub (and by extension the customer's cloud application) can see the message counts, RSSI, and so on, of each sensor.  The downside, as I noted above, is that whenever the gateway is spending time moving statistics from memory into the Notecard via note.update, it isn't listening to the network for LoRa activity.

"sensordb_reset_counts"
(config_notecard.h VAR_GATEWAY_SENSORDB_RESET_COUNTS)

Normally, the "message count" statistics in the sensor database (sensors.db) just keep counting upward.  Messages received, messages lost, and so on.  Sometimes you will move sensors around, or even move the gateway around, and you'd just like to reset the statistics.

The way that you do this is to set this env var to the current unix epoch time.  The Notecard will notice that this value has changed since the last time it checked, and will reset all the counters to 0.


THE CONFIG.DB NOTEFILE

The gateway uses one notefile in a read-only manner, to receive configuration information that it may use for itself and will pass-on to the sensors themselves.  Although not strictly required, it is recommended that the cloud app use the HTTPS API to add or update notes within this database to distinguish one sensor from another, as the sensor IDs are fairly obscure.

The NoteID of each note is the sensor ID.  The two (optional) fields currently in the body of each note are:
"name" is the human-readable name of the sensor
"loc" is the Open Location Code string indicating the geolocation of the sensor

If you examine config_radio.h, you'll see that each time the gateway sends an ACK to a sensor, it carries along with it, among other things,
- The current Unix Epoch time
- The current Time Zone offset (minutes from UTC)
- The current Time Zone abbreviation (3 characters)
- The current sensor's name

The reason these are sent to the sensor is so that if the sensor has a UI, it can display its name and the time.  Furthermore, decisions can be made by the sensor such as taking a sensor reading only at 2AM local time.

(The location is not passed to the device only because I haven't found a useful reason to do so, but that's an easy extension to this structure.)
