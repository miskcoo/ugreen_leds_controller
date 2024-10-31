#include <fstream>
#include <thread>
#include <vector>
#include <iostream>
#include <string>
#include <chrono>
#include <optional>

#include <fcntl.h>
#include <linux/hdreg.h>
#include <sys/ioctl.h>
#include <unistd.h>

std::optional<bool> is_standby_mode(const std::string &device) {

    int fd = open(device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        std::cerr << "Failed to open device: " << device << std::endl;
        return std::nullopt;
    }

    unsigned char args[4] = { WIN_CHECKPOWERMODE1, 0, 0, 0 };

    if (ioctl(fd, HDIO_DRIVE_CMD, args) == -1) {
        std::cerr << "ioctl failed in checking power mode of " << device << std::endl;
        close(fd);
        return std::nullopt;
    }

    close(fd);

    return args[2] == 0x00;
}

int main(int argc, char *argv[]) {

    constexpr int preleading_args = 4;

    if (argc < preleading_args + 2 || (argc - preleading_args) % 2 != 0) {
        std::cerr << "Usage: " << argv[0] 
            << " <check interval (in second)>"
            << " <disk standby color>"
            << " <disk normal color>"
            << " <block device 1> <led device 1> <block device 2> <led device 2>...\n";
        return 1;
    }

    int num_devices = (argc - preleading_args) / 2;
    int checker_sleep_ms = int(std::stod(argv[1]) * 1000);

    std::string standby_color = argv[2];
    std::string normal_color = argv[3];
    std::vector<std::string> block_device_paths;
    std::vector<std::string> led_device_color_paths;

    std::cout << normal_color;

    for (int i = 0; i < num_devices; i++) {
        std::string block_device = argv[preleading_args + 2 * i];
        std::string led_device = argv[preleading_args + 2 * i + 1];


        block_device_paths.push_back(
                "/dev/" + block_device);
        led_device_color_paths.push_back(
                "/sys/class/leds/" + led_device + "/color");
    }

    std::vector<bool> device_valid(num_devices, true);
    std::vector<bool> device_standby(num_devices, false);

    while (true) {
        for (int i = 0; i < num_devices; i++) {

            if (!device_valid[i]) {
                continue;
            }

            auto is_standby = is_standby_mode(block_device_paths[i]);

            if (!is_standby.has_value()) {
                device_valid[i] = false;
                continue;
            }

            if (is_standby.value() != device_standby[i]) {
                std::fstream led_color(led_device_color_paths[i], std::ios::in | std::ios::out);
                if (!led_color) {
                    std::cerr << "Failed to open led color file: " 
                        << led_device_color_paths[i] << std::endl;
                    device_valid[i] = false;
                    continue;
                }

                std::string current_color;
                std::getline(led_color, current_color);

                if (device_standby[i] && current_color == standby_color) {
                    led_color << normal_color << std::endl;
                } else if (!device_standby[i] && current_color == normal_color) {
                    led_color << standby_color << std::endl;
                }

                device_standby[i] = is_standby.value();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(checker_sleep_ms));
    }

    return 0;
}
