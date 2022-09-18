#ifndef CONSTANTS_H
#define CONSTANTS_H

#define LOG_FILENAME          "LOG1"                // the log filename to write to
#define METADATA_FILENAME     "METADATA"            // the metadata file name
#define FILE_EXT              ".CSV"                // the log file extension


#define DEV_NAME_PREFIX       "NSP"                 // the device name prefix
#define BAUDRATE              921600                // the device baudrate

#define DEF_CAPTURE_INTERVAL  5000                  // how frequently to capture data in ms
#define MIN_WAVELENGTH        340                   // the minimum wavelength we are interested in
#define MAX_WAVELENGTH        1010                  // the maximum wavelength we are interested in
#define SENSOR_MIN_WAVELENGTH 340                   // the minimum sensing wavelength (for W1 sensor)
#define SENSOR_MAX_WAVELENGTH 1010                  // the maximum sensing wavelength  (for W1 sensor)
#define DEF_FRAME_AVG         6                     // how many frames to average into single reading
#define WAVELENGTH_STEPSIZE   5                     // the sensor wavelength resolution (5 nm for W1 sensor)
#define CAPTURE_PRECISION     18                    // how many digits of precision to write (18 digits)
#define CALIBRATION_FACTOR    1                     // the calibration factor for the particular sensor
#define MIN_ACCEPTABLE_Y      5                     // the minimum acceptable Y value for reading (CIE1931)

// PINS
#define SD_CS_PIN             4                     // pin connected to SD CS
#define NSP_CS_PIN            5                     // pin connected to NSP CS
#define NSP_RESET             28                    // NSP reset pin
#define NSP_READY             29                    // NSP ready pin

#endif
