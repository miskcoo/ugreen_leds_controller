#ifndef __DX4600_I2C_H__
#define __DX4600_I2C_H__

#include <stdint.h>
#include <vector>

class i2c_device_t {

private:
    int _fd;

public:
    ~i2c_device_t();

    int start(const char *filename, uint16_t addr);
    std::vector<uint8_t> read_block_data(uint8_t command, uint32_t size);
    int write_block_data(uint8_t command, std::vector<uint8_t> data);

};

#endif
