# Pressure controller
An ESP32-based 5 channel pressure controller prototype. 

Its main purpose just to show pressure.
~~Latter (soon) it is going to manage 220V socket to control an air compressor.~~
It is already can turn ON\OFF the power socket depending on the pressure and maximum continuous working time following by a minimum rest period. Parameters are hardcoded as of yet. Now it serves as an additional smart pressure relay for the air compressor. 

*WARNING! This project is not intended to be used as a safety device software!!! Always use air compressor's own pressure relay and safety valves!!!*

OTA firmware update support is also in the list.

Parts:
- [ESP32](https://www.espressif.com/en/products/socs/esp32/overview) Doit DevKit V1
- ST7789 1.3" 240x240 IPS LCD screen with SPI-interface
- A button
- A few RJ-9 sockets
- A bunch of resistors for voltage dividers
- Wires
- Unnamed 5V analog pressure sensors from Ali
- A small food container from the nearest store
- A dual power socket case
- Couple Crydom CX240D5 solid-state relays
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [LVGL](https://lvgl.io)

A very brief review [on YouTube](https://youtu.be/pnPwiyVQR3A).

A few images:

![1](images/1.jpg?raw=true)
![2](images/2.jpg?raw=true)
![3](images/3.jpg?raw=true)
![4](images/4.jpg?raw=true)
![5](images/5.jpg?raw=true)
![5](images/6.jpg?raw=true)

## How to start
Clone this repo and its submodules:

```
git clone --recurse-submodules https://github.com/vheathen/esp32-pressure-controller
cd esp32-pressure-controller
idf.py menuconfig
```

You should go to the `Pressure sensor` menu and configure a button GPIO number (the button lost its functions so you can skip this step). Then you should set GPIO pins you've connected your socket controlling relays to.
Pressure sensor to control socket, relay index and pressure settings\timings are currently defined in 'relay_control.c'.
It is necessary to configure LVGL in `Component config -> LittlevGL (LVGL)...` sections.
Please include Montserrat 12pt font.

It necessary to change some values in LVGL's `lv_conf.h` file. Temporary it is possible to change `components/lv_port_esp32/components/lvgl/lv_conf.h` and make following changes:

`LV_USE_USER_DATA` should be `1`:
```
#define LV_USE_USER_DATA        1
```

Some theme changes:
```
#define LV_THEME_DEFAULT_FLAG               LV_THEME_MATERIAL_FLAG_DARK
#define LV_THEME_DEFAULT_FONT_SMALL         &lv_font_montserrat_12
```

It is also necessary to comment `include(...)` line in `components/lv_port_esp32/CMakeLists.txt` file:
```
cmake_minimum_required(VERSION 3.5)

# include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS components/lvgl_esp32_drivers components/lvgl_esp32_drivers/lvgl_touch components/lvgl_esp32_drivers/lvgl_tft)

if (NOT DEFINED PROJECT_NAME)
	project(lvgl-demo)
endif (NOT DEFINED PROJECT_NAME)
```

After that you can build and flash project with:
```
idf.py build
idf.py flash monitor
```

## Got a suggestion?

If you have any questions or improvement advices feel free to contact me!