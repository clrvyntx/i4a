# ESP32 SPI Ring Connection Project

## Project Overview

This project implements a ring network topology using ESP32 microcontrollers connected via SPI (Serial Peripheral Interface). Each ESP32 board functions simultaneously as an SPI master and slave, creating a circular communication chain where data can flow bidirectionally through the network. This architecture enables efficient local communication within IoT nodes, supporting configurations from 2 to 5 boards in a single ring.

The system's unique feature is its dual-role approach: each board acts as a master for the next board and a slave for the previous one, enabling seamless data transmission throughout the network. One of the boards serves as an Access Point (AP), allowing the node to communicate with external networks while maintaining internal ring communication.

## Compatible Hardware
This project has been tested and verified to work with the following ESP32 boards:

- Espressif ESP32-DevKitC v4 (https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/hw-reference/esp32/get-started-devkitc.html#)

### Pinout

![alt text](./docs/esp32-devkitC-v4-pinout.png)



While the project may work with other ESP32 boards that have similar specifications, compatibility cannot be guaranteed without testing.

## Prerequisites

### ESP-IDF Installation
Before starting with this project, you need to install ESP-IDF v5.1.2. Follow these steps:

1. **Windows Installation**:
   - Download the ESP-IDF Windows Installer from the official ESP-IDF Releases page
   - Run the installer and follow the installation wizard
   - The installer will download and install:
     * ESP-IDF tools
     * Git
     * Python
     * Required build tools

2. **Linux/macOS Installation**:
   ```bash
   mkdir -p ~/esp
   cd ~/esp
   git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh
   ```

3. **Set up ESP-IDF environment variables**:
   - Windows: Run `C:\esp\esp-idf\export.bat`
   - Linux/macOS: `. ~/esp/esp-idf/export.sh`

For detailed installation instructions, visit [ESP-IDF Get Started Guide](https://docs.espressif.com/projects/esp-idf/en/v5.1.2/esp32/get-started/index.html)

## Installing and Building the Project

1. Clone this repo and cd into it:
   ```bash
   git clone https://github.com/changeemma/esp32-internet-for-all.git
   cd esp32-internet-for-all
   ```

2. Set up the ESP-IDF environment:
   ```bash
   get_idf
   ```

3. Build the project:
   ```bash
   idf.py build
   ```
   Note: Initial build will take some time while it compiles all required libraries.


## Board Configuration Options

The system supports between 2 to 5 ESP32 boards in a ring configuration:

### Possible Setups:
1. **2 boards**: Minimal configuration
   - Each ESP32 acts as both master and slave
   - Direct bidirectional communication

2. **3-4 boards**: Intermediate configuration

   Forms a ring where each board connects to its neighbors
   Required boards:

   One AP (Access Point) board
   Two or three boards from: North, South, East, or West


   Example (3 boards): AP + North + South
   Example (4 boards): AP + North + South + East

3. **5 boards**: Full configuration
   - Complete board setup with all roles:
     * North board
     * South board
     * East board
     * West board
     * AP (Access Point) board

### Board Roles
Each ESP32 in the ring must be configured with its specific role using the configuration pins (see GPIO Configuration section).

## GPIO - Connections for ESP32 - Physical ports ring connections

```
Master Device          Slave Device
-------------         -------------
GPIO 23 (MOSI)  ────► GPIO 13 (MOSI)
GPIO 18 (SCLK)  ────► GPIO 14 (SCLK)
GPIO 5  (CS)    ────► GPIO 15 (CS)
```

#### Role Configuration System
**Configuration Pins (Pull-Up):**
- CONFIG_PIN_0: GPIO 22 (least significant bit)
- CONFIG_PIN_1: GPIO 21
- CONFIG_PIN_2: GPIO 16 (most significant bit)


## IDF configuration: sdkconfig file

```bash
   get_idf menuconfig
```

Variables:
```
CONFIG_LWIP_L2_TO_L3_COPY=y (Enable copy between Layer2 and Layer3 packets)
CONFIG_LWIP_IP_FORWARD=y (Enable IP forwarding)
LWIP_IP_DEBUG=y (Only for debug)
```

## ESP32 Component Testing Guide

This guide explains how to add unit tests to your ESP32 component and how to run them using the ESP-IDF unit testing framework.

### Adding Tests to Your Component

1. Create a `test` directory in your component folder:

   ```
   your_component/
   ├── include/
   ├── your_component.c
   ├── CMakeLists.txt
   └── test/
       ├── test_your_component.c
       └── CMakeLists.txt
   ```

2. Write your test cases in `test_your_component.c`:

   ```c
   #include "unity.h"
   #include "your_component.h"

   TEST_CASE("Test case description", "[your_component]")
   {
       // Your test code here
       TEST_ASSERT_EQUAL(expected, your_function());
   }
   ```

3. Create a `CMakeLists.txt` file in the `test` directory:

   ```cmake
   idf_component_register(SRC_DIRS "."
                          INCLUDE_DIRS "."
                          REQUIRES your_component unity)
   ```

### Modifying the Unit Test App

1. Navigate to the unit test app directory:

   ```
   cd $IDF_PATH/tools/unit-test-app
   ```

2. Modify the `CMakeLists.txt` file to include your component's directory:

   ```cmake
   list(APPEND EXTRA_COMPONENT_DIRS "/path/to/your/components/directory")
   ```

   Replace `/path/to/your/components/directory` with the actual path to your components.


### Running the Tests


1. Navigate to the unit test app directory:

   ```
   cd $IDF_PATH/tools/unit-test-app
   ```

2. Configure the project:

   ```
   idf.py menuconfig
   ```

   Ensure that "Component config" -> "Unity unit testing library" -> "Enable unit tests" is enabled.

3. Build the unit test app:

   ```
   idf.py -T build all build
   ```
   or 

   ```
   idf.py -T build your_component build
   ```

4. Flash the app to your ESP32:

   ```
   idf.py -p PORT flash
   ```

   Replace `PORT` with your ESP32's port (e.g., `/dev/ttyUSB0`).

5. Run the tests:

   ```
   idf.py -p PORT monitor
   ```

   In the monitor, press 't' to see the test menu and select your tests to run.

## Further Reading


For more detailed information on ESP-IDF's build system and component structure, refer to the official ESP-IDF Programming Guide:

- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html#cmake-file-globbing)
- [Unit Testing in ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)
