
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <functional>

#include "ugreen_leds.h"

#define MAX_RETRY_COUNT 5
#define USLEEP_READ_STATUS_INTERVAL 8000
#define USLEEP_READ_STATUS_RETRY_INTERVAL 3000
#define USLEEP_MODIFICATION_INTERVAL 500
#define USLEEP_MODIFICATION_RETRY_INTERVAL 3000
#define USLEEP_MODIFICATION_QUERY_RESULT_INTERVAL 2000

static std::map<std::string, ugreen_leds_t::led_type_t> led_name_map = {
    { "power",  UGREEN_LED_POWER },
    { "netdev", UGREEN_LED_NETDEV },
    { "disk1",  UGREEN_LED_DISK1 },
    { "disk2",  UGREEN_LED_DISK2 },
    { "disk3",  UGREEN_LED_DISK3 },
    { "disk4",  UGREEN_LED_DISK4 },
    { "disk5",  UGREEN_LED_DISK5 },
    { "disk6",  UGREEN_LED_DISK6 },
    { "disk7",  UGREEN_LED_DISK7 },
    { "disk8",  UGREEN_LED_DISK8 },
};

using led_type_pair = std::pair<std::string, ugreen_leds_t::led_type_t>;

ugreen_leds_t::led_data_t get_status_robust(ugreen_leds_t &leds_controller, ugreen_leds_t::led_type_t led) {
    usleep(USLEEP_READ_STATUS_INTERVAL);
    auto data = leds_controller.get_status(led);

    for (int retry_cnt = 1; !data.is_available && retry_cnt < MAX_RETRY_COUNT; ++retry_cnt) {
        usleep(USLEEP_READ_STATUS_RETRY_INTERVAL);
        data = leds_controller.get_status(led);
    }

    return data;
}

void show_leds_info(ugreen_leds_t &leds_controller, const std::vector<led_type_pair>& leds) {

    for (auto led : leds) {

        auto data = get_status_robust(leds_controller, led.second);

        if (!data.is_available) {
            std::printf("%s: unavailable or non-existent\n", led.first.c_str());
            continue;
        }

        std::string op_mode_txt = "unknown";

        switch(data.op_mode) {
            case ugreen_leds_t::op_mode_t::off:
                op_mode_txt = "off"; break;
            case ugreen_leds_t::op_mode_t::on:
                op_mode_txt = "on"; break;
            case ugreen_leds_t::op_mode_t::blink:
                op_mode_txt = "blink"; break;
            case ugreen_leds_t::op_mode_t::breath:
                op_mode_txt = "breath"; break;
        };

        std::printf("%s: status = %s, brightness = %d, color = RGB(%d, %d, %d)",
                led.first.c_str(), op_mode_txt.c_str(), (int)data.brightness, 
                (int)data.color_r, (int)data.color_g, (int)data.color_b);

        if (data.op_mode == ugreen_leds_t::op_mode_t::blink) {
            std::printf(", blink_on = %d ms, blink_off = %d ms",
                    (int)data.t_on, (int)data.t_off);
        }

        std::puts("");
    }
}

void show_help() {
    std::cerr 
        << "Usage: ugreen_leds_cli  [LED-NAME...] [-on] [-off] [-(blink|breath) T_ON T_OFF]\n"
           "                    [-color R G B] [-brightness BRIGHTNESS] [-status]\n\n"
           "       LED_NAME:    separated by white space, possible values are\n"
           "                    { power, netdev, disk[1-8], all }.\n"
           "       -on / -off:  turn on / off corresponding LEDs.\n"
           "       -blink / -breath:  set LED to the blink / breath mode. This \n"
           "                    mode keeps the LED on for T_ON millseconds and then\n"
           "                    keeps it off for T_OFF millseconds. \n"
           "                    T_ON and T_OFF should belong to [0, 65535].\n"
           "       -color:      set the color of corresponding LEDs.\n"
           "                    R, G and B should belong to [0, 255].\n"
           "       -brightness: set the brightness of corresponding LEDs.\n"
           "                    BRIGHTNESS should belong to [0, 255].\n"
           "       -status:     display the status of corresponding LEDs.\n"
        << std::endl;
}

void show_help_and_exit() {
    show_help();
    std::exit(-1);
}

ugreen_leds_t::led_type_t get_led_type(const std::string& name) {
    if (led_name_map.find(name) == led_name_map.end()) {
        std::cerr << "Err: unknown LED name " << name << std::endl;
        show_help_and_exit();
    }

    return led_name_map[name];
}

int parse_integer(const std::string& str, int low = 0, int high = 0xffff) {
    std::size_t size;
    int x = std::stoi(str, &size);

    if (size != str.size()) {
        std::cerr << "Err: " << str << " is not an integer." << std::endl;
        show_help_and_exit();
    }

    if (x < low || x > high) {
        std::cerr << "Err: " << str << " is not in [" << low << ", " << high << "]" << std::endl;
        show_help_and_exit();
    }

    return x;
}

