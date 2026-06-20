# YF-P32-10bS pressure sensor
This is a 0-10bar (1 MPa) pressure sensor with an I2C (IIC) interface. There is little to no documentation available for this sensor at the time this project was created (June 2026).

This project has some simple Raspberry Pi Pico C code to scan the I2C bus to see attached devices - if you've wired this up correctly, you'll see the device appear at 0x58.  The code lays out the read & conversion procedure to read the data from the sensor over the I2C bus.

## Wiring

|Pico GPIO|Pico physical pin|Function|Sensor wire|
|-|-|-|-|
|GPIO 8|11|SDA|Yello|
|GPIO 9|12|SCL|White|

For testing at 100kHz I2C, you can use the RP2040's internal pullups.

![Sensor image](./docs/sensor.png "Sensor")
![Connection to Pico](./docs/connection_to_pico.jpg "I2C connection")
![Connection to Air](./docs/connection_to_air.jpg "Air connection")