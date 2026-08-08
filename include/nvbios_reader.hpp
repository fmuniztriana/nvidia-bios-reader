#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nvbr {

#ifndef NVBR_VERSION
#define NVBR_VERSION "0.3.0-dev"
#endif

inline constexpr const char* version = NVBR_VERSION;
inline constexpr const char* project_name = "NVIDIA BIOS Reader";
inline constexpr const char* project_author = "Felipe Muniz";
inline constexpr const char* project_handle = "@fmuniztriana";
inline constexpr const char* project_url =
    "https://github.com/fmuniztriana/nvidia-bios-reader";
inline constexpr const char* copyright = "Copyright (c) 2026 Felipe Muniz";

struct TimingView {
    std::size_t range_index{};
    std::uint16_t raw_low{};
    std::uint16_t raw_high{};
    double device_low_mhz{};
    double device_high_mhz{};
    double device_clock_divisor{1.0};
    bool unused{};
    std::optional<std::uint8_t> timing_id;
    std::size_t map_byte_offset{};
    std::optional<std::size_t> timing_record_offset;
    bool all_zero_record{};
    std::string decoded_fields;
};

struct MemoryView {
    std::size_t entry_number{};
    std::size_t strap_group{};
    std::size_t descriptor_offset{};
    std::uint32_t descriptor{};
    bool skipped{};
    std::string type;
    std::string vendor;
    std::string density;
    std::string organization;
    std::string physical_straps;
    std::string physical_straps_detail;
    std::string coverage;
    std::vector<TimingView> timings;
};

struct StrapTranslationView {
    std::size_t physical_code{};
    std::string ramcfg_bits;
    std::string electrical_levels;
    std::size_t target_group{};
    bool valid_target{};
};

struct Document {
    std::filesystem::path path;
    std::size_t file_size{};
    std::string chip;
    std::string device_id;
    std::string vendor_id;
    std::string vbios_version;
    std::size_t bit_offset{};
    std::size_t memory_info_offset{};
    std::size_t strap_translation_offset{};
    std::optional<std::size_t> timing_map_offset;
    std::optional<std::size_t> timing_table_offset;
    std::size_t declared_memory_records{};
    std::size_t described_memory_profiles{};
    std::size_t referenced_memory_profiles{};
    std::vector<StrapTranslationView> strap_translation;
    std::vector<MemoryView> memory;
    std::string report;
    std::string detailed_report;
};

Document inspect_vbios(const std::filesystem::path& path);

} // namespace nvbr
