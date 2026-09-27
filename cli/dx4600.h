#ifndef UGREEN_DX4600_H
#define UGREEN_DX4600_H

#include <fstream>
#include <string>

inline bool is_dx4600_product(const std::string &product) {
    return product == "DX4600" || product == "DX4600+" || product == "DX4600 Pro";
}

inline bool is_dx4600() {
    std::ifstream dmi("/sys/class/dmi/id/product_name");
    std::string product;
    std::getline(dmi, product);
    return is_dx4600_product(product);
}

#endif
