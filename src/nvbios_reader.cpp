#include "nvbios_reader.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nvbr {

namespace fs = std::filesystem;

class ParseError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Bytes {
public:
    explicit Bytes(std::span<const std::uint8_t> data) : data_(data) {}

    [[nodiscard]] std::size_t size() const { return data_.size(); }

    [[nodiscard]] std::uint8_t u8(std::size_t offset) const {
        require(offset, 1);
        return data_[offset];
    }

    [[nodiscard]] std::uint16_t u16(std::size_t offset) const {
        require(offset, 2);
        return static_cast<std::uint16_t>(data_[offset]) |
               (static_cast<std::uint16_t>(data_[offset + 1]) << 8U);
    }

    [[nodiscard]] std::uint32_t u32(std::size_t offset) const {
        require(offset, 4);
        return static_cast<std::uint32_t>(data_[offset]) |
               (static_cast<std::uint32_t>(data_[offset + 1]) << 8U) |
               (static_cast<std::uint32_t>(data_[offset + 2]) << 16U) |
               (static_cast<std::uint32_t>(data_[offset + 3]) << 24U);
    }

    [[nodiscard]] std::span<const std::uint8_t> slice(
        std::size_t offset, std::size_t length) const {
        require(offset, length);
        return data_.subspan(offset, length);
    }

private:
    void require(std::size_t offset, std::size_t length) const {
        if (offset > data_.size() || length > data_.size() - offset) {
            std::ostringstream message;
            message << "Read outside ROM at offset 0x" << std::hex << offset
                    << " (length 0x" << length << ")";
            throw ParseError(message.str());
        }
    }

    std::span<const std::uint8_t> data_;
};

struct PciImage {
    std::size_t base{};
    std::size_t pcir{};
    std::uint16_t vendor{};
    std::uint16_t device{};
    std::size_t length{};
    std::uint8_t code_type{};
};

struct BitToken {
    char id{};
    std::uint8_t version{};
    std::uint16_t length{};
    std::uint16_t pointer{};
    std::size_t descriptor_offset{};
};

struct MemoryEntry {
    std::size_t index{};
    std::size_t offset{};
    std::uint32_t descriptor{};
    std::uint8_t memory_type_code{};
    std::uint8_t strap_code{};
    std::uint8_t variant_index{};
    std::uint8_t vendor_code{};
    std::uint8_t revision_code{};
    std::uint8_t density_code{};
    std::uint8_t organization_code{};
    std::uint8_t feature_code{};
    std::string memory_type;
    std::string vendor;
    std::string density;
    std::string organization;
    std::vector<std::size_t> physical_straps;
};

struct TimingRange {
    std::size_t index{};
    std::size_t offset{};
    std::uint16_t low{};
    std::uint16_t high{};
    std::vector<std::uint8_t> timing_ids;
    std::vector<std::size_t> timing_id_offsets;
};

struct TimingTable {
    std::size_t offset{};
    std::uint8_t version{};
    std::uint8_t header_length{};
    std::uint8_t base_length{};
    std::uint8_t extended_length{};
    std::uint8_t extended_count{};
    std::uint8_t record_count{};
    std::size_t stride{};
};

struct TimingFields {
    std::uint32_t rc{};
    std::uint32_t rfc{};
    std::uint32_t ras{};
    std::uint32_t rp{};
    std::uint32_t cl{};
    std::uint32_t wl{};
    std::uint32_t rd_rcd{};
    std::uint32_t wr_rcd{};
    std::uint32_t rpre{};
    std::uint32_t wpre{};
    std::uint32_t cdlr{};
    std::uint32_t wr{};
    std::uint32_t w2r_bus{};
    std::uint32_t r2w_bus{};
    std::uint32_t faw{};
    std::uint32_t refresh{};
    std::uint32_t rrd{};
    std::uint32_t wrcrc{};
};

struct Analysis {
    fs::path path;
    std::size_t file_size{};
    PciImage legacy;
    std::vector<PciImage> pci_images;
    std::size_t bit_offset{};
    std::string vbios_version;
    std::string chip;
    std::size_t info_offset{};
    std::size_t memory_token_offset{};
    std::size_t memory_info_offset{};
    std::size_t translation_offset{};
    std::uint8_t memory_table_version{};
    std::uint8_t memory_record_length{};
    std::vector<std::uint8_t> translation;
    std::vector<MemoryEntry> memory_entries;
    std::optional<std::size_t> timing_map_offset;
    std::vector<TimingRange> timing_ranges;
    std::optional<TimingTable> timing_table;
};

[[nodiscard]] std::uint32_t field(
    std::uint32_t value, unsigned low, unsigned high) {
    const unsigned width = high - low + 1U;
    const std::uint32_t mask = width == 32U
        ? std::numeric_limits<std::uint32_t>::max()
        : ((std::uint32_t{1} << width) - 1U);
    return (value >> low) & mask;
}

