#ifndef __UGREEN_I2C_H__
#define __UGREEN_I2C_H__

#include <stdint.h>
#include <vector>

class i2c_device_t {

private:
    int _fd = 0;

public:
    ~i2c_device_t();

    int start(const char *filename, uint16_t addr);
    std::vector<uint8_t> read_block_data(uint8_t command, uint32_t size);
    int write_block_data(uint8_t command, std::vector<uint8_t> data);
    // SMBus block write: the kernel prepends the count byte on the wire.
    // Required by the iDX601x MCU family (DXP4800 GT, iDX6011 Pro).
    int write_smbus_block_data(uint8_t command, std::vector<uint8_t> data);
    uint8_t read_byte_data(uint8_t command);
    int read_word_data(uint8_t command);

};

#endif
