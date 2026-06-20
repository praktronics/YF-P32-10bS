#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 100KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

// I2C address range (7-bit addresses)
// Standard I2C addresses are 0x03 to 0x77; 0x00-0x02 and 0x78-0x7F are reserved
#define I2C_FIRST_ADDRESS 0x03
#define I2C_LAST_ADDRESS  0x77

// YF-P32-10bS pressure sensor
// Shanghai Yufavor Sensor Instrument Co., Ltd. — P31/P32/P33 IIC protocol.
//
// Protocol (from datasheet):
//   1. Write 0x01 to register 0x01  — starts ASIC acquisition
//   2. Wait 10 ms
//   3. Write register address 0x04, repeated-start, read 3 bytes
//   4. RAW_P  = Byte1*65536 + Byte2*256 + Byte3
//   5. Inter_P = (RAW_P > 8388608) ? RAW_P - 16777216 : RAW_P
//   6. P (kPa) = Inter_P / 2^21 * RANGE_KPA + ZERO_KPA
//
// For YF-P32-10bS: range = 1000 kPa (10 bar), zero point = 18.7 kPa (1700m altitude relative to STP).
#define PRESSURE_SENSOR_ADDR  0x58
#define PRESSURE_REG_TRIGGER  0x01   // register to write to trigger acquisition
#define PRESSURE_CMD_TRIGGER  0x01   // value to write
#define PRESSURE_REG_DATA     0x04   // register to read 3 pressure bytes from
#define PRESSURE_RANGE_KPA    1000.0f  // 10 bar = 1000 kPa
#define PRESSURE_ZERO_KPA     18.7f
#define PRESSURE_DIVISOR      2097152.0f  // 2^21

// Scan a single I2C address. Returns true if a device acknowledged.
static bool i2c_bus_scan_address(uint8_t addr) {
    // A zero-length write does not generate a proper address cycle on the RP2040.
    // A 1-byte read forces the hardware to send the address and detect ACK/NACK.
    uint8_t rxdata;
    int ret = i2c_read_blocking(I2C_PORT, addr, &rxdata, 1, false);
    return (ret >= 0);
}

// Read pressure from the YF-P32-10bS using the correct datasheet protocol.
// Returns true on success and writes pressure in kPa to *pressure_kpa.
//
// Full sequence per datasheet:
//   START, 0xB0, reg=0x01, data=0x01, STOP   (trigger acquisition)
//   delay 10 ms
//   START, 0xB0, reg=0x04, REPEATED-START, 0xB1, Byte1, Byte2, Byte3, STOP
static bool read_pressure(float *pressure_kpa) {
    // Step 1: trigger acquisition — write 0x01 to register 0x01
    uint8_t trigger[2] = { PRESSURE_REG_TRIGGER, PRESSURE_CMD_TRIGGER };
    int ret = i2c_write_blocking(I2C_PORT, PRESSURE_SENSOR_ADDR, trigger, 2, false);
    if (ret < 0) {
        printf("Trigger write failed (%d)\n", ret);
        return false;
    }

    // Step 2: wait for ASIC acquisition
    sleep_ms(10);

    // Step 3: set register pointer to 0x04, then repeated-start read 3 bytes
    uint8_t reg = PRESSURE_REG_DATA;
    ret = i2c_write_blocking(I2C_PORT, PRESSURE_SENSOR_ADDR, &reg, 1, true);
    if (ret < 0) {
        printf("Register address write failed (%d)\n", ret);
        return false;
    }
    uint8_t buf[3];
    ret = i2c_read_blocking(I2C_PORT, PRESSURE_SENSOR_ADDR, buf, 3, false);
    if (ret < 0) {
        printf("Data read failed (%d)\n", ret);
        return false;
    }

    // Step 4: reconstruct 24-bit unsigned value
    uint32_t raw_p = (uint32_t)buf[0] * 65536u
                   + (uint32_t)buf[1] * 256u
                   + (uint32_t)buf[2];

    // Step 5: sign-extend — values above 2^23 represent negative counts
    int32_t inter_p = (raw_p > 8388608u)
                    ? (int32_t)(raw_p - 16777216u)
                    : (int32_t)raw_p;

    // Step 6: convert to kPa
    *pressure_kpa = ((float)inter_p / PRESSURE_DIVISOR) * PRESSURE_RANGE_KPA
                    + PRESSURE_ZERO_KPA;

    printf("raw bytes: %02x %02x %02x  raw_p: %lu  inter_p: %ld  ",
           buf[0], buf[1], buf[2], (unsigned long)raw_p, (long)inter_p);

    return true;
}

// Print a table similar to the Linux 'i2c-detect' command.
static void i2c_detect(void) {
    printf("\n     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");

    for (int row = 0; row < 8; ++row) {
        printf("%02x: ", row << 4);

        for (int col = 0; col < 16; ++col) {
            int addr = (row << 4) | col;

            if (addr < I2C_FIRST_ADDRESS || addr > I2C_LAST_ADDRESS) {
                printf("   ");
            } else {
                if (i2c_bus_scan_address((uint8_t)addr)) {
                    printf("%02x ", addr);
                } else {
                    printf("-- ");
                }
            }
        }
        printf("\n");
    }
    printf("\n");
}

int main() {
    stdio_init_all();

    // Wait for USB serial to become ready (common when using USB stdio)
    sleep_ms(5000);

    // I2C Initialisation. Using it at 100KHz.
    i2c_init(I2C_PORT, 100 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    printf("I2C Bus Scan (I2C0 @ 100 kHz, SDA=GPIO%d, SCL=GPIO%d)\n", I2C_SDA, I2C_SCL);
    printf("Scanning addresses 0x%02x to 0x%02x...\n", I2C_FIRST_ADDRESS, I2C_LAST_ADDRESS);

    i2c_detect();

    printf("Reading YF-P32-10bS at 0x%02x (range: %.0f kPa)...\n\n",
           PRESSURE_SENSOR_ADDR, (double)PRESSURE_RANGE_KPA);

    while (true) {
        float pressure_kpa;
        if (read_pressure(&pressure_kpa)) {
            printf("Pressure: %.2f kPa (%.4f bar)\n",
                   (double)pressure_kpa, (double)(pressure_kpa / 100.0f));
        }
        sleep_ms(500);
    }
}
