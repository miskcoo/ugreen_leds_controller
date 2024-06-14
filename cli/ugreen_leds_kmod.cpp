#include "ugreen_leds_kmod.h"
#include <string>
#include <filesystem>
#include <fstream>

#define LEDS_CLASS_PATH  "/sys/class/leds"

int ugreen_leds_kmod::start() {
    namespace fs = std::filesystem;

    fs::path base_path = LEDS_CLASS_PATH;

    if (!fs::exists(base_path))
        return -1;

    static const char* possible_leds[] = {
        "power", "netdev", "disk1", "disk2", "disk3", 
        "disk4", "disk5", "disk6", "disk7", "disk8" 
    };

    for (size_t i = 0; i < sizeof(possible_leds) / sizeof(possible_leds[0]); ++i) {
        auto led_path = base_path / possible_leds[i];

        // check whether it is supported by the led-ugreen module
        // by checking the existence of `blink_type` file 
        if (fs::exists(led_path / "blink_type")) {
            _available_leds[i] = led_path;
        }
    }

    return -_available_leds.empty();
}

ugreen_leds_t::led_data_t ugreen_leds_kmod::get_status(led_type_t id) {
    auto it = _available_leds.find((int)id);
    if (it == _available_leds.end()) return { };

    std::ifstream ifs(it->second / "status");

    led_data_t data;
    std::string status;

    int r, g, b, brightness, t_on, t_off;

    ifs >> status 
        >> brightness 
        >> r >> g >> b
        >> t_on >> t_off;

    static std::map<std::string, ugreen_leds_t::op_mode_t> status_map = {
        { "off", op_mode_t::off },
        { "on", op_mode_t::on },
        { "blink", op_mode_t::blink },
        { "breath", op_mode_t::breath }
    };

    auto status_it = status_map.find(status);
    if (status_it == status_map.end()) 
        return { };

    data.op_mode = status_it->second;
    data.is_available = true;
    data.color_r = r;
    data.color_g = g;
    data.color_b = b;
    data.brightness = brightness;
    data.t_on = t_on;
    data.t_off = t_off;
    return data;
}

int ugreen_leds_kmod::set_onoff(led_type_t id, uint8_t status) {
    if (!status) {
        return set_brightness(id, 0);
    } else {
        auto it = _available_leds.find((int)id);
        if (it == _available_leds.end()) return -1;

        std::ofstream(it->second / "blink") << "none";
        return 0;
    }
}

int ugreen_leds_kmod::set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b) {
    auto it = _available_leds.find((int)id);
    if (it == _available_leds.end()) return -1;

    std::ofstream(it->second / "color")
        << (int)r << " " << (int)g << " " << (int)b;
    return 0;
}

int ugreen_leds_kmod::set_brightness(led_type_t id, uint8_t brightness) {
    auto it = _available_leds.find((int)id);
    if (it == _available_leds.end()) return -1;

    std::ofstream(it->second / "brightness") << brightness;
    return 0;
}

int ugreen_leds_kmod::set_blink(led_type_t id, uint16_t t_on, uint16_t t_off) {
    auto it = _available_leds.find((int)id);
    if (it == _available_leds.end()) return -1;

    std::ofstream(it->second / "blink_type")
        << "blink " << t_on << " " << t_off;
    return 0;
}

int ugreen_leds_kmod::set_breath(led_type_t id, uint16_t t_on, uint16_t t_off) {
    auto it = _available_leds.find((int)id);
    if (it == _available_leds.end()) return -1;

    std::ofstream(it->second / "blink_type")
        << "breath " << t_on << " " << t_off;
    return 0;
}

int ugreen_leds_kmod::set_oneshot(led_type_t id, uint16_t t_on, uint16_t t_off) {
    auto it = _available_leds.find((int)id);
    if (it == _available_leds.end()) return -1;

    std::ofstream(it->second / "trigger") << "oneshot\n";
    std::ofstream(it->second / "invert") << "1\n";
    std::ofstream(it->second / "delay_on") << t_on << std::endl;
    std::ofstream(it->second / "delay_off") << t_off << std::endl;

    return 0;
}

int ugreen_leds_kmod::shot(led_type_t id) {
    auto it = _available_leds.find((int)id);
    if (it == _available_leds.end()) return -1;

    std::ofstream(it->second / "shot") << "1\n";
    return 0;
}

std::shared_ptr<ugreen_leds_t> ugreen_leds_t::create_kmod_controller() {
    return std::make_shared<ugreen_leds_kmod>();
}
