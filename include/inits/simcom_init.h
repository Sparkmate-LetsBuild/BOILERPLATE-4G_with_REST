#pragma once

// configs
#include <configs/HARDWARE_config.h>
#include <configs/OPERATIONS_config.h>

// libs
#include <ArduinoHttpClient.h> // How we handle HTTP requests
#include <LoopbackStream.h>
#include <StatusLogger.h>

// --hardware agnosticism
#if defined(SIM7070G)
#define TINY_GSM_MODEM_SIM7070
#elif defined(SIM7000x)
#define TINY_GSM_MODEM_SIM7000SSL
#elif defined(SIM7600x) or defined(A7672x)
#define TINY_GSM_MODEM_SIM7600
#endif
#include <TinyGsmClient.h> // How we talk to the SIMCOM module
#include <SSLClient.h>
#include <configs/trust_anchors.h>
#ifdef DEBUG_AT_COMMANDS
#include <StreamDebugger.h>
#endif
// SETUPs
namespace SIMCOMHandler
{

#ifdef DEBUG_AT_COMMANDS
    StreamDebugger debugger(SerialAT_4g, Serial);
    TinyGsm modem(debugger);
#else
    TinyGsm modem(SerialAT_4g);
#endif

#ifndef SIM7070G
    TinyGsmClient beeceptor_client(modem, 0);
    TinyGsmClient openmeteo_client(modem, 1);
    SSLClient beeceptor_client_secured(beeceptor_client, TAs, (size_t)TAs_NUM, RESERVED_NOISE_PIN);
    SSLClient openmeteo_client_secured(openmeteo_client, TAs, (size_t)TAs_NUM, RESERVED_NOISE_PIN);
#else
    TinyGsmClientSecure beeceptor_client_secured(modem, 0);
    TinyGsmClientSecure openmeteo_client_secured(modem, 1);
#endif
    // Create a new HttpClient for Beeceptor for this session (it won't connect until we ask it to)
    HttpClient BeeceptorHTTP(beeceptor_client_secured, BEECEPTOR_URL, 443); // 443 needed for SSL
    // Create a new HttpClient for OpenMeteo for this session (it won't connect until we ask it to)
    HttpClient OpenMeteoHTTP(openmeteo_client_secured, OPEN_METEO_URL, 443); // 443 needed for SSL

    // SETUP DATATYPES
    enum SIMMODULE_STATUS_ENUM
    {
        FAILED_TO_AT,   // can't even talk to simcom module
        NO_SIM_CARD,    // can't find a sim card
        NO_NETWORK,     // can't find a network
        NO_INTERNET,    // can't connect to internet
        INTERNET_READY, // internet ready
    };

    bool is_available = false;
    String debug_body_ended_with;
    String owner = "";

    bool is_initialized = false;
    bool is_ssl_date_updated = false;
    SIMMODULE_STATUS_ENUM setupSIMModule();

    /**
     * @brief A wrapper around modem.isGprsConnected (for readibility)
     *
     * @returns true if internet connected, false if not
     */
    bool isInternetConnected()
    {
        return modem.isGprsConnected();
    }

    /**
     * @brief Set if the SIMCOM module is available for HTTP requests
     *
     * @param available True if available for other requests, false if busy
     */
    void setAvailable(bool available = true)
    {
        is_available = available;
    }

    /**
     * @brief Wait until the SIMCOM module is available for a HTTP request
     *
     *
     * @param new_owner a string to refer to as the "owner" we're waiting for when this function returns true.
     * @param timeout
     * @returns true if became available before the timeout was hit, otherwise false
     */
    bool waitUntilAvailable(String new_owner, int timeout = 30000)
    {
        long int start_time = millis();
        if (!SIMCOMHandler::is_initialized)
        {
            SIMCOMHandler::setupSIMModule();
        }
        while (!is_available)
        // While the simcom module isn't available keep checking
        {
            // Serial.print(owner);
            if (start_time + timeout < millis())
            // timeout hit
            {
                return false;
            }
            vTaskDelay(100); // Check every 100 milliseconds
        }
        setAvailable(false);
        owner = new_owner;
        vTaskDelay(100); // Check every 100 milliseconds
        return true;
    }