[[nodiscard]] std::string hex_value(std::uint64_t value, int width = 0) {
    std::ostringstream output;
    output << "0x" << std::uppercase << std::hex << std::setfill('0');
    if (width > 0) {
        output << std::setw(width);
    }
    output << value;
    return output.str();
}

[[nodiscard]] std::vector<std::uint8_t> read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("Could not open file: " + path.string());
    }
    const auto end = input.tellg();
    if (end <= 0) {
        throw std::runtime_error("The input file is empty");
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!input.read(reinterpret_cast<char*>(data.data()),
                    static_cast<std::streamsize>(data.size()))) {
        throw std::runtime_error("Could not read the complete input file");
    }
    return data;
}

[[nodiscard]] std::vector<PciImage> find_pci_images(const Bytes& rom) {
    std::vector<PciImage> images;
    for (std::size_t base = 0; base + 0x20 <= rom.size(); base += 0x200) {
        if (rom.u8(base) != 0x55 || rom.u8(base + 1) != 0xAA) {
            continue;
        }
        const std::size_t pcir = base + rom.u16(base + 0x18);
        if (pcir + 0x18 > rom.size()) {
            continue;
        }
        const auto signature = rom.slice(pcir, 4);
        if (!(signature[0] == 'P' && signature[1] == 'C' &&
              signature[2] == 'I' && signature[3] == 'R')) {
            continue;
        }
        const std::size_t length = static_cast<std::size_t>(rom.u16(pcir + 0x10)) * 512U;
        if (length == 0 || base + length > rom.size()) {
            continue;
        }
        images.push_back(PciImage{
            base,
            pcir,
            rom.u16(pcir + 4),
            rom.u16(pcir + 6),
            length,
            rom.u8(pcir + 0x14),
        });
    }
    return images;
}

[[nodiscard]] std::size_t find_bit(const Bytes& rom, const PciImage& legacy) {
    static constexpr std::array<std::uint8_t, 6> signature{
        0xFF, 0xB8, 'B', 'I', 'T', 0x00
    };
    const std::size_t end = std::min(rom.size(), legacy.base + legacy.length);
    for (std::size_t offset = legacy.base; offset + signature.size() <= end; ++offset) {
        if (std::equal(signature.begin(), signature.end(), rom.slice(offset, signature.size()).begin())) {
            return offset;
        }
    }
    throw ParseError("NVIDIA BIT table signature was not found");
}

[[nodiscard]] std::unordered_map<char, BitToken> parse_bit_tokens(
    const Bytes& rom, std::size_t bit_offset) {
    const std::uint8_t header_length = rom.u8(bit_offset + 8);
    const std::uint8_t token_length = rom.u8(bit_offset + 9);
    const std::uint8_t token_count = rom.u8(bit_offset + 10);
    if (header_length < 12 || token_length < 6) {
        throw ParseError("Invalid BIT table header");
    }
    std::unordered_map<char, BitToken> tokens;
    for (std::size_t i = 0; i < token_count; ++i) {
        const std::size_t offset = bit_offset + header_length + i * token_length;
        const char id = static_cast<char>(rom.u8(offset));
        tokens[id] = BitToken{
            id,
            rom.u8(offset + 1),
            rom.u16(offset + 2),
            rom.u16(offset + 4),
            offset,
        };
    }
    return tokens;
}

[[nodiscard]] std::string chip_from_bios_code(
    std::uint8_t family, std::uint8_t die) {
    const std::uint16_t code = (static_cast<std::uint16_t>(family) << 8U) | die;
    switch (code) {
        case 0x9002: return "TU102";
        case 0x9004: return "TU104";
        case 0x9006: return "TU106";
        case 0x9016: return "TU116";
        case 0x9017: return "TU117";
        case 0x9402: return "GA102";
        case 0x9403: return "GA103";
        case 0x9404: return "GA104";
        case 0x9406: return "GA106";
        case 0x9407: return "GA107";
        case 0x9502: return "AD102";
        case 0x9503: return "AD103";
        case 0x9504: return "AD104";
        case 0x9506: return "AD106";
        case 0x9507: return "AD107";
        default: return "Unknown (BIOS code " + hex_value(code, 4) + ")";
    }
}

