#include "dx4600_leds.h"

int dx4600_leds_t::start() {
    return _i2c.start("/dev/i2c-1", 0x3a);
}

static int compute_checksum(const std::vector<uint8_t>& data, int size) {
    if (size < 2 || size > (int)data.size()) 
        return 0;

    int sum = 0;
    for (int i = 0; i < size; ++i)
        sum += (int)data[i];

    return sum;
}

static bool verify_checksum(const std::vector<uint8_t>& data) {
    int size = data.size();
    if (size < 2) return false;
    int sum = compute_checksum(data, size - 2);
    return sum != 0 && sum == (data[size - 1] | (((int)data[size - 2]) << 8));
}

static void append_checksum(std::vector<uint8_t>& data) {
    int size = data.size();
    int sum = compute_checksum(data, size);
    data.push_back((sum >> 8) & 0xff);
    data.push_back(sum & 0xff);
}

dx4600_leds_t::led_data_t dx4600_leds_t::get_status(int id) {
    led_data_t data { };

    if (id < 0 || id > 6) return data;

    auto raw_data = _i2c.read_block_data(0x81 + id, 0xb);
    if (raw_data.size() != 0xb || !verify_checksum(raw_data)) 
        return data;

    switch (raw_data[0]) {
        case 0: data.op_mode = op_mode_t::off; break;
        case 1: data.op_mode = op_mode_t::on; break;
        case 2: data.op_mode = op_mode_t::blink; break;
        case 3: data.op_mode = op_mode_t::breath; break;
        default: return data;
    };


    data.brightness = raw_data[1];
    data.color_r = raw_data[2];
    data.color_g = raw_data[3];
    data.color_b = raw_data[4];
    int t_hight = (((int)raw_data[5]) << 8) | raw_data[6];
    int t_low = (((int)raw_data[7]) << 8) | raw_data[8];
    data.t_on = t_low;
    data.t_off = t_hight - t_low;

    return data;
}

int dx4600_leds_t::_change_status(int id, uint8_t command, std::array<uint8_t, 3> params) {
    if (id < 0 || id > 6) return 0;

    std::vector<uint8_t> data {
    //   3c    3b    3a
        0x00, 0xa0, 0x01,
    //     39        38         37
    //     36        35         34
        0x00, 0x00, command, 
        params[0], params[1], params[2]
    };

    append_checksum(data);
    data[0] = id;
    return _i2c.write_block_data(id, data);
}

int dx4600_leds_t::set_onoff(int id, uint8_t status) {
    return _change_status(id, 0x03, { status, 0x00, 0x00 } );
}

int dx4600_leds_t::set_blink(int id, uint16_t t_on, uint16_t t_off) {
    uint16_t t_hight = t_on + t_off;
    uint16_t t_low = t_on;
    return _change_status(id, 0x04, { 
        (uint8_t)(t_hight >> 8), 
        (uint8_t)(t_hight & 0xff), 
        (uint8_t)(t_low >> 8),
//            (uint8_t)(t_low & 0xff)
    } );
}

int dx4600_leds_t::set_rgb(int id, uint8_t r, uint8_t g, uint8_t b) {
    return _change_status(id, 0x02, { r, g, b } );
}

int dx4600_leds_t::set_brightness(int id, uint8_t brightness) {
    return _change_status(id, 0x01, { brightness, 0x00, 0x00 } );
}
