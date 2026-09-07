#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace ANCSProtocol {
inline uint32_t readLE32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

// One Get Notification Attributes response. Feed arbitrary ATT fragments.
// Request order is app identifier, title (256 bytes), message (1024 bytes).
class Response {
public:
    enum Result { Pending, Complete, Invalid };
    void begin(uint32_t uid) {
        expected = uid;
        bytes.clear();
        app.clear(); title.clear(); message.clear();
        offset = 5; attributes = 0; headerChecked = false;
    }
    Result feed(const uint8_t *data, size_t length) {
        if (!data || length == 0 || bytes.size() + length > 1810) return Invalid;
        bytes.append(reinterpret_cast<const char *>(data), length);
        if (bytes.size() < 5) return Pending;
        const auto *b = reinterpret_cast<const uint8_t *>(bytes.data());
        if (!headerChecked) {
            if (b[0] != 0 || readLE32(b + 1) != expected) return Invalid;
            headerChecked = true;
        }
        while (bytes.size() - offset >= 3) {
            const uint8_t id = b[offset];
            const size_t size = size_t(b[offset + 1]) | (size_t(b[offset + 2]) << 8);
            const uint8_t wanted = attributes == 0 ? 0 : attributes == 1 ? 1 : 3;
            const size_t limit = id == 0 ? 512 : id == 1 ? 256 : 1024;
            if (attributes >= 3 || id != wanted || size > limit) return Invalid;
            if (bytes.size() - offset - 3 < size) return Pending;
            std::string value(bytes.data() + offset + 3, size);
            if (id == 0) app = value;
            else if (id == 1) title = value;
            else message = value;
            offset += 3 + size;
            ++attributes;
            if (attributes == 3) return bytes.size() == offset ? Complete : Invalid;
        }
        return Pending;
    }
    std::string app, title, message;
private:
    std::string bytes;
    uint32_t expected = 0;
    size_t offset = 5;
    unsigned attributes = 0;
    bool headerChecked = false;
};
}