void parse_info(
    const Bytes& rom,
    const PciImage& legacy,
    const std::unordered_map<char, BitToken>& tokens,
    Analysis& result) {
    const auto found = tokens.find('i');
    if (found == tokens.end() || found->second.length < 5) {
        result.chip = "Unknown (BIT 'i' token unavailable)";
        return;
    }
    const std::size_t offset = legacy.base + found->second.pointer;
    result.info_offset = offset;
    const std::uint8_t b0 = rom.u8(offset);
    const std::uint8_t b1 = rom.u8(offset + 1);
    const std::uint8_t b2 = rom.u8(offset + 2);
    const std::uint8_t b3 = rom.u8(offset + 3);
    const std::uint8_t b4 = rom.u8(offset + 4);
    std::ostringstream version_text;
    version_text << std::uppercase << std::hex << std::setfill('0')
                 << std::setw(2) << static_cast<unsigned>(b3) << '.'
                 << std::setw(2) << static_cast<unsigned>(b2) << '.'
                 << std::setw(2) << static_cast<unsigned>(b1) << '.'
                 << std::setw(2) << static_cast<unsigned>(b0) << '.'
                 << std::setw(2) << static_cast<unsigned>(b4);
    result.vbios_version = version_text.str();
    result.chip = chip_from_bios_code(b3, b2);
}

[[nodiscard]] std::string memory_type(std::uint8_t code) {
    switch (code) {
        case 0x0: return "DDR2";
        case 0x1: return "DDR3";
        case 0x2: return "GDDR3";
        case 0x3: return "GDDR5";
        case 0x6: return "HBM2";
        case 0x9: return "GDDR6";
        case 0xA: return "GDDR6X";
        case 0xF: return "Skip";
        default: return "Unknown " + hex_value(code);
    }
}

[[nodiscard]] std::string memory_vendor(std::uint8_t code) {
    switch (code) {
        case 0x1: return "Samsung";
        case 0x2: return "Qimonda/Infineon";
        case 0x3: return "Elpida";
        case 0x4: return "Etron";
        case 0x5: return "Nanya";
        case 0x6: return "Hynix";
        case 0x7: return "ProMOS/Mosel";
        case 0x8: return "Winbond";
        case 0x9: return "ESMT";
        case 0xF: return "Micron";
        default: return "Unknown " + hex_value(code);
    }
}

[[nodiscard]] std::string memory_density(std::uint8_t code) {
    switch (code) {
        case 0x0: return "256 Mbit";
        case 0x1: return "512 Mbit";
        case 0x2: return "1 Gbit";
        case 0x3: return "2 Gbit";
        case 0x4: return "4 Gbit";
        case 0x5: return "8 Gbit";
        case 0x6: return "16 Gbit";
        default: return "Unknown " + hex_value(code);
    }
}

[[nodiscard]] std::string memory_organization(
    std::uint8_t memory_type_code, std::uint8_t code) {
    if (memory_type_code == 0x9 || memory_type_code == 0xA) {
        switch (code) {
            case 0x1: return "x8 / double-sided clamshell";
            case 0x2: return "x16 / single-sided";
            default: return "Unknown " + hex_value(code);
        }
    }
    if (memory_type_code == 0x3) {
        switch (code) {
            case 0x2: return "double-sided clamshell";
            case 0x3: return "single-sided";
            default: return "Unknown " + hex_value(code);
        }
    }
    switch (code) {
        case 0x1: return "x8";
        case 0x2: return "x16";
        default: return "Unknown " + hex_value(code);
    }
}

[[nodiscard]] double device_clock_divisor(std::uint8_t memory_type_code) {
    switch (memory_type_code) {
        case 0x9: return 4.0; // GDDR6: NVIDIA/Afterburner MCLK to device clock.
        case 0xA: return 8.0; // GDDR6X: NVIDIA/Afterburner MCLK to device clock.
        default: return 1.0;
    }
}

[[nodiscard]] std::size_t uefi_length_after_legacy(
    const std::vector<PciImage>& images, const PciImage& legacy) {
    const std::size_t expected_base = legacy.base + legacy.length;
    for (const auto& image : images) {
        if (image.base == expected_base && image.code_type == 3) {
            return image.length;
        }
    }
    return 0;
}

[[nodiscard]] std::size_t adjusted_pointer(
    std::uint32_t raw, std::size_t legacy_length, std::size_t uefi_length) {
    return static_cast<std::size_t>(raw) + (raw > legacy_length ? uefi_length : 0U);
}

