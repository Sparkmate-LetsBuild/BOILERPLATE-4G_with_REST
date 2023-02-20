#pragma once

// Open Meteo endpoint
#define OPEN_METEO_URL "api.open-meteo.com"
#define OPEN_METEO_ENDPOINT "/v1/forecast?latitude=DEFAULT_LAT&longitude=DEFAULT_LON&hourly=temperature_2m,relativehumidity_2m,rain&current_weather=true&timeformat=unixtime"

// Beeceptor endpoints
#define BEECEPTOR_URL "sparkmate.proxy.beeceptor.com"
#define DATA_ENDPOINT "/data"
#define STATUS_ENDPOINT "/status"