

# Commands

## Make the initial sdkconfig
`idf.py set-target esp32s3`

## Build the code
`idf.py build`

## Flash it
`idf.py flash`

## Watch the serial line
`idf.py monitor`

## Try running it in the qemu
`idf.py qemu monitor`

## Add component
`idf.py add-dependency "espressif/esp_lvgl_port^2.3.0"`