void parse_memory(
    const Bytes& rom,
    const PciImage& legacy,
    const std::unordered_map<char, BitToken>& tokens,
    Analysis& result) {
    const auto found = tokens.find('M');
    if (found == tokens.end()) {
        throw ParseError("BIT memory token 'M' was not found");
    }
    const auto& token = found->second;
    if (token.version != 2 || token.length < 5) {
        throw ParseError("Unsupported BIT memory token version or length");
    }
    const std::size_t token_offset = legacy.base + token.pointer;
    result.memory_token_offset = token_offset;
    const std::uint8_t group_count = rom.u8(token_offset);
    const std::uint16_t translation_pointer = rom.u16(token_offset + 1);
    const std::uint16_t memory_info_pointer = rom.u16(token_offset + 3);
    result.translation_offset = legacy.base + translation_pointer;
    result.memory_info_offset = legacy.base + memory_info_pointer;

    result.translation.reserve(group_count);
    for (std::size_t i = 0; i < group_count; ++i) {
        result.translation.push_back(rom.u8(result.translation_offset + i));
    }

    const std::size_t table = result.memory_info_offset;
    result.memory_table_version = rom.u8(table);
    const std::uint8_t header_length = rom.u8(table + 1);
    const std::uint8_t record_length = rom.u8(table + 2);
    const std::uint8_t record_count = rom.u8(table + 3);
    result.memory_record_length = record_length;
    if (header_length < 4 || record_length < 4) {
        throw ParseError("Invalid memory information table geometry");
    }

    result.memory_entries.reserve(record_count);
    for (std::size_t i = 0; i < record_count; ++i) {
        const std::size_t offset = table + header_length + i * record_length;
        const std::uint32_t descriptor = rom.u32(offset);
        MemoryEntry entry;
        entry.index = i;
        entry.offset = offset;
        entry.descriptor = descriptor;
        entry.memory_type_code = static_cast<std::uint8_t>(field(descriptor, 0, 3));
        entry.strap_code = static_cast<std::uint8_t>(field(descriptor, 4, 7));
        entry.variant_index = static_cast<std::uint8_t>(field(descriptor, 8, 11));
        entry.vendor_code = static_cast<std::uint8_t>(field(descriptor, 12, 15));
        entry.revision_code = static_cast<std::uint8_t>(field(descriptor, 16, 19));
        entry.density_code = static_cast<std::uint8_t>(field(descriptor, 20, 23));
        entry.organization_code = static_cast<std::uint8_t>(field(descriptor, 24, 26));
        entry.feature_code = static_cast<std::uint8_t>(field(descriptor, 27, 31));
        entry.memory_type = memory_type(entry.memory_type_code);
        if (entry.memory_type_code == 0xF) {
            entry.vendor = "";
            entry.density = "";
            entry.organization = "";
        } else {
            entry.vendor = memory_vendor(entry.vendor_code);
            entry.density = memory_density(entry.density_code);
            entry.organization = memory_organization(
                entry.memory_type_code, entry.organization_code);
        }
        for (std::size_t physical = 0; physical < result.translation.size(); ++physical) {
            if (result.translation[physical] == i) {
                entry.physical_straps.push_back(physical);
            }
        }
        result.memory_entries.push_back(std::move(entry));
    }
}

void parse_timings(
    const Bytes& rom,
    const PciImage& legacy,
    const std::unordered_map<char, BitToken>& tokens,
    Analysis& result) {
    const auto found = tokens.find('P');
    if (found == tokens.end() || found->second.length < 12) {
        return;
    }
    const std::size_t performance_offset = legacy.base + found->second.pointer;
    const std::uint32_t timing_map_raw = rom.u32(performance_offset + 4);
    const std::uint32_t timing_table_raw = rom.u32(performance_offset + 8);
    const std::size_t uefi_length = uefi_length_after_legacy(result.pci_images, legacy);
    const std::size_t map_offset = legacy.base + adjusted_pointer(
        timing_map_raw, legacy.length, uefi_length);
    const std::size_t table_offset = legacy.base + adjusted_pointer(
        timing_table_raw, legacy.length, uefi_length);

    const std::uint8_t map_version = rom.u8(map_offset);
    const std::uint8_t map_header_length = rom.u8(map_offset + 1);
    const std::uint8_t map_base_length = rom.u8(map_offset + 2);
    const std::uint8_t map_extended_length = rom.u8(map_offset + 3);
    const std::uint8_t map_extended_count = rom.u8(map_offset + 4);
    const std::uint8_t map_record_count = rom.u8(map_offset + 5);
    if (map_version != 0x11 || map_header_length < 6 ||
        map_base_length < 4 || map_extended_length < 1) {
        return;
    }
    const std::size_t map_stride = map_base_length +
        static_cast<std::size_t>(map_extended_length) * map_extended_count;
    result.timing_map_offset = map_offset;
    result.timing_ranges.reserve(map_record_count);
    for (std::size_t i = 0; i < map_record_count; ++i) {
        const std::size_t offset = map_offset + map_header_length + i * map_stride;
        TimingRange range;
        range.index = i;
        range.offset = offset;
        range.low = rom.u16(offset);
        range.high = rom.u16(offset + 2);
        range.timing_ids.reserve(map_extended_count);
        range.timing_id_offsets.reserve(map_extended_count);
        for (std::size_t group = 0; group < map_extended_count; ++group) {
            const std::size_t id_offset = offset + map_base_length + group * map_extended_length;
            range.timing_ids.push_back(rom.u8(id_offset));
            range.timing_id_offsets.push_back(id_offset);
        }
        result.timing_ranges.push_back(std::move(range));
    }

    TimingTable table;
    table.offset = table_offset;
    table.version = rom.u8(table_offset);
    table.header_length = rom.u8(table_offset + 1);
    table.base_length = rom.u8(table_offset + 2);
    table.extended_length = rom.u8(table_offset + 3);
    table.extended_count = rom.u8(table_offset + 4);
    table.record_count = rom.u8(table_offset + 5);
    table.stride = table.base_length +
        static_cast<std::size_t>(table.extended_length) * table.extended_count;
    if (table.header_length < 6 || table.stride == 0) {
        return;
    }
    static_cast<void>(rom.slice(
        table.offset + table.header_length,
        table.stride * table.record_count));
    result.timing_table = table;
}

