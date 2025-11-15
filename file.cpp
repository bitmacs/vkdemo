#include "file.h"
#include <cassert>
#include <fstream>

std::vector<char> read_binary_file(const std::string &filepath) {
    std::ifstream file{filepath, std::ios::binary | std::ios::ate};
    assert(file.is_open());
    auto size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();
    return buffer;
}
