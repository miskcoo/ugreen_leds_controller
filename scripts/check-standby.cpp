#include <fstream>
#include <thread>
#include <vector>
#include <iostream>
#include <string>
#include <chrono>
#include <optional>
#include <limits>

#include <fcntl.h>
#include <linux/hdreg.h>
#include <sys/ioctl.h>
#include <unistd.h>

std::optional<bool> is_standby_mode(const std::string &device, bool log_errors) {

    int fd = open(device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        if (log_errors)
            std::cerr << "Failed to open device: " << device
                << " (will retry every cycle)" << std::endl;
        return std::nullopt;
    }

    unsigned char args[4] = { WIN_CHECKPOWERMODE1, 0, 0, 0 };

    if (ioctl(fd, HDIO_DRIVE_CMD, args) == -1) {
        if (log_errors)
            std::cerr << "ioctl failed in checking power mode of " << device
                << " (will retry every cycle)" << std::endl;
        close(fd);
        return std::nullopt;
    }

    close(fd);

    return args[2] == 0x00;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }

    tokens.push_back(str.substr(start));
    return tokens;
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
    std::string normal_colors_as_string = argv[3];
    std::vector<std::string> block_device_paths;
    std::vector<std::string> led_device_color_paths;

    std::cout << normal_colors_as_string;

    std::vector<std::string> normal_colors = split(normal_colors_as_string, ',');

    for (int i = 0; i < num_devices; i++) {
        std::string block_device = argv[preleading_args + 2 * i];
        std::string led_device = argv[preleading_args + 2 * i + 1];


        block_device_paths.push_back(
                "/dev/" + block_device);
        led_device_color_paths.push_back(
                "/sys/class/leds/" + led_device + "/color");
    }

    std::vector<bool> device_standby(num_devices, false);

    // Failures can be transient (device busy, controller reset), so a failed
    // check must not permanently disable a device; retry it every cycle.
    // These counters only suppress repeated logging while a failure persists.
    std::vector<long> check_failures(num_devices, 0);
    std::vector<long> led_failures(num_devices, 0);

    while (true) {
        for (int i = 0; i < num_devices; i++) {

            auto is_standby = is_standby_mode(block_device_paths[i],
                    check_failures[i] == 0);

            if (!is_standby.has_value()) {
                // saturate so a persistent failure cannot overflow the counter
                if (check_failures[i] < std::numeric_limits<long>::max())
                    ++check_failures[i];
                continue;
            }

            if (check_failures[i] > 0) {
                std::cerr << "Power mode check of " << block_device_paths[i]
                    << " recovered after " << check_failures[i]
                    << " failed cycles" << std::endl;
                check_failures[i] = 0;
            }

            if (is_standby.value() != device_standby[i]) {
                std::fstream led_color(led_device_color_paths[i], std::ios::in | std::ios::out);
                if (!led_color) {
                    if (led_failures[i] == 0)
                        std::cerr << "Failed to open led color file: "
                            << led_device_color_paths[i]
                            << " (will retry every cycle)" << std::endl;
                    // saturate so a persistent failure cannot overflow the counter
                    if (led_failures[i] < std::numeric_limits<long>::max())
                        ++led_failures[i];
                    continue;
                }

                if (led_failures[i] > 0) {
                    std::cerr << "Opening led color file " << led_device_color_paths[i]
                        << " recovered after " << led_failures[i]
                        << " failed cycles" << std::endl;
                    led_failures[i] = 0;
                }

                std::string current_color;
                std::getline(led_color, current_color);

                std::string normal_color = normal_colors[i];

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