[[nodiscard]] TimingFields decode_timing_fields(
    const Bytes& rom, std::size_t offset) {
    const std::uint32_t c0 = rom.u32(offset);
    const std::uint32_t c1 = rom.u32(offset + 4);
    const std::uint32_t c2 = rom.u32(offset + 8);
    const std::uint32_t c3 = rom.u32(offset + 12);
    const std::uint32_t c4 = rom.u32(offset + 16);
    const std::uint32_t c5 = rom.u32(offset + 20);
    return TimingFields{
        field(c0, 0, 7), field(c0, 8, 16), field(c0, 17, 23), field(c0, 24, 30),
        field(c1, 0, 6), field(c1, 7, 13), field(c1, 14, 19), field(c1, 20, 25),
        field(c2, 0, 3), field(c2, 4, 7), field(c2, 8, 14), field(c2, 16, 22),
        field(c2, 24, 27), field(c2, 28, 31), field(c3, 9, 16),
        field(c4, 3, 14), field(c4, 15, 20), field(c5, 4, 10),
    };
}

[[nodiscard]] bool timing_record_is_zero(
    const Bytes& rom, const TimingTable& table, std::uint8_t id) {
    if (id >= table.record_count) {
        return false;
    }
    const std::size_t offset = table.offset + table.header_length +
        static_cast<std::size_t>(id) * table.stride;
    const auto raw = rom.slice(offset, table.stride);
    return std::all_of(raw.begin(), raw.end(), [](std::uint8_t byte) { return byte == 0; });
}

[[nodiscard]] std::string timing_status(
    const Bytes& rom, const Analysis& analysis, std::size_t group) {
    if (!analysis.timing_table || analysis.timing_ranges.empty()) {
        return "UNAVAILABLE";
    }
    bool any_ff = false;
    bool any_valid = false;
    bool invalid = false;
    for (const auto& range : analysis.timing_ranges) {
        if (range.low == 0 && range.high == 0) {
            continue;
        }
        if (group >= range.timing_ids.size()) {
            invalid = true;
            continue;
        }
        const std::uint8_t id = range.timing_ids[group];
        if (id == 0xFF) {
            any_ff = true;
        } else if (id >= analysis.timing_table->record_count ||
                   timing_record_is_zero(rom, *analysis.timing_table, id)) {
            invalid = true;
        } else {
            any_valid = true;
        }
    }
    if (invalid) return "INVALID REFERENCE";
    if (any_valid && !any_ff) return "FULL";
    if (!any_valid && any_ff) return "EMPTY (all FF)";
    return "PARTIAL";
}

[[nodiscard]] Analysis analyze(const fs::path& path, const Bytes& rom) {
    Analysis result;
    result.path = path;
    result.file_size = rom.size();
    result.pci_images = find_pci_images(rom);
    const auto legacy = std::find_if(
        result.pci_images.begin(), result.pci_images.end(),
        [](const PciImage& image) {
            return image.vendor == 0x10DE && image.code_type == 0;
        });
    if (legacy == result.pci_images.end()) {
        throw ParseError("No NVIDIA legacy PCI expansion ROM image was found");
    }
    result.legacy = *legacy;
    result.bit_offset = find_bit(rom, result.legacy);
    const auto tokens = parse_bit_tokens(rom, result.bit_offset);
    parse_info(rom, result.legacy, tokens, result);
    parse_memory(rom, result.legacy, tokens, result);
    try {
        parse_timings(rom, result.legacy, tokens, result);
    } catch (const ParseError&) {
        result.timing_map_offset.reset();
        result.timing_ranges.clear();
        result.timing_table.reset();
    }
    return result;
}

[[nodiscard]] std::string ramcfg_bits(std::size_t code) {
    if (code > 0x1F) {
        return "outside 5-bit RAMCFG range";
    }
    return std::bitset<5>(code).to_string();
}

