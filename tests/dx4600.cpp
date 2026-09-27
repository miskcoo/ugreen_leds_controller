#include <cassert>
#include <cstdarg>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <map>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../cli/ugreen_leds.cpp"

static std::vector<std::string> opened;
static std::vector<std::string> closed;
static std::map<int, std::string> descriptors;
static std::map<std::string, int> reads;
static std::map<std::string, int> signatures;
static std::string open_failure;
static std::string slave_failure;
static int next_fd = 0; // Exercise the valid descriptor zero case, too.
static int successful_after = 1;
static unsigned long last_size;
static std::vector<uint8_t> last_frame;
static uint8_t ack = 0;
static std::vector<unsigned int> sleeps;

extern "C" int __wrap_open(const char *path, int, ...) {
    opened.emplace_back(path);
    if (path == open_failure) return -1;
    descriptors[next_fd] = path;
    return next_fd++;
}
extern "C" int __wrap_close(int fd) {
    assert(descriptors.count(fd));
    closed.push_back(descriptors.at(fd));
    descriptors.erase(fd);
    return 0;
}
extern "C" int __wrap_usleep(unsigned int usec) {
    sleeps.push_back(usec);
    return 0;
}
extern "C" int __wrap_ioctl(int fd, unsigned long operation, ...) {
    assert(descriptors.count(fd));
    const auto &path = descriptors.at(fd);
    va_list args;
    va_start(args, operation);
    if (operation == I2C_SLAVE) {
        assert(va_arg(args, int) == 0x3a);
        va_end(args);
        return path == slave_failure ? -1 : 0;
    }
    assert(operation == I2C_SMBUS);
    auto *request = va_arg(args, i2c_smbus_ioctl_data *);
    va_end(args);
    last_size = request->size;
    if (request->size == I2C_SMBUS_WORD_DATA) {
        assert(request->command == 0x5a && request->read_write == I2C_SMBUS_READ);
        ++reads[path];
        if (signatures[path] < 0 || reads[path] < successful_after) return -1;
        request->data->word = signatures[path];
    } else if (request->size == I2C_SMBUS_BYTE_DATA) {
        assert(request->command == 0x80);
        request->data->byte = ack;
    } else if (request->read_write == I2C_SMBUS_WRITE) {
        assert(request->command == 0);
        last_frame.assign(request->data->block + 1,
                          request->data->block + 1 + request->data->block[0]);
    } else {
        // An all-zero status frame must remain unavailable.
        std::fill(request->data->block + 1, request->data->block + 12, 0);
    }
    return 0;
}

static void reset() {
    assert(descriptors.empty());
    opened.clear(); closed.clear(); reads.clear(); signatures.clear(); sleeps.clear();
    open_failure.clear(); slave_failure.clear(); successful_after = 1; next_fd = 0;
}

int main() {
    namespace fs = std::filesystem;
    assert(is_dx4600_product("DX4600"));
    assert(is_dx4600_product("DX4600+"));
    assert(is_dx4600_product("DX4600 Pro"));
    assert(!is_dx4600_product("UGREEN DX4600"));
    assert(!is_dx4600_product("DX4600 Engineering"));
    assert(!is_dx4600_product("DH2600"));
    assert(!is_dx4600_product(""));

    const auto root = fs::temp_directory_path() / ("dx4600-test-" + std::to_string(getpid()));
    fs::create_directories(root);
    for (const auto &entry : std::vector<std::pair<std::string, std::string>> {
        {"i2c-10", "SMBus I801 adapter"}, {"i2c-2", "Synopsys DesignWare I2C adapter"},
        {"i2c-1", "SMBus I801 adapter"}, {"i2c-0", "Unrelated adapter"},
        {"i2c-bad", "SMBus I801 adapter"}, {"i2c-3garbage", "SMBus I801 adapter"}}) {
        fs::create_directories(root / entry.first / "device");
        // Exercise both name locations found in sysfs.
        std::ofstream(root / entry.first / (entry.first == "i2c-1" ? "device/name" : "name")) << entry.second;
    }
    {
        i2c_device_t i2c;
        signatures["/dev/i2c-1"] = 0;
        signatures["/dev/i2c-2"] = -1;
        signatures["/dev/i2c-10"] = 0xc5b2;
        assert(start_controller(i2c, true, root) == 0);
        assert((opened == std::vector<std::string>{"/dev/i2c-1", "/dev/i2c-2", "/dev/i2c-10"}));
        assert(reads["/dev/i2c-1"] == 3 && reads["/dev/i2c-2"] == 3 && reads["/dev/i2c-10"] == 1);
        assert(closed.size() == 2);
        assert(std::count(sleeps.begin(), sleeps.end(), 100000) == 6);
    }
    reset();
    {
        i2c_device_t i2c;
        open_failure = "/dev/i2c-1";
        slave_failure = "/dev/i2c-2";
        signatures["/dev/i2c-10"] = 0xc5b2;
        successful_after = 3;
        assert(start_controller(i2c, true, root) == 0);
        assert(reads["/dev/i2c-10"] == 3);
    }
    reset();
    {
        i2c_device_t i2c;
        assert(start_controller(i2c, true, root) == -1);
        assert(descriptors.empty());
        assert(opened.size() == 3 && closed.size() == 3);
        assert(start_controller(i2c, true, root / "absent") == -1);
    }
    reset();
    {
        i2c_device_t i2c;
        assert(start_controller(i2c, false, root) == 0);
        assert(opened.size() == 1 && reads.empty());
        assert(opened[0] == "/dev/i2c-1" || opened[0] == "/dev/i2c-10");
        assert(i2c.start("/dev/i2c-1", 0x3a) == 0);
        assert(closed.size() == 1); // Restart closes the previous descriptor.
    }
    reset();
    {
        // Exercise the actual existing LED frame and ACK code without hardware.
        ugreen_leds_t controller;
        // A fresh controller has no open descriptor: no accidental ioctl on fd 0.
        assert(!controller.is_last_modification_successful());
        assert(!controller.get_status(UGREEN_LED_POWER).is_available);
    }
    {
        i2c_device_t i2c;
        assert(i2c.start("/dev/i2c-1", 0x3a) == 0);
        ack = 0;
        assert(i2c.read_byte_data(0x80) == 0);
        ack = 1;
        assert(i2c.read_byte_data(0x80) == 1);
        assert(i2c.write_block_data(0, {0, 0xa0, 1, 0, 0, 2, 255, 0, 0, 0, 1, 0xa2}) == 0);
        assert(last_size == I2C_SMBUS_I2C_BLOCK_DATA && last_frame.size() == 12);
    }
    assert(descriptors.empty());
    fs::remove_all(root);
    std::cout << "DX4600 discovery, retries, descriptor lifecycle and I2C checks passed\n";
}
