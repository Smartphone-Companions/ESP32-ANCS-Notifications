#include "ancs_protocol.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>
using Bytes = std::vector<uint8_t>;
using ANCSProtocol::Response;

static Bytes response(uint32_t uid, const std::string &app,
                      const std::string &title, const std::string &message) {
    Bytes bytes{0, uint8_t(uid), uint8_t(uid >> 8), uint8_t(uid >> 16), uint8_t(uid >> 24)};
    auto attribute = [&](uint8_t id, const std::string &s) {
        bytes.push_back(id);
        bytes.push_back(uint8_t(s.size()));
        bytes.push_back(uint8_t(s.size() >> 8));
        bytes.insert(bytes.end(), s.begin(), s.end());
    };
    attribute(0, app); attribute(1, title); attribute(3, message);
    return bytes;
}
int main() {
    const uint8_t uidBytes[] = {0x78, 0x56, 0x34, 0x12};
    assert(ANCSProtocol::readLE32(uidBytes) == 0x12345678);
    unsigned cases = 1;
    for (uint32_t uid : {0u, 1u, 0x12345678u, 0xFFFFFFFFu}) {
        for (const std::string &text : {std::string(), std::string("UTF-8: \xE2\x98\x83 \xF0\x9F\x93\xA9"), std::string(1024, 'x')}) {
            const std::string app = "com.apple.MobileSMS";
            const std::string title = text.empty() ? "" : "Title";
            const Bytes bytes = response(uid, app, title, text);
            // Split inside every possible command header, UID, tuple header and value byte.
            for (size_t split = 1; split < bytes.size(); ++split) {
                Response parser; parser.begin(uid);
                assert(parser.feed(bytes.data(), split) == Response::Pending);
                assert(parser.feed(bytes.data() + split, bytes.size() - split) == Response::Complete);
                assert(parser.app == app && parser.title == title && parser.message == text);
                ++cases;
            }
            for (size_t mtuPayload : {1u, 7u, 20u, 182u, 244u, 512u}) {
                Response parser; parser.begin(uid);
                for (size_t offset = 0; offset < bytes.size();) {
                    const size_t n = std::min(mtuPayload, bytes.size() - offset);
                    auto result = parser.feed(bytes.data() + offset, n);
                    offset += n;
                    assert(result == (offset == bytes.size() ? Response::Complete : Response::Pending));
                }
                assert(parser.message == text); ++cases;
            }
        }
    }
    Response parser;
    Bytes bytes = response(0x12345678, "app", "title", "message");
    parser.begin(9); assert(parser.feed(bytes.data(), bytes.size()) == Response::Invalid); ++cases;
    parser.begin(0x12345678); bytes[0] = 1;
    assert(parser.feed(bytes.data(), bytes.size()) == Response::Invalid); ++cases;
    bytes = response(1, "app", "title", "message"); bytes[6] = 0xFF; bytes[7] = 0xFF;
    parser.begin(1); assert(parser.feed(bytes.data(), bytes.size()) == Response::Invalid); ++cases;
    bytes = response(1, "app", "title", "message"); bytes[5] = 7;
    parser.begin(1); assert(parser.feed(bytes.data(), bytes.size()) == Response::Invalid); ++cases;
    bytes = response(1, "app", "title", "message"); bytes.push_back(0);
    parser.begin(1); assert(parser.feed(bytes.data(), bytes.size()) == Response::Invalid); ++cases;
    parser.begin(1); assert(parser.feed(nullptr, 0) == Response::Invalid); ++cases;
    bytes = response(1, std::string(512, 'a'), std::string(256, 't'), std::string(1024, 'm'));
    parser.begin(1); assert(parser.feed(bytes.data(), bytes.size()) == Response::Complete); ++cases;
    // Deterministic malformed input stress under ASan/UBSan, with bounded allocations.
    std::mt19937 random(2450);
    for (unsigned i = 0; i < 10000; ++i) {
        Bytes garbage(random() % 2000);
        for (auto &byte : garbage) byte = uint8_t(random());
        parser.begin(random());
        parser.feed(garbage.data(), garbage.size());
        ++cases;
    }
    std::cout << "PASS: " << cases << " protocol cases\n";
}