[[nodiscard]] std::string strap_levels(std::size_t code) {
    static constexpr std::array<std::string_view, 16> levels{
        "L/L/L", "L/L/H", "L/H/L", "L/H/H",
        "H/L/L", "H/L/H", "H/H/L", "H/H/H",
        "L/L/M", "L/M/L", "L/M/H", "L/H/M",
        "M/L/L", "M/L/H", "M/H/L", "M/H/H",
    };
    return code < levels.size() ? std::string(levels[code]) : "not decoded";
}

[[nodiscard]] std::string physical_straps(const MemoryEntry& entry) {
    if (entry.physical_straps.empty()) {
        return "not referenced by the translation table";
    }
    std::ostringstream output;
    for (std::size_t i = 0; i < entry.physical_straps.size(); ++i) {
        if (i != 0) output << ", ";
        const std::size_t code = entry.physical_straps[i];
        output << code << " (" << strap_levels(code)
               << ", RAMCFG " << ramcfg_bits(code) << ')';
    }
    return output.str();
}

[[nodiscard]] std::string compact_physical_straps(const MemoryEntry& entry) {
    if (entry.physical_straps.empty()) {
        return "Unmapped";
    }
    std::ostringstream output;
    for (std::size_t i = 0; i < entry.physical_straps.size(); ++i) {
        if (i != 0) output << ", ";
        const std::size_t code = entry.physical_straps[i];
        output << code << ": " << strap_levels(code)
               << " (" << ramcfg_bits(code) << ')';
    }
    return output.str();
}

