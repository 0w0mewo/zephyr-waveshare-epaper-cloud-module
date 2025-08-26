## Waveshare 2.13inch E-Paper Cloud Module Zephyr OS Board support
A quick Hacky port to make DISCONTINUED Waveshare 2.13inch E-Paper Cloud Module usable on Zephyr OS

### Details of the board
- ESP32-WROOM-32(NRND) module on board
- Unknown 250x128, 2.13 inch e-paper display. However, it's compatible with SSD1608 driver.

More details: https://www.waveshare.com/wiki/2.13inch_e-Paper_Cloud_Module

### Quick start
```
west init -m https://github.com/0w0mewo/zephyr-waveshare-epaper-cloud-module --mr master

# update Zephyr modules
cd app
west update

# build
west build -b waveshare_2_13inch_epaper_cloud_module/esp32/procpu -p always app

# flash
west flash
```