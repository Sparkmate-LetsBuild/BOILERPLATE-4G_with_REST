#pragma once

// configs
#include <configs/HARDWARE_config.h>

// inits
#include <inits/simcom_init.h>

// libs
#include <TimeLib.h>
#include <StatusLogger.h>

// Brick for the SIMCOM (Sim7070G) module. We will only ever use one SIMCOM module, so this is a namespace (treat as an object), not a class.
namespace SIMCOMHandler
{
    bool attempted_initialized = false;
    bool certs_set = false;

    // Power up the SIM Module and check we can communicate

    /**
     * @brief Power up the SIMCOM module
     *
     * @returns true if power on was successful and we can start using it, otherwise false
     */
    bool initSIMModule()
    {
        if (is_initialized)
        {
            return true;
        }
        if (modem.isNetworkConnected())
        {
            StatusLogger::log(StatusLogger::LEVEL_VERBOSE, StatusLogger::NAME_SIMCOM, "Already initialized..");
            is_initialized = true;
            return true;
        }
        StatusLogger::log(StatusLogger::LEVEL_VERBOSE, StatusLogger::NAME_SIMCOM, "6 secs to load the simcom module");

#ifdef SIM_POWER_PIN
        vTaskDelay(2000);                  //
        digitalWrite(SIM_POWER_PIN, LOW);  // Let it float again
        vTaskDelay(2000);                  // Wait 2 seconds for it to get quiet
        digitalWrite(SIM_POWER_PIN, HIGH); //
        vTaskDelay(1000);                  // Do not wait for more than 1.2s or it is a power down signal for the simcom
        digitalWrite(SIM_POWER_PIN, LOW);  // Let it float again
        vTaskDelay(1000);                  // Wait 1 second for it to wake up
#endif

        // Setup Serial Comms
        SerialAT_4g.begin(SERIAL_AT_SIMCOM_BAUD, SERIAL_8N1, SIM_RX_pin, SIM_TX_pin); // Communicate between Arduino and SimCom chip
        StatusLogger::log(StatusLogger::LEVEL_VERBOSE, StatusLogger::NAME_SIMCOM, "Attempting to connect to SIMCOM module");

        // Setup Module
        int attempted = 0;
        while (!modem.begin())
        {
            if (attempted)
            {
                if (attempted == 1)
                {
                    StatusLogger::log(StatusLogger::LEVEL_WARNING, StatusLogger::NAME_SIMCOM, "SIMCOM not communicating on default baud, trying another baudrate", true);
                    StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_OFFLINE, "Offline.");
                    attempted = 2;
                }
                else
                {
                    StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_OFFLINE, "Offline.");
                    return 0;
                }
            }
            StatusLogger::log(StatusLogger::LEVEL_WARNING, StatusLogger::NAME_SIMCOM, "Unable to communicate with SIMCOM. Trying once more.");
            modem.restart();
            attempted += 1; // Tries twice more

