#include "ugreen_leds_socket.h"
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define LEDS_SOCKET_PATH  "/tmp/leds-ugreen.socket"

int ugreen_leds_socket::start() {

    // open the socket file
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    // connect to the socket
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, LEDS_SOCKET_PATH);
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        sockfd = -1;
        return -1;
    }

    return 0;
}

ugreen_leds_socket::~ugreen_leds_socket() {
    if (sockfd >= 0) {
        // write data to the socket
        std::string msg = "exit\n";
        send(sockfd, msg.c_str(), msg.size(), 0);
        close(sockfd);
    }
}

ugreen_leds_t::led_data_t ugreen_leds_socket::get_status(led_type_t id) {
    std::string msg = std::to_string((int)id) + " status\n";
    send(sockfd, msg.c_str(), msg.size(), 0);
    // read data from the socket 
    char buf[256];
    recv(sockfd, buf, 256, 0);
    std::stringstream ss(buf);
    int op_mode, brightness, color_r, color_g, color_b, t_on, t_off;
    ss >> op_mode >> brightness >> color_r >> color_g >> color_b >> t_on >> t_off;
    led_data_t led_data;
    led_data.is_available = true;
    led_data.op_mode = (op_mode_t)op_mode;
    led_data.brightness = brightness;
    led_data.color_r = color_r;
    led_data.color_g = color_g;
    led_data.color_b = color_b;
    led_data.t_on = t_on;
    led_data.t_off = t_off;
    return led_data;
}

int ugreen_leds_socket::set_onoff(led_type_t id, uint8_t status) {
    std::stringstream ss;
    ss << (int)id << " ";
    if (status) ss << "on\n";
    else ss << "off\n";
    send(sockfd, ss.str().c_str(), ss.str().size(), 0);
    return 0;
}

int ugreen_leds_socket::set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b) {
    std::stringstream ss;
    ss << (int)id << " color_set " << (int)r << " " << (int)g << " " << (int)b << "\n";
    send(sockfd, ss.str().c_str(), ss.str().size(), 0);
    return 0;
}

int ugreen_leds_socket::set_brightness(led_type_t id, uint8_t brightness) {
    std::stringstream ss;
    ss << (int)id << " brightness_set " << (int)brightness << "\n";
    send(sockfd, ss.str().c_str(), ss.str().size(), 0);
    return 0;
}

int ugreen_leds_socket::set_blink(led_type_t id, uint16_t t_on, uint16_t t_off) {
    std::stringstream ss;
    ss << (int)id << " blink blink " << t_on << " " << t_off << "\n";
    send(sockfd, ss.str().c_str(), ss.str().size(), 0);
    return 0;
}

int ugreen_leds_socket::set_breath(led_type_t id, uint16_t t_on, uint16_t t_off) {
    std::stringstream ss;
    ss << (int)id << " blink breath " << t_on << " " << t_off << "\n";
    send(sockfd, ss.str().c_str(), ss.str().size(), 0);
    return 0;
}

std::shared_ptr<ugreen_leds_t> ugreen_leds_t::create_socket_controller() {
    return std::make_shared<ugreen_leds_socket>();
}
