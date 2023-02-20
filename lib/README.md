# Libs folder

There are three ways you can add libs to your project:
1. You can link in the `platformio.ini` file in the lib deps from an existing pio library, e.g. `lib_deps = 256dpi/MQTT@^2.5.1` → _preferred for widely-used libs (like MQTT, Sensors, Displays, etc.)_
2. You can link in the `platformio.ini` file in the lib deps from a github repo, e.g. `lib_deps = git@github.com:Sparkmate-LetsBuild/BRICK-StatusLogger.git` → _preferred for any lib that isn't an official PIO lib ([check the PIO registry](https://registry.platformio.org/) for your favourite libs)_
3. You can download them directly from GitHub (try use a release, failing that the main branch), and drop the entire folder here → _best if you anticipate changing the lib code in meaningful ways (i.e. project specific. If you find bugs/errors in other people's code, be a champion and open an issue or MR 😉)_