            // n.b. if you don't have AT hardware, you could restart and try again, but chances are hardware issue.
        };
        attempted_initialized = 1;

        setAvailable();
        is_initialized = true;
        return true;
    }

    /**
     * @brief Power down the SIMCOM module using both our power circuitry and an AT command
     *
     * @returns true if simcom successfully powered down, otherwise false
     */
    bool powerDownSIMModule(bool restart = false)
    {
        // Close module
        attempted_initialized = false; // reset for next time
        modem.waitResponse();
        if (restart)
        {
            StatusLogger::log(StatusLogger::LEVEL_WARNING, StatusLogger::NAME_SIMCOM, "Restarting SIMCOM device");
            modem.restart();
        }
        else
        {
            StatusLogger::log(StatusLogger::LEVEL_WARNING, StatusLogger::NAME_SIMCOM, "Powering down SIMCOM device");
            modem.poweroff();
            delay(500);
#ifdef SIM_POWER_PIN
            digitalWrite(SIM_POWER_PIN, LOW);
#endif
        }
        delay(1000);
        is_initialized = false;
        return true;
    }

    SIMMODULE_STATUS_ENUM setupSIMModule()
    {
        if (!initSIMModule())
        {
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_OFFLINE, "FAILED TO AT.");
            return FAILED_TO_AT;
        }

        StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_PARTIAL, String("Modem Name: ") + modem.getModemName());

        SimStatus sim_status = modem.getSimStatus();
        if (sim_status != SIM_READY)
        {
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_OFFLINE, "NO SIM CARD.");
            return NO_SIM_CARD;
        }

        modem.setPhoneFunctionality(1);

        // Set the CA certs to make the handshake to your SSL servers
        return NO_NETWORK;
    };

    /**
     * @brief Attempt to connect to the internet using preferred settings
     *
     * @param preferLTEm Prefer to use LTE-m settings (i.e. avoid GRPS if possible)
     * @return SIMMODULE_STATUS_ENUM The status of the connection (was successful? Was not?)
     */
    SIMMODULE_STATUS_ENUM connectToInternet(bool preferLTEm = false)
    {
        if (!modem.testAT())
        {
            return FAILED_TO_AT;
        }
        modem.setPhoneFunctionality(1);

        // Ready for cellular stuff!
        if (modem.isGprsConnected())
        {
            setAvailable();
            return INTERNET_READY;
        }
        if (!modem.isNetworkConnected())
        {
            if (!modem.waitForNetwork(3 * 60000)) // give it 3 minutes to connect to the internet
            {
                StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_PARTIAL, "No network available. Trying again...");
                return NO_NETWORK;
            }
        }
        if (preferLTEm)
        {
            StatusLogger::log(StatusLogger::LEVEL_WARNING, StatusLogger::NAME_SIMCOM, "Preferring LTE-m");
#if defined(SIM7000x) or defined(SIM7070G) or defined(SIM7600x)
            modem.setNetworkMode(38);
#endif
#if defined(SIM7000x) or defined(SIM7070G)
            modem.setPreferredMode(1); // for LTE-m
#endif
        }
        else
        {
#if defined(SIM7000x) or defined(SIM7070G) or defined(SIM7600x)
            modem.setNetworkMode(2);
#endif
#if defined(SIM7000x) or defined(SIM7070G)
            modem.setPreferredMode(3); // for LTE-m
#endif
        }
        if (modem.gprsConnect(APN))
        {
#if defined(SIM7000x) or defined(SIM7070G)
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_FULL, "Connected on Network Mode " + String(modem.getNetworkMode()) + ", and Preferred Mode " + String(modem.getPreferredMode()));
#elif defined(SIM7600x)
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_FULL, "Connected on Network Mode " + String(modem.getNetworkMode()));
#else
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_FULL, "Connected.");
#endif
            setAvailable();
            return INTERNET_READY;
        }
        StatusLogger::log(StatusLogger::LEVEL_WARNING, StatusLogger::NAME_SIMCOM, "Setting to LTE-m mode for next attempt.");
#if defined(SIM7000x) or defined(SIM7070G) or defined(SIM7600x)
        modem.setNetworkMode(38);
#endif
#if defined(SIM7000x) or defined(SIM7070G)
        modem.setPreferredMode(1); // for LTE-m
#endif
        if (modem.gprsConnect(APN))
        { // Defined in config.h
#if defined(SIM7000x) or defined(SIM7070G)
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_FULL, "Connected on Network Mode " + String(modem.getNetworkMode()) + ", and Preferred Mode " + String(modem.getPreferredMode()));
#elif defined(SIM7600x)
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_FULL, "Connected on Network Mode " + String(modem.getNetworkMode()));
#else
            StatusLogger::setBrickStatus(StatusLogger::NAME_SIMCOM, StatusLogger::FUNCTIONALITY_FULL, "Connected.");
