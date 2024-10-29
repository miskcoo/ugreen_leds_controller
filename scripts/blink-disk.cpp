#include <fstream>
#include <thread>
#include <iostream>
#include <string>
#include <chrono>

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <block device> <led device> <sleep time (in second)>\n";
        return 1;
    }

    std::string block_device = argv[1];
    std::string led_device = argv[2];
    int sleep_time_ms = int(std::stod(argv[3]) * 1000);

    std::string block_device_stat = "/sys/block/" + block_device + "/stat";
    std::string led_device_shot = "/sys/class/leds/" + led_device + "/shot";

    for (std::string line, old_line; ; old_line = line) {

        std::ifstream stat_file(block_device_stat);
        if (!stat_file.is_open()) {
            std::cerr << "Failed to open " << block_device_stat << "\n";
            return 1;
        }

        std::getline(stat_file, line);
        stat_file.close();

        if (line != old_line) {
            std::ofstream led_file(led_device_shot);
            if (!led_file.is_open()) {
                std::cerr << "Failed to open " << led_device_shot << "\n";
                return 1;
            }

            led_file << "1";
            led_file.close();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_ms));
    }

    return 0;
}
