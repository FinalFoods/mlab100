# mlab100
Microbiota Personal Lab 100

# Building

The first step is to **recursively** clone the `mlab100` repository.

```
git clone --recursive git@github.com:MicrobiotaLabs/mlab100.git
```

See the [`mlab100` project](mlab100/README.md) for details of the main
application firmware.

## One-time development host setup

The toolchain for the build host is automatically downloaded the first
time the top-level `make` is executed. So you do not need to install
the Xtensa toolchain explicitly. However some host run-time packages
are needed for the build to succeed.

If needed, the [esp-idf Get Started](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/)
web page provides the definitive information on setting up a host for
esp-idf development.

### Linux

Install the following packages using the relevant tool for your
installer (e.g. Kubuntu/Ubuntu/Debian use `sudo apt install`):

```
gcc git wget make libncurses-dev flex bison gperf python python-pip python-setuptools python-serial python-cryptography python-future python-pyparsing curl
```

### Mac OS X

```
sudo easy_install pip
```

### Common

Once the host support packages are installed you also need to ensure
the Python world is configured for the `esp-idf` framework:

```
python -m pip install --user -r mlab100/3rd_party/esp-idf/requirements.txt
```

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