#endif
            setAvailable();
            return INTERNET_READY;
        }
        StatusLogger::log(StatusLogger::LEVEL_VERBOSE, StatusLogger::NAME_SIMCOM, "Setting back to normal mode for full retry.");
        return NO_INTERNET;
    };

    /**
     * @brief Set the RTC time from the Network (of the cellular module)
     *
     * @param UTC
     * @returns true if the RTC time was set from the cellular connection, otherwise false
     */
    bool updateSSLTime() // set UTC to true to account for timezone
    {
        if (!SIMCOMHandler::isInternetConnected())
        // Only works if you are connected to the internet!
        {
            SIMCOMHandler::connectToInternet();
        }
        bool GSM_time_is_good = true;

        // Get the date time from the GSM modem
        String working_time = modem.getGSMDateTime(DATE_FULL);

        tmElements_t timelib_time_elements;
        time_t true_time;
        float timezone = 0;

        // 23/02/16,16:03:23+04
        uint8_t year = working_time.substring(0, working_time.indexOf("/")).toInt();
        if (year > 2100 or year < 23)
        // Something is wrong, we shouldn't be operating younger than 2023, or older than 2100
        {
            GSM_time_is_good = false;
        }
        // we've got some time around 1970 most likely, and that's wrong
        else if (75 > year && year >= 55)
        {
            GSM_time_is_good = false;
        }
        else if (year >= 2023)
        // we've got the year as 202X value
        {
            /* code */
            year -= 1970; // year will be 2023 - 1970 = 53
        }
        else if (year >= 53)
        // we've got the year relative to 1970
        {
            year = year; // year will be 53 = 53
        }
        // we've got the year relative to 2000
        {
            year += 30; // year will be 22 + 30 = 53 (years since 1970)
        }

        if (GSM_time_is_good)
        // If it looked like we got a good time from the GSM tower
        {
            StatusLogger::log(StatusLogger::LEVEL_GOOD_NEWS, StatusLogger::NAME_SIMCOM, "GSM time is: " + working_time);
            timelib_time_elements.Year = year;
            working_time = working_time.substring(working_time.indexOf("/") + 1); // trim til next slash
            timelib_time_elements.Month = working_time.substring(0, working_time.indexOf("/")).toInt();
            working_time = working_time.substring(working_time.indexOf("/") + 1); // trim til next slash
            timelib_time_elements.Day = working_time.substring(0, working_time.indexOf(",")).toInt();
            working_time = working_time.substring(working_time.indexOf(",") + 1); // trim til next slash
            timelib_time_elements.Hour = working_time.substring(0, working_time.indexOf(":")).toInt();
            working_time = working_time.substring(working_time.indexOf(":") + 1);
            timelib_time_elements.Minute = working_time.substring(0, working_time.indexOf(":")).toInt();
            working_time = working_time.substring(working_time.indexOf(":") + 1);
            if (working_time.indexOf("+") > 0)
            // if positive timezone
            {
                timelib_time_elements.Second = working_time.substring(0, working_time.indexOf("+")).toInt();
                working_time = working_time.substring(working_time.indexOf("+") + 1);
                timezone = working_time.substring(0, working_time.length()).toFloat();
            }
            else if (working_time.indexOf("-") > 0)
            // if negative timezone
            {
                timelib_time_elements.Second = working_time.substring(0, working_time.indexOf("-")).toInt();
                working_time = working_time.substring(working_time.indexOf("-") + 1);
                timezone = working_time.substring(0, working_time.length()).toFloat();
            }
            else
            {
                timelib_time_elements.Second = working_time.substring(0, working_time.length()).toInt();
            }

            true_time = makeTime(timelib_time_elements);
            true_time -= int(timezone * 3600 / 4); // timezone will be like +4 (for 4 quarters, don't ask),
        }
        else
        // We didn't get a good time from the GSM tower, so we'll use the RTC time instead.
        {
            StatusLogger::log(StatusLogger::LEVEL_ERROR, StatusLogger::NAME_SIMCOM, "GSM time was " + working_time);
            true_time = now(); // get it from the ESP (which was set by the RTC on init).
            return false;
        }

        StatusLogger::log(StatusLogger::LEVEL_VERBOSE, StatusLogger::NAME_SIMCOM, "Time being used is: " + String(true_time));

        // Set the secure client's verification times
        SIMCOMHandler::beeceptor_client_secured.setVerificationTime(elapsedDays(true_time) + 719528UL, elapsedSecsToday(true_time));
        SIMCOMHandler::openmeteo_client_secured.setVerificationTime(elapsedDays(true_time) + 719528UL, elapsedSecsToday(true_time));

        is_ssl_date_updated = true;
        return true;
    }
}