int main(int argc, char *argv[])
{

    if (argc < 2) {
        show_help();
        return 0;
    }

    ugreen_leds_t leds_controller;
    if (leds_controller.start() != 0) {
        std::cerr << "Err: fail to open the I2C device." << std::endl;
        std::cerr << "Please check that (1) you have the root permission; " << std::endl;
        std::cerr << "              and (2) the i2c-dev module is loaded. " << std::endl;
        return -1;
    }

    std::deque<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);

    // parse LED names
    std::vector<led_type_pair> leds;

    while (!args.empty() && args.front().front() != '-') {
        if (args.front() == "all") {
            for (const auto &v : led_name_map) {
                if (get_status_robust(leds_controller, v.second).is_available)
                    leds.push_back(v);
            }
        } else {
            auto led_type = get_led_type(args.front());
            leds.emplace_back(args.front(), led_type);
        }

        args.pop_front();
    }

    // if no additional parameters, display current info
    if (args.empty()) {
        show_leds_info(leds_controller, leds);
        return 0;
    }

    // (is_modification, callback)
    using ops_pair = std::pair<bool, std::function<int(led_type_pair)>>;
    std::vector<ops_pair> ops_seq;

    while (!args.empty()) {
        if (args.front() == "-on" || args.front() == "-off") {
            // turn on / off LEDs
            uint8_t status = args.front() == "-on";
            ops_seq.emplace_back(true, [=, &leds_controller](led_type_pair led) {
                return leds_controller.set_onoff(led.second, status);
            } );

            args.pop_front();
        } else if(args.front() == "-blink" || args.front() == "-breath") {
            // set blink
            args.pop_front();

            if (args.size() < 2) {
                std::cerr << "Err: -blink / -breath requires 2 parameters" << std::endl;
                show_help_and_exit();
            }

            uint16_t t_on = parse_integer(args.front(), 0x0000, 0xffff);
            args.pop_front();
            uint16_t t_off = parse_integer(args.front(), 0x0000, 0xffff);
            args.pop_front();

            bool is_blink = (args.front() == "-blink");
            ops_seq.emplace_back(true, [=, &leds_controller](led_type_pair led) {
                if (is_blink) {
                    return leds_controller.set_blink(led.second, t_on, t_off);
                } else {
                    return leds_controller.set_breath(led.second, t_on, t_off);
                }
            } );
        } else if(args.front() == "-color") {
            // set color
            args.pop_front();

            if (args.size() < 3) {
                std::cerr << "Err: -color requires 3 parameters" << std::endl;
                show_help_and_exit();
            }

            uint8_t R = parse_integer(args.front(), 0x00, 0xff);
            args.pop_front();
            uint8_t G = parse_integer(args.front(), 0x00, 0xff);
            args.pop_front();
            uint8_t B = parse_integer(args.front(), 0x00, 0xff);
            args.pop_front();
            ops_seq.emplace_back(true, [=, &leds_controller](led_type_pair led) {
                return leds_controller.set_rgb(led.second, R, G, B);
            } );
        } else if(args.front() == "-brightness") {
            // set brightness
            args.pop_front();

            if (args.size() < 1) {
                std::cerr << "Err: -brightness requires 1 parameter" << std::endl;
                show_help_and_exit();
            }

            uint8_t brightness = parse_integer(args.front(), 0x00, 0xff);
            args.pop_front();
            ops_seq.emplace_back(true, [=, &leds_controller](led_type_pair led) {
                return leds_controller.set_brightness(led.second, brightness);
            } );
        } else if(args.front() == "-status") {
            // display the status
            args.pop_front();

            ops_seq.emplace_back(false, [=, &leds_controller](led_type_pair led) {
                show_leds_info(leds_controller, { led } );
                return 0;
            } );
        } else {
            std::cerr << "Err: unknown parameter " << args.front() << std::endl;
            show_help_and_exit();
        }
    }

    for (const auto& led : leds) {
        for (const auto& fn_pair : ops_seq) {
            bool is_modification = fn_pair.first;
            const auto &fn = fn_pair.second;
            int last_status = -1;

            for (int retry_cnt = 0; retry_cnt < MAX_RETRY_COUNT && last_status != 0; ++retry_cnt) {

                if (retry_cnt == 0) {
                    if (is_modification) 
                        usleep(USLEEP_MODIFICATION_INTERVAL);  // usleep_range(200, 0x5dc)
                } else {
                    usleep(USLEEP_MODIFICATION_RETRY_INTERVAL);  
                }

                last_status = fn(led);

                if (last_status == 0 && is_modification) {
                    usleep(USLEEP_MODIFICATION_QUERY_RESULT_INTERVAL);  
                    last_status = !leds_controller.is_last_modification_successful();
                }
            }

            if (last_status != 0) {
                std::cerr << "failed to change status!" << std::endl;
                return -1;
            }
        }
    }
    

    return 0;
}

