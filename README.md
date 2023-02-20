# REST API calls over 4G on the ESP32/Arduino

_⚡ a Sparkmate Let's Build boiler-plate ⚡_

## Summary

- This boilerplate gets you started with utilising HTTP REST calls over 4G with the ESP32/Arduinos.
- We will be leveraging a [SIMCOM A7600E chip (on a Waveshare dev board)](https://www.waveshare.com/a7600e-cat-1-gsm-gprs-hat.htm) to connect our ESP32 to the internet over 4G (LTE Cat 1).
- This boilerplate essentially serves as a wrapper around the very common [TinyGSM](https://github.com/vshymanskyy/TinyGSM) library, combining in what we've learnt about connecting to HTTPS endpoints, POSTing JSON requests, etc.
- You should be able to combine this boilerplate with the [MQTT AWS boilerplate](https://github.com/Sparkmate-LetsBuild/BOILERPLATE-DualCoreESP32) if you wish to leverage MQTT over 4G.

## Capabilities/Limitations

- TWO SERVERS/WEBSITES ONLY. These modules are capable of connecting to two servers for REST requests. _There may be a way to change the server/website during runtime, but so far I've not found it._
- HTTP BODY SIZE. The maximum body size is around 32 kB. This is already quite large for microprocessor devices, so you should be okay, but just be aware of this.
- FILE DOWNLOADING/UPLOADING is entirely possible, but I've never tested this, and we do not (yet) explore this in this boilerplate. [Check out the example here](https://github.com/vshymanskyy/TinyGSM/blob/master/examples/FileDownload/FileDownload.ino) if you want to explore this on your own.
- UPLOAD RATE. The fastest real-world upload rate I've ever really achieved is around 8kb/s (kilobyte per second). This is limited by the UART interface to the SIMCOM module (even at higher baud rates), so you may have better luck with SPI but you will need to modify the TinyGSMN library to leverage SPI.

## Optional Extras

All of the below optional extras have been included, but you may wish to remove them if they are not needed and/or causing you headaches.

### SSL layers (for HTTPS and 443 endpoints)

- Note that an SSL layer using Trust Anchors requires an up to date timestamp in order to issue an up to date certificate.
- We attempt to get this up to date timestamp from the cell tower in the function `updateSSLTime()`.
- In the event that we can't update the SSL time from the cell tower, we automatically fall back to the compilation time of the ESP32's firmware, as per [this point on the SSLClient readme](https://github.com/OPEnSLab-OSU/SSLclient#time).

### JSON POST requests

- JSON bodies are very common in REST requests, so that's what we use here.
- We construct the JSON bodies using the [ArduinoJSON](https://arduinojson.org/) functions, notably using `serializeJson` and `deserializeJson` to Stringify/deStringify our bodies.
- In many cases you will instead use _Influx Line Protocol_, _MQTT_, or any other range of custom/standard methods to transmit data.

# Making a GET request, filtering the results, then making a POST request _(example of repo)_

- For this example we will keep it super simple, and only upload to a simple logging endpoint to confirm that our data is coming through.
- We will download data from [Open Meteo](https://open-meteo.com/) in JSON format, and then perform a basic filter on this data.
- Then we will upload a simple JSON object of this data to a custom endpoint on [Beeceptor](https://beeceptor.com/), just so we can prove that our data pipeline from our device to the cloud works.
- If you need more complex examples involving streaming data to Influx or to a GCP endpoint (e.g. Strapi) then contact the team at [letsbuild@sparkmate.co](mailto:letsbuild@sparkmate.co) or open an issue here on Github.

## Electronics design

- You will need a 4G chip/dev-kit. This example leverages the SIMCOM A7600E dev board, but we've had success with the SIMCOM SIM7600 dev board, the A7672E chips in our ORIA Marine project, and _some_ success with the SIM7070G.

_n.b. We will be setting the on-board time on the fly, using the time response from the cell towers, but for more remote applications you will want to use a [battery powered RTC device](https://www.amazon.fr/AZDelivery-RTC-gratuite-Raspberry-microcontr%C3%B4leur/dp/B01M2B7HQB/ref=sr_1_3_sspa?crid=1PB6MLJE2DHI&keywords=rtc+arduino&qid=1676900726&sprefix=rtc+arduino%2Caps%2C349&sr=8-3-spons&sp_csd=d2lkZ2V0TmFtZT1zcF9hdGY&psc=1&smid=A1X7QLRQH87QA3)._

- Your wiring diagram will be similar to the below. Don't forget an active SIM card and an antenna!

<img src="./readme_assets/wiring%20diagram.jpg" width="500px">

### Selecting your cellular chip

- Chip selection is an important factor. The two main brands we have worked with are [UBlox](https://www.u-blox.com/en/cellular-modules) and [SIMCOM](https://www.simcom.com/module/4g.html).
- For SIMCOM, any chip that ends with "E" is for Europe, "G" is Global, and "X" is a designator suggesting that there are region specific options (e.g. the SIM7070X).

**If you want 4G**

- You need LTE Cat 1 or Cat 2.
- Cat 2 is about 5 times faster than Cat 1, but both Cat 1 and 2 are a lot faster than what we can currently reach because we're going to be limited by the UART baud rate, so both are fine.
- Chips like the A7672E and the SIM7600G are your best SIMCOM options.
- These will work with nearly all SIM card types, but we've had good success with [Soracom](https://www.soracom.io/) and [EMnify](https://www.emnify.com/) for fleet projects.

**If you want IoT connectivity**

- You have two options: LTE-m and Nb-IoT.
- We've had limited success with either of these, mostly because the coverage is not as wide-spread as you would hope (though it is constantly growing).
- Chips like the SIM7070G is your best SIMCOM option.
- These WILL NOT work with all SIM types, you need a provider that provides LTE-m and Nb-IoT in your area. Be aware, because companies like Soracom advertize themselves as IoT providers but actually don't offer these two options in a lot of their regions (including France). I think EMnify does, but still be hyper-careful!

## Operation

### OpenMeteo GET testing endpoint

- We will use the [the OpenMeteo API](https://open-meteo.com/en/docs#latitude=48.82&longitude=2.38&hourly=temperature_2m,relativehumidity_2m,rain&current_weather=true&timezone=auto) to grab today's Temperature, Humidity, and Rain levels, as well as forecasted data.
- We will then filter the results down to just today's data.
- Then construct a new .json ready to upload to our Beeceptor endpoint.

### Beeceptor POST testing endpoint

We will be POSTing to two different Beeceptor endpoints on the [sparkmate-rest-test](https://beeceptor.com/console/sparkmate-rest-test) listener:

- The `data` endpoint will have our filtered temperature, humidity, and rain results (current values only). This will happen every 30 seconds on the ESP32. This is obviously overkill but gives us an easy example to use.
- The `status` endpoint will have device data so we can monitor the health of our device remotely. This will only happen every 2 minutes.

_Be aware that free endpoints on Beeceptor have a limit of 50 requests per day. You can change the proxy name in the [./include/configs/REST_config.h](./include/configs/REST_config.h) file._

Find the POST and GET functions in the [./include/rest_handler.h](./include/rest_handler.h) file.

## Expected behaviour

### Your Serial monitor

<img src="./readme_assets/successful%20operation%20console.jpg" width="500px">

### Your Beeceptor endpoint

<img src="./readme_assets/successful%20operation%20beeceptor.jpg" width="500px">

### SSL Timeset error

<img src="./readme_assets/ssl%20timeset%20error.jpg" width="500px">

- It is highly likely you'll get an SSL timeset error (about 1 in every 2 times). This completely depends on which cell tower you connect to first and what their capability is. I am afraid I don't know an obvious way around this.
- You could try disconnecting from the internet and reconnecting, but it's still only 50/50.
- This is why for these projects I highly recommend using a battery-powered RTC to store a real timestamp _before_ you attempt to connect to the interet.

# The Sparkmate Let's Build open source policy

This boilerplate is open source, reusable, and hackable by design. **We want you to build** with it. We don't ask for credit, attribution, funding, or anything like that.
We just want you to make something cool, and we hope we've helped 😉.

Please add any issues or MRs as you see fit.

Get in touch with [letsbuild@sparkmate.co](mailto:letsbuild@sparkmate.co) for any questions or comments.