[[nodiscard]] std::string build_report(
    const Analysis& analysis, const Bytes& rom, bool show_decoded_timings) {
    std::ostringstream out;
    out << "NVIDIA BIOS Reader " << version << "\n"
        << "========================\n\n"
        << "File:          " << analysis.path.string() << '\n'
        << "File size:     " << analysis.file_size << " bytes\n"
        << "GPU chip:      " << analysis.chip << '\n'
        << "Device ID:     " << hex_value(analysis.legacy.device, 4) << '\n'
        << "Vendor ID:     " << hex_value(analysis.legacy.vendor, 4) << '\n';
    if (!analysis.vbios_version.empty()) {
        out << "VBIOS version: " << analysis.vbios_version << '\n';
    }
    out << "Legacy image:  " << hex_value(analysis.legacy.base)
        << " (" << analysis.legacy.length << " bytes)\n"
        << "BIT table:     " << hex_value(analysis.bit_offset) << '\n'
        << "Info table:    " << hex_value(analysis.info_offset) << "\n\n"
        << "Memory tables\n"
        << "-------------\n"
        << "BIT M token:       " << hex_value(analysis.memory_token_offset) << '\n'
        << "Memory info table: " << hex_value(analysis.memory_info_offset)
        << " (version " << hex_value(analysis.memory_table_version, 2)
        << ", record length " << static_cast<unsigned>(analysis.memory_record_length) << ")\n"
        << "Strap translation: " << hex_value(analysis.translation_offset)
        << " (" << analysis.translation.size() << " declared physical codes)\n"
        << "Timing map:        ";
    if (analysis.timing_map_offset) out << hex_value(*analysis.timing_map_offset);
    else out << "unavailable";
    out << '\n' << "Timing table:      ";
    if (analysis.timing_table) {
        out << hex_value(analysis.timing_table->offset)
            << " (version " << hex_value(analysis.timing_table->version, 2)
            << ", " << static_cast<unsigned>(analysis.timing_table->record_count)
            << " records, " << analysis.timing_table->stride << " bytes each)";
    } else {
        out << "unavailable";
    }
    out << "\n\nPhysical RAMCFG translation\n"
        << "---------------------------\n";
    for (std::size_t physical = 0; physical < analysis.translation.size(); ++physical) {
        const std::size_t target = analysis.translation[physical];
        out << "Physical " << physical
            << " / RAMCFG " << ramcfg_bits(physical)
            << " / " << strap_levels(physical)
            << " -> group " << target;
        if (target < analysis.memory_entries.size()) {
            out << " (entry " << (target + 1) << ')';
        } else {
            out << " (invalid target)";
        }
        out << '\n';
    }

    out << "\nMemory support\n"
        << "--------------\n";
    const std::size_t described_profiles = static_cast<std::size_t>(std::count_if(
        analysis.memory_entries.begin(), analysis.memory_entries.end(),
        [](const MemoryEntry& entry) { return entry.memory_type_code != 0xF; }));
    const std::size_t referenced_profiles = static_cast<std::size_t>(std::count_if(
        analysis.memory_entries.begin(), analysis.memory_entries.end(),
        [](const MemoryEntry& entry) {
            return entry.memory_type_code != 0xF && !entry.physical_straps.empty();
        }));
    out << "Declared records: " << analysis.memory_entries.size()
        << "; shown descriptors: " << described_profiles
        << "; referenced descriptors: " << referenced_profiles << "\n\n";

    for (const auto& entry : analysis.memory_entries) {
        out << "Entry " << (entry.index + 1) << " / strap group " << entry.index
            << " @ " << hex_value(entry.offset) << '\n';
        if (entry.memory_type_code == 0xF) {
            out << "  Type:            Skip\n";
        } else {
            out << "  Type:            " << entry.memory_type << '\n'
                << "  Vendor:          " << entry.vendor
                << " (code " << hex_value(entry.vendor_code) << ")\n"
                << "  Density:         " << entry.density
                << " per memory device (code " << hex_value(entry.density_code) << ")\n"
                << "  Organization:    " << entry.organization
                << " (code " << hex_value(entry.organization_code) << ")\n"
                << "  Strap selector:  " << hex_value(entry.strap_code) << '\n'
                << "  Variant/rev:     " << hex_value(entry.variant_index)
                << " / " << hex_value(entry.revision_code) << '\n'
                << "  Physical straps: " << physical_straps(entry) << '\n'
                << "  Descriptor:      " << hex_value(entry.descriptor, 8) << '\n'
                << "  Timing coverage: " << timing_status(rom, analysis, entry.index) << '\n';
        }

        if (entry.memory_type_code != 0xF && !analysis.timing_ranges.empty()) {
            for (const auto& range : analysis.timing_ranges) {
                out << "    Range " << range.index << " @ " << hex_value(range.offset)
                    << ": MCLK " << range.low << "-" << range.high << " MHz";
                if (range.low == 0 && range.high == 0) {
                    out << " (unused map slot; excluded from coverage) -> ";
                } else {
                    const double divisor = device_clock_divisor(entry.memory_type_code);
                    out << " (inferred device clock, MCLK/"
                        << static_cast<unsigned>(divisor) << ": "
                        << std::fixed << std::setprecision(2)
                        << (static_cast<double>(range.low) / divisor) << '-'
                        << (static_cast<double>(range.high) / divisor) << " MHz) -> ";
                }
                if (entry.index >= range.timing_ids.size()) {
                    out << "missing group\n";
                    continue;
                }
                const std::uint8_t id = range.timing_ids[entry.index];
                out << "map byte @ " << hex_value(range.timing_id_offsets[entry.index]) << ": ";
                if (id == 0xFF) {
                    out << "FF (no timing record)\n";
                    continue;
                }
                out << "timing ID " << static_cast<unsigned>(id);
                if (!analysis.timing_table || id >= analysis.timing_table->record_count) {
                    out << " (invalid reference)\n";
                    continue;
                }
                const std::size_t timing_offset = analysis.timing_table->offset +
                    analysis.timing_table->header_length +
                    static_cast<std::size_t>(id) * analysis.timing_table->stride;
                out << " @ " << hex_value(timing_offset);
                if (timing_record_is_zero(rom, *analysis.timing_table, id)) {
                    out << " (all-zero record)";
                }
                out << '\n';
                if (show_decoded_timings && analysis.timing_table->stride >= 24) {
                    const auto t = decode_timing_fields(rom, timing_offset);
                    out << "      RC=" << t.rc << " RFC=" << t.rfc
                        << " RAS=" << t.ras << " RP=" << t.rp
                        << " CL=" << t.cl << " WL=" << t.wl
                        << " RD_RCD=" << t.rd_rcd << " WR_RCD=" << t.wr_rcd
                        << " RPRE=" << t.rpre << " WPRE=" << t.wpre
                        << " CDLR=" << t.cdlr << " WR=" << t.wr
                        << " W2R_BUS=" << t.w2r_bus << " R2W_BUS=" << t.r2w_bus
                        << " FAW=" << t.faw << " REFRESH=" << t.refresh
                        << " RRD=" << t.rrd << " WRCRC=" << t.wrcrc << '\n';
                }
            }
        }
        out << '\n';
    }

    out << "Notes\n"
        << "-----\n"
        << "- Density is per memory device; total VRAM cannot be derived from this descriptor alone.\n"
        << "- L/M/H describes the standard multilevel RAMCFG codebook; M is the midpoint voltage.\n"
        << "- Only physical codes inside the VBIOS-declared translation-table count are selectable mappings.\n"
        << "- Timing values are memory-controller register fields/cycle counts, not nanoseconds.\n"
        << "- Timing-map ranges use the NVIDIA/Afterburner MCLK domain; device clock is inferred as MCLK/4 for GDDR6 and MCLK/8 for GDDR6X.\n"
        << "- This tool reads the ROM only and never modifies it.\n";
    return out.str();
}

