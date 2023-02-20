#pragma once

// configs
#include <configs/HARDWARE_config.h>
#include <configs/OPERATIONS_config.h>

// libs
#include <StatusLogger.h>

namespace Firmware
{
    bool initialized = false;
    String firmware_version = "unknown";
    String hardware_version = "uknown";
    String write_date = "unknown";

    /**
     * @brief Print the firmware details to console (no status logging)
     */
    void printFirmwareDetails()
    {
        Serial.println();
        Serial.println("-- FIRMWARE DETAILS --");
        Serial.println("Device ID: " + String(THINGNAME));
        Serial.println("Firmware version: " + firmware_version);
        Serial.println("Board version: " + hardware_version);
        Serial.println("Firmware write date: " + write_date);
        Serial.println("Environment: " + String(ENVIRONMENT));
        Serial.println("-- END OF FIRMWARE DETAILS --");
        Serial.println();
    }

    /**
     * @brief Initialize the Firmware preferences brick
     */
    void init()
    {
#ifdef FIRMWARE_VERSION
        firmware_version = FIRMWARE_VERSION;
#endif
#ifdef HARDWARE_VERSION
        hardware_version = "v.1.0.0";
#endif
#ifdef WRITE_DATE
        write_date = WRITE_DATE;
#endif
        printFirmwareDetails();
        initialized = true;
        StatusLogger::setBrickStatus(StatusLogger::NAME_ESP32, StatusLogger::FUNCTIONALITY_FULL, String(THINGNAME) + " has started.");
    }
}