#pragma once

// configs
#include <configs/OPERATIONS_config.h>
#include <configs/HTTP_config.h>

// bricks
#include <bricks/simcom_handler.h>

// libs
#include <ArduinoJson.h>
#include <LoopbackStream.h>
#include <StatusLogger.h>

using namespace ArduinoJson; // Needed to make ArduinoJson compile 😡

namespace HTTP
{
    DynamicJsonDocument large_doc(32000);
    DynamicJsonDocument small_doc(4000);

    /**
     * @brief Get the Meteorological Data from the Open Meteo API
     *
     * @param lat Your latitude
     * @param lon
     * @returns the meteo data as a DynamicJsonDocument
     */
    DynamicJsonDocument getMeteorologicalData(float lat, float lon)
    {
        // Step 1 - Let's reconstruct the right endpoint to use whatever Lat and Lon you want to use.
        String url_endpoint = String(OPEN_METEO_ENDPOINT);
        url_endpoint.replace("DEFAULT_LAT", String(lat));
        url_endpoint.replace("DEFAULT_LON", String(lon));

        //  Step 2 - Send the GET request
        SIMCOMHandler::OpenMeteoHTTP.beginRequest();
        SIMCOMHandler::OpenMeteoHTTP.connectionKeepAlive();
        SIMCOMHandler::OpenMeteoHTTP.get(url_endpoint); // Check the "/ping" path at the BeeceptorHTTP. url (locked to influxdb)
        SIMCOMHandler::OpenMeteoHTTP.endRequest();
        delay(200); // Give it 200 ms for the server to respond.

        // Step 3 - Get the repsonse
        int response_status = SIMCOMHandler::OpenMeteoHTTP.responseStatusCode();
        String response_body = SIMCOMHandler::OpenMeteoHTTP.responseBody();
        if (response_status > 300 or response_status < 200)
        {
            StatusLogger::log(StatusLogger::LEVEL_ERROR, StatusLogger::NAME_METEO, "No valid response from the Open Meteo API.");
            Serial.println("Response body was: ");
            Serial.println(response_body);
            DynamicJsonDocument error_doc(100);
            return error_doc;
        }
        deserializeJson(large_doc, response_body);
        return HTTP::large_doc;
    }

    /**
     * @brief Post the metereological data (or any JSON) to our data endpoint on beeceptor
     *
     * @param filtered_data
     * @returns true if successfully posted, otherwise false
     */
    bool postMeteorologicalData(DynamicJsonDocument filtered_data)
    {
        // Construct and check the body
        String body;
        serializeJson(filtered_data, body);

        Serial.print("You will be POSTing this: ");
        Serial.println(body);

        // Construct into a http post request
        SIMCOMHandler::BeeceptorHTTP.beginRequest();
        SIMCOMHandler::BeeceptorHTTP.connectionKeepAlive();
        if (SIMCOMHandler::BeeceptorHTTP.post(DATA_ENDPOINT) != 0)
        {
            return false;
        }
        SIMCOMHandler::BeeceptorHTTP.sendHeader("Connection", "keep-alive");
        SIMCOMHandler::BeeceptorHTTP.sendHeader(HTTP_HEADER_CONTENT_TYPE, "application/json");

        SIMCOMHandler::BeeceptorHTTP.sendHeader(HTTP_HEADER_CONTENT_LENGTH, body.length());
        SIMCOMHandler::BeeceptorHTTP.beginBody();
        SIMCOMHandler::BeeceptorHTTP.println(body);
        SIMCOMHandler::BeeceptorHTTP.endRequest();
        if (!SIMCOMHandler::beeceptor_client_secured.connected() or SIMCOMHandler::beeceptor_client_secured.getWriteError() != 0)
        // This will happen if you lose connection in between transmissions
        {
            SIMCOMHandler::refreshConnection("we were unable to POST the data to beeceptor...");
            return false;
        }

        delay(200); // Give the server some time to act.

        // Check if our POST was successful.
        int response_status = SIMCOMHandler::BeeceptorHTTP.responseStatusCode();
        String response_body = SIMCOMHandler::BeeceptorHTTP.responseBody();
        if (response_status > 300 or response_status < 200)
        {
            StatusLogger::log(StatusLogger::LEVEL_ERROR, StatusLogger::NAME_BEECEPTOR, "Unable to POST data to Beeceptor...");
            Serial.println("Response body was: ");
            Serial.println(response_body);
            return false;
        }
        StatusLogger::log(StatusLogger::LEVEL_GOOD_NEWS, StatusLogger::NAME_BEECEPTOR, "Successfully posted.");
        return true;
    }

    /**
     * @brief Post the Device Statuses to our "status" endpoint on Beeceptor
     *
     * @param statuses_string This could actually be any String
     * @returns true if successfully posted, otherwise false
     */
    bool postStatuses(String statuses_string)
    {

        Serial.print("You will be POSTing this: ");
        Serial.println(statuses_string);

        // Construct into a http post request
        SIMCOMHandler::BeeceptorHTTP.beginRequest();
        SIMCOMHandler::BeeceptorHTTP.connectionKeepAlive();
        if (SIMCOMHandler::BeeceptorHTTP.post(STATUS_ENDPOINT) != 0)
        {
            return false;
        }
        SIMCOMHandler::BeeceptorHTTP.sendHeader("Connection", "keep-alive");
        SIMCOMHandler::BeeceptorHTTP.sendHeader(HTTP_HEADER_CONTENT_TYPE, "text/plain");

        SIMCOMHandler::BeeceptorHTTP.sendHeader(HTTP_HEADER_CONTENT_LENGTH, statuses_string.length());
        SIMCOMHandler::BeeceptorHTTP.beginBody();
        SIMCOMHandler::BeeceptorHTTP.println(statuses_string);
        SIMCOMHandler::BeeceptorHTTP.endRequest();
        if (!SIMCOMHandler::beeceptor_client_secured.connected() or SIMCOMHandler::beeceptor_client_secured.getWriteError() != 0)
        // This will happen if you lose connection in between transmissions
        {
            SIMCOMHandler::refreshConnection("we were unable to POST the data to beeceptor...");
            return false;
        }

        delay(200); // Give the server some time to act.

        // Check if our POST was successful.
        int response_status = SIMCOMHandler::BeeceptorHTTP.responseStatusCode();
        String response_body = SIMCOMHandler::BeeceptorHTTP.responseBody();
        if (response_status > 300 or response_status < 200)
        {
            StatusLogger::log(StatusLogger::LEVEL_ERROR, StatusLogger::NAME_BEECEPTOR, "Unable to POST statuses to beeceptor...");
            Serial.println("Response body was: ");
            Serial.println(response_body);
            return false;
        }
        StatusLogger::log(StatusLogger::LEVEL_GOOD_NEWS, StatusLogger::NAME_BEECEPTOR, "Successfully posted.");
        return true;
    }
}