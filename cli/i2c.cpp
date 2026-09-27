
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <fcntl.h>

#include "i2c.h"


i2c_device_t::~i2c_device_t() {
    close();
}

int i2c_device_t::start(const char *filename, uint16_t addr) {
    close();
    _fd = open(filename, O_RDWR | O_CLOEXEC);

    if (_fd < 0) {
        int rc = _fd;
        _fd = -1;
        return rc;
    }

    int rc = ioctl(_fd, I2C_SLAVE, addr);
    if (rc < 0) {
        close();
        return rc;
    }

    return 0;
};

std::vector<uint8_t> i2c_device_t::read_block_data(uint8_t command, uint32_t size) {
    if (_fd < 0) return { };

    if (size > I2C_SMBUS_BLOCK_MAX)
        return { };

    i2c_smbus_data smbus_data;
    smbus_data.block[0] = size;

    i2c_smbus_ioctl_data ioctl_data;
    ioctl_data.size = I2C_SMBUS_I2C_BLOCK_DATA;
    ioctl_data.read_write = I2C_SMBUS_READ;
    ioctl_data.command = command;
    ioctl_data.data = &smbus_data;

    int rc = ioctl(_fd, I2C_SMBUS, &ioctl_data);

    if (rc < 0) return { };

    std::vector<uint8_t> data;
    for (uint32_t i = 0; i < size; ++i)
        data.push_back(smbus_data.block[i + 1]);

    return data;
}

int i2c_device_t::write_block_data(uint8_t command, std::vector<uint8_t> data) {
    if (_fd < 0) return -1;

    uint32_t size = data.size();
    if (size > I2C_SMBUS_BLOCK_MAX)
        size = I2C_SMBUS_BLOCK_MAX;

    i2c_smbus_data smbus_data;
    smbus_data.block[0] = size;
    for (uint32_t i = 0; i < size; ++i)
        smbus_data.block[i + 1] = data[i];

    i2c_smbus_ioctl_data ioctl_data;
    ioctl_data.size = I2C_SMBUS_I2C_BLOCK_DATA;
    ioctl_data.read_write = I2C_SMBUS_WRITE;
    ioctl_data.command = command;
    ioctl_data.data = &smbus_data;

    int rc = ioctl(_fd, I2C_SMBUS, &ioctl_data);

    return rc;
}

uint8_t i2c_device_t::read_byte_data(uint8_t command) {
    if (_fd < 0) return { };

    i2c_smbus_data smbus_data;

    i2c_smbus_ioctl_data ioctl_data;
    ioctl_data.size = I2C_SMBUS_BYTE_DATA;
    ioctl_data.read_write = I2C_SMBUS_READ;
    ioctl_data.command = command;
    ioctl_data.data = &smbus_data;

    int rc = ioctl(_fd, I2C_SMBUS, &ioctl_data);

    if (rc < 0) return { };

    return smbus_data.byte & 0xff;
}

void i2c_device_t::close() {
    if (_fd >= 0) ::close(_fd);
    _fd = -1;
}

int i2c_device_t::read_word_data(uint8_t command, uint16_t &data) {
    if (_fd < 0) return -1;
    i2c_smbus_data value {};
    i2c_smbus_ioctl_data request {};
    request.size = I2C_SMBUS_WORD_DATA;
    request.read_write = I2C_SMBUS_READ;
    request.command = command;
    request.data = &value;
    if (ioctl(_fd, I2C_SMBUS, &request) < 0) return -1;
    data = value.word;
    return 0;
}
