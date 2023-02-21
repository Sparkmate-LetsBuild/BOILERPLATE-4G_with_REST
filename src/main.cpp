// include Arduino.h first to avoid squiggles
#include <Arduino.h>

// configs
#include <configs/HARDWARE_config.h>
#include <configs/OPERATIONS_config.h>
#include <configs/BRICKS_config.h>

// bricks
#include <inits/firmware_details_init.h>
#include <http_handler.h>

// libs
#include <StatusLogger.h>

// defines
#define DEFAULT_LAT 48.82
#define DEFAULT_LON 2.38

const int DELAY_DATA_TIME = 30 * 1000;    // Perform the data stream every 30 seconds
const int DELAY_STATUS_TIME = 120 * 1000; // Perform a status report every 2 minutes

unsigned long last_data_time = 0;
unsigned long last_status_time = 0;

LoopbackStream working_stream(4000); // A working loopback stream, use this like super-flexible strings ;)

void setup()
{
    // Set up all serial connections and misc. pins and run a systems checks
    Serial.begin(SERIAL_MON_BAUD);
    Firmware::init();

    // Connect to the simcom module
    SIMCOMHandler::SIMMODULE_STATUS_ENUM sim_status = SIMCOMHandler::setupSIMModule();
    if (sim_status == SIMCOMHandler::FAILED_TO_AT or sim_status == SIMCOMHandler::NO_SIM_CARD)
    {
        if (sim_status == SIMCOMHandler::FAILED_TO_AT)
        {
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_OFFLINE, "We failed to AT command with the SIMCOM module.");
        }
        else
        {
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_OFFLINE, "We AT commanded, but it looks like there's no SIM card?");
        }
        StatusLogger::log(StatusLogger::LEVEL_VERBOSE, StatusLogger::NAME_SIMCOM, "DATA UPLOAD DISABLED FOR THIS SESSION!");
        SIMCOMHandler::powerDownSIMModule();
        while (1)
        // enter an infinite loop, there's nothing more we can do.
        {
            delay(1);
        }
    }
    StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_PARTIAL, "We AT commanded and there is a SIM card, that's a good start.");

    // Connect to the Internet
    while (SIMCOMHandler::connectToInternet() != SIMCOMHandler::INTERNET_READY)
    {
        StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_OFFLINE, "We couldn't connect to the internet :(");
        delay(2000);
    }

    // Update the SSL time from the cell tower, this is necessary for SSL endpoints.
    if (!SIMCOMHandler::updateSSLTime())
    {
        StatusLogger::log(StatusLogger::LEVEL_WARNING, StatusLogger::NAME_SIMCOM, "The time from the Cell Tower doesn't look right, so we can't securely update the SSL time. This might explain any weird SSL errors you see in the Serial Monitor.");
    }
}

void loop()
{
    // Task 1 - Upload the current meteo data to our beeceptor data endpoint
    if ((millis() - last_data_time) > DELAY_DATA_TIME)
    {
        // Get Meteo data
        HTTP::large_doc = HTTP::getMeteorologicalData(DEFAULT_LAT, DEFAULT_LON);

        // Filter Meteo data
        if (HTTP::large_doc.containsKey("current_weather"))
        {
            HTTP::small_doc = HTTP::large_doc["current_weather"];

            // Post Meteo data
            if (HTTP::postMeteorologicalData(HTTP::small_doc))
            {
                StatusLogger::setBrickStatus(StatusLogger::NAME_BEECEPTOR, StatusLogger::FUNCTIONALITY_FULL, "Meteo data is up to date on beeceptor.");
            }
            else
            {
                StatusLogger::setBrickStatus(StatusLogger::NAME_BEECEPTOR, StatusLogger::FUNCTIONALITY_PARTIAL, "unable to post the Meteo data to beeceptor.");
            }
        }
        else
        {
            StatusLogger::setBrickStatus(StatusLogger::NAME_METEO, StatusLogger::FUNCTIONALITY_PARTIAL, "We didn't get the current weather conditions from the API.");
        }
        last_data_time = millis();
    }

    // Task 2 - Upload our brick health to our beeceptor device endpoint
    if ((millis() - last_status_time) > DELAY_STATUS_TIME)
    {
        StatusLogger::printBrickStatuses(&working_stream);
        if (HTTP::postStatuses(working_stream.readString()))
        {
            StatusLogger::setBrickStatus(StatusLogger::NAME_METEO, StatusLogger::FUNCTIONALITY_FULL, "Statuses up to date on beeceptor.");
        }
        else
        {
            StatusLogger::setBrickStatus(StatusLogger::NAME_METEO, StatusLogger::FUNCTIONALITY_PARTIAL, "Unable to post the statuses to beeceptor.");
        }
        last_status_time = millis();
    }

    // Delay til next tick
    delay(1000);       // delay 1 second
    Serial.print("."); // have a visible tick just so we can ensure our board is working
}