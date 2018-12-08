# mlab100
Microbiota Personal Lab 100

# Building

The first step is to **recursively** clone the `mlab100` repository.

```
git clone --recursive git@github.com:MicrobiotaLabs/mlab100.git
```

See [`mlab100` project](mlab100/README.md) for details of the main
application firmware.

## One-time development host setup

**TODO**: Describe host packages needed and `esp-idf` python requirements.

## Make

A standard GNU make top-level build world will complete a build of all
the necessary firmware images:

```
cd mlab100
make
```

The `sdkconfig` file defines the configuration of the software for the
relevant project. **NOTE**: If the `sdkconfig` for the project is
changed then the complete project is re-built.

**WARNING**: The `sdkconfig` will default to `/dev/ttyUSB0` as the
connection interface, and depending on your platform and what other
"serial" devices are connected you may need to use a different tty
address for any connected ESP32 hardware. The easist method is to use
a local `ESPPORT` shell environment variable to override the
`sdkconfig` file setting. This avoids the need for using menuconfig
and changing the sdkconfig default for your build/test host checked
out repo. e.g.

```
export ESPPORT=/dev/ttyUSB3
```

The above is even more relevant if building on a Mac OS X host since
the `/dev/tty*` naming scheme is not used by Darwin.

**NOTE**: For the WROVER platform port A is the JTAG interface, with
port B being the UART0 "monitor" connection.