[[nodiscard]] std::string timing_fields_text(const TimingFields& t) {
    std::ostringstream out;
    out << "RC=" << t.rc << "  RFC=" << t.rfc
        << "  RAS=" << t.ras << "  RP=" << t.rp
        << "  CL=" << t.cl << "  WL=" << t.wl
        << "  RD_RCD=" << t.rd_rcd << "  WR_RCD=" << t.wr_rcd
        << "  RPRE=" << t.rpre << "  WPRE=" << t.wpre
        << "  CDLR=" << t.cdlr << "  WR=" << t.wr
        << "  W2R_BUS=" << t.w2r_bus << "  R2W_BUS=" << t.r2w_bus
        << "  FAW=" << t.faw << "  REFRESH=" << t.refresh
        << "  RRD=" << t.rrd << "  WRCRC=" << t.wrcrc;
    return out.str();
}

Document inspect_vbios(const fs::path& path) {
    const auto data = read_file(path);
    const Bytes rom(data);
    const Analysis analysis = analyze(path, rom);

    Document document;
    document.path = path;
    document.file_size = analysis.file_size;
    document.chip = analysis.chip;
    document.device_id = hex_value(analysis.legacy.device, 4);
    document.vendor_id = hex_value(analysis.legacy.vendor, 4);
    document.vbios_version = analysis.vbios_version;
    document.bit_offset = analysis.bit_offset;
    document.memory_info_offset = analysis.memory_info_offset;
    document.strap_translation_offset = analysis.translation_offset;
    document.timing_map_offset = analysis.timing_map_offset;
    if (analysis.timing_table) {
        document.timing_table_offset = analysis.timing_table->offset;
    }
    document.declared_memory_records = analysis.memory_entries.size();
    document.described_memory_profiles = static_cast<std::size_t>(std::count_if(
        analysis.memory_entries.begin(), analysis.memory_entries.end(),
        [](const MemoryEntry& entry) { return entry.memory_type_code != 0xF; }));
    document.referenced_memory_profiles = static_cast<std::size_t>(std::count_if(
        analysis.memory_entries.begin(), analysis.memory_entries.end(),
        [](const MemoryEntry& entry) {
            return entry.memory_type_code != 0xF && !entry.physical_straps.empty();
        }));

    document.strap_translation.reserve(analysis.translation.size());
    for (std::size_t physical = 0; physical < analysis.translation.size(); ++physical) {
        const std::size_t target = analysis.translation[physical];
        document.strap_translation.push_back(StrapTranslationView{
            physical,
            ramcfg_bits(physical),
            strap_levels(physical),
            target,
            target < analysis.memory_entries.size(),
        });
    }

    document.memory.reserve(analysis.memory_entries.size());
    for (const auto& entry : analysis.memory_entries) {
        MemoryView memory;
        memory.entry_number = entry.index + 1;
        memory.strap_group = entry.index;
        memory.descriptor_offset = entry.offset;
        memory.descriptor = entry.descriptor;
        memory.skipped = entry.memory_type_code == 0xF;
        memory.type = entry.memory_type;
        memory.vendor = entry.vendor;
        memory.density = entry.density;
        memory.organization = entry.organization;
        memory.physical_straps = compact_physical_straps(entry);
        memory.physical_straps_detail = physical_straps(entry);
        memory.coverage = memory.skipped
            ? "N/A"
            : timing_status(rom, analysis, entry.index);

        if (!memory.skipped) {
            memory.timings.reserve(analysis.timing_ranges.size());
            for (const auto& range : analysis.timing_ranges) {
                TimingView timing;
                timing.range_index = range.index;
                timing.raw_low = range.low;
                timing.raw_high = range.high;
                timing.device_clock_divisor = device_clock_divisor(entry.memory_type_code);
                timing.device_low_mhz = static_cast<double>(range.low) /
                    timing.device_clock_divisor;
                timing.device_high_mhz = static_cast<double>(range.high) /
                    timing.device_clock_divisor;
                timing.unused = range.low == 0 && range.high == 0;
                if (entry.index < range.timing_ids.size()) {
                    const std::uint8_t id = range.timing_ids[entry.index];
                    timing.map_byte_offset = range.timing_id_offsets[entry.index];
                    if (id != 0xFF) {
                        timing.timing_id = id;
                        if (analysis.timing_table && id < analysis.timing_table->record_count) {
                            const std::size_t offset = analysis.timing_table->offset +
                                analysis.timing_table->header_length +
                                static_cast<std::size_t>(id) * analysis.timing_table->stride;
                            timing.timing_record_offset = offset;
                            timing.all_zero_record = timing_record_is_zero(
                                rom, *analysis.timing_table, id);
                            if (analysis.timing_table->stride >= 24) {
                                timing.decoded_fields = timing_fields_text(
                                    decode_timing_fields(rom, offset));
                            }
                        }
                    }
                }
                memory.timings.push_back(std::move(timing));
            }
        }
        document.memory.push_back(std::move(memory));
    }

    document.report = build_report(analysis, rom, false);
    document.detailed_report = build_report(analysis, rom, true);
    return document;
}

} // namespace nvbr