    /**
     * @brief Send data to the modem, regardless of size. Basically a chunked .println()
     *
     * @param this_send_data the data you wish to send
     * @param this_client the http client
     *
     * @returns True if we were able to stream the data to the client without issue, otherwise false
     */
    bool stream_data_to_modem(String this_send_data, HttpClient *this_client)
    {
        const int ONE_CHUNK = 1024; // Defined by the minimum between the SIMCOM module or the ESP32 Serial.
        if (this_send_data.length() < ONE_CHUNK)
        { // less than ONE_CHUNK, send it
            this_client->println(this_send_data);
        }
        else
        { // more than ONE_CHUNK, break it into number of chunks
            int chunks = this_send_data.length() / ONE_CHUNK;
            for (int i = 0; i < chunks; i++)
            {
                this_client->print(this_send_data.substring(i * ONE_CHUNK, (i * ONE_CHUNK) + ONE_CHUNK));
            }
            int last_chunk = this_send_data.length() - chunks * ONE_CHUNK;
            if (last_chunk)
            {
                this_client->println(this_send_data.substring(this_send_data.length() - last_chunk, this_send_data.length() - 1));
            }
        }
        return true;
    }

    /**
     * @brief Send data to the modem from a loopback stream regardless of size
     *
     * @param this_send_data_stream the data you wish to send nested within a LoopbackStream
     * @param this_client the http client
     *
     * @returns True if we were able to stream the data to the client without issue, otherwise false
     */
    bool stream_data_to_modem(LoopbackStream *this_send_data_stream, HttpClient *this_client)
    {
        const int ONE_CHUNK = 1360; // Defined by the minimum between the SIMCOM module or the ESP32 Serial.
        String STR_CHUNK_BUFF;
        char c;

#ifdef DEBUG_HTTP_BODY // We're not using the logger here because the body could be far longer than the expected msg length that the logger would allow
        Serial.println("Body will be: ");
#endif
        while (this_send_data_stream->available() >= ONE_CHUNK)
        {
            STR_CHUNK_BUFF = "";
            while (STR_CHUNK_BUFF.length() < ONE_CHUNK)
            {
                c = this_send_data_stream->read();
                STR_CHUNK_BUFF.concat(c);
            }
#ifdef DEBUG_HTTP_BODY // We're not using the logger here because the body could be far longer than the expected msg length that the logger would allow
            Serial.print(STR_CHUNK_BUFF);
#endif
            this_client->print(STR_CHUNK_BUFF);
            if (this_client->getWriteError() != 0)
            {
                Serial.println("Matt a write error was set: " + this_client->getWriteError());
                return false;
            }
        }
        STR_CHUNK_BUFF = "";
        while (this_send_data_stream->available())
        {
            c = this_send_data_stream->read();
            STR_CHUNK_BUFF.concat(c);
        }
        this_client->println(STR_CHUNK_BUFF);
        if (this_client->getWriteError() != 0)
        {
            Serial.println("Matt a write error was set: " + this_client->getWriteError());
            return false;
        }
#ifdef DEBUG_HTTP_BODY // We're not using the logger here because the body could be far longer than the expected msg length that the logger would allow
        Serial.println(STR_CHUNK_BUFF);
        Serial.println("END OF BODY");
#endif
        debug_body_ended_with = STR_CHUNK_BUFF;
        return true;
    }

    /**
     * @brief Refresh the connection of all HTTP clients to keep things from going to 💩
     *
     * @param reason Log a reason as to why we're refreshing the connection.
     */
    void refreshConnection(String reason = "")
    {
        StatusLogger::log(StatusLogger::LEVEL_WARNING, StatusLogger::NAME_SIMCOM, "Refreshing the client because " + reason);
        vTaskDelay(500);
        SIMCOMHandler::BeeceptorHTTP.stop();
        SIMCOMHandler::OpenMeteoHTTP.stop();
        SIMCOMHandler::modem.streamClear();
        SIMCOMHandler::beeceptor_client_secured.clearWriteError();
        SIMCOMHandler::openmeteo_client_secured.clearWriteError();
        vTaskDelay(1500);
    }

    // Declare for later definition
    bool updateSSLTime();
};
