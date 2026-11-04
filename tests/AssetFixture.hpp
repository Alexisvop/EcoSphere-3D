#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fixture_test {
namespace fs = std::filesystem;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void write32(std::ofstream& out, std::uint32_t value) {
    const unsigned char bytes[]{static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8),
        static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 24)};
    out.write(reinterpret_cast<const char*>(bytes), 4);
}
std::string replace(std::string s, const std::string& from, const std::string& to) {
    const auto p = s.find(from); check(p != std::string::npos, "Fixture replacement missing");
    s.replace(p, from.size(), to); return s;
}
void fixture(const fs::path& path, std::string json, const std::vector<unsigned char>& extra = {}) {
    // Tres vértices intercalados: XYZ, normal XYZ, UV; índices ushort al final.
    const std::array<float,24> vertices{0,0,0, 0,0,1, 0,0, 1,0,0, 0,0,1, 1,0, 0,1,0, 0,0,1, 0,1};
    std::vector<unsigned char> bin;
    for (float f : vertices) {
        std::uint32_t bits; std::memcpy(&bits, &f, sizeof(bits));
        for (int shift = 0; shift < 32; shift += 8) bin.push_back(static_cast<unsigned char>(bits >> shift));
    }
    bin.insert(bin.end(), {0,0,1,0,2,0,0,0});
    bin.insert(bin.end(), extra.begin(), extra.end());
    while (bin.size() % 4) bin.push_back(0);
    while (json.size() % 4) json += ' ';
    std::ofstream out(path, std::ios::binary); out.exceptions(std::ios::badbit | std::ios::failbit);
    write32(out, 0x46546c67); write32(out, 2); write32(out, static_cast<std::uint32_t>(28 + json.size() + bin.size()));
    write32(out, static_cast<std::uint32_t>(json.size())); write32(out, 0x4e4f534a); out << json;
    write32(out, static_cast<std::uint32_t>(bin.size())); write32(out, 0x004e4942);
    out.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(bin.size()));
}
const std::string valid = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
"nodes":[{"name":"parent","translation":[2,3,4],"rotation":[0,0,0.7071067811865476,0.7071067811865476],"children":[1,2]},
{"name":"child","translation":[1,0,0],"scale":[2,3,4],"mesh":0},
{"name":"matrix","matrix":[1,0,0,0,0,1,0,0,0,0,1,0,0,2,0,1],"mesh":0}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3},
{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3}]}],
"buffers":[{"byteLength":104}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":96,"byteStride":32},
{"buffer":0,"byteOffset":96,"byteLength":6}],"accessors":[
{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
{"bufferView":0,"byteOffset":12,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":0,"byteOffset":24,"componentType":5126,"count":3,"type":"VEC2"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}]})";
}
