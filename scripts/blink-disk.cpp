#include <fstream>
#include <thread>
#include <vector>
#include <iostream>
#include <string>
#include <chrono>

int main(int argc, char *argv[]) {
    if (argc < 4 || argc % 2 != 0) {
        std::cerr << "Usage: " << argv[0] << " <sleep time (in second)>"
            << " <block device 1> <led device 1> <block device 2> <led device 2>...\n";
        return 1;
    }

    int num_devices = (argc - 2) / 2;
    int sleep_time_ms = int(std::stod(argv[1]) * 1000);

    std::vector<std::string> block_device_stat_paths;
    std::vector<std::string> led_device_shot_paths;

    for (int i = 0; i < num_devices; i++) {
        std::string block_device = argv[2 + 2 * i];
        std::string led_device = argv[3 + 2 * i];

        block_device_stat_paths.push_back(
                "/sys/block/" + block_device + "/stat");
        led_device_shot_paths.push_back(
                "/sys/class/leds/" + led_device + "/shot");
    }

    std::vector<bool> device_status(num_devices, true);
    std::vector<std::string> old_lines(num_devices);

    while (true) {
        for (int i = 0; i < num_devices; i++) {

            if (!device_status[i]) {
                continue;
            }

            std::ifstream stat_file(block_device_stat_paths[i]);

            if (!stat_file.is_open()) {
                std::cerr << "Failed to open " << block_device_stat_paths[i] << "\n";
                device_status[i] = false;
                continue;
            }

            std::string line;
            std::getline(stat_file, line);
            stat_file.close();

            if (line != old_lines[i]) {

                std::ofstream led_file(led_device_shot_paths[i]);
                if (!led_file.is_open()) {
                    std::cerr << "Failed to open " << led_device_shot_paths[i] << "\n";
                    device_status[i] = false;
                    continue;
                }

                led_file << "1";
                led_file.close();

                old_lines[i] = std::move(line);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_ms));
    }

    return 0;
}
