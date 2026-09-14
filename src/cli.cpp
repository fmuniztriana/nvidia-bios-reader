#include "nvbios_reader.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

#ifdef _WIN32
using NativeChar = wchar_t;
using NativeString = std::wstring;
using NativeStringView = std::wstring_view;
#else
using NativeChar = char;
using NativeString = std::string;
using NativeStringView = std::string_view;
#endif

struct Options {
    std::filesystem::path rom;
    std::optional<std::filesystem::path> output;
    std::optional<std::pair<std::size_t, std::size_t>> compare_profiles;
    bool timings{};
    bool ramcfg{};
    bool interactive{};
};

void print_usage() {
    std::cout
        << "NVIDIA BIOS Reader CLI " << nvbr::version << "\n\n"
        << "Usage:\n"
        << "  nvidia-bios-reader-cli <file.rom> [--timings] [-o report.txt]\n"
        << "  nvidia-bios-reader-cli <file.rom> --ramcfg [-o ramcfg.txt]\n"
        << "  nvidia-bios-reader-cli <file.rom> --compare-profiles A B [-o diff.txt]\n\n"
        << "Options:\n"
        << "  --timings         Decode legacy fields and inventory raw records (GDDR7: raw-only)\n"
        << "  --ramcfg          Show physical codes, translation bytes, targets and aliases\n"
        << "  --compare-profiles A B\n"
        << "                    Compare two 1-based memory entries byte by byte\n"
        << "  -o, --output      Save the text report to a file\n"
        << "  -v, --version     Show the program version\n"
        << "  -h, --help        Show this help\n";
}

bool is_argument(NativeStringView value, std::string_view expected) {
    if (value.size() != expected.size()) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != static_cast<NativeChar>(expected[index])) {
            return false;
        }
    }
    return true;
}

std::filesystem::path read_interactive_path() {
#ifdef _WIN32
    std::wcout << L"ROM path: ";
    std::wstring path;
    std::getline(std::wcin, path);
    if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"') {
        path = path.substr(1, path.size() - 2);
    }
#else
    std::cout << "ROM path: ";
    std::string path;
    std::getline(std::cin, path);
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        path = path.substr(1, path.size() - 2);
    }
#endif
    return std::filesystem::path(path);
}

Options parse_options(int argc, NativeChar** argv) {
    Options options;
    if (argc == 1) {
        options.interactive = true;
        options.rom = read_interactive_path();
        return options;
    }
    for (int i = 1; i < argc; ++i) {
        const NativeStringView argument = argv[i];
        if (is_argument(argument, "-h") || is_argument(argument, "--help")) {
            print_usage();
            std::exit(0);
        }
        if (is_argument(argument, "-v") || is_argument(argument, "--version")) {
            std::cout
                << nvbr::project_name << ' ' << nvbr::version << '\n'
                << nvbr::copyright << '\n'
                << nvbr::project_url << '\n';
            std::exit(0);
        }
        if (is_argument(argument, "--timings")) {
            options.timings = true;
            continue;
        }
        if (is_argument(argument, "--ramcfg")) {
            options.ramcfg = true;
            continue;
        }
        if (is_argument(argument, "--compare-profiles")) {
            if (i + 2 >= argc) {
                throw std::runtime_error(
                    "--compare-profiles requires two 1-based entry numbers");
            }
            const auto parse_entry = [](const NativeChar* text) -> std::size_t {
                const NativeString value(text);
                std::size_t consumed = 0;
                const unsigned long long parsed = std::stoull(value, &consumed, 10);
                if (consumed != value.size() || parsed == 0) {
                    throw std::runtime_error(
                        "Profile entry numbers must be positive integers");
                }
                return static_cast<std::size_t>(parsed);
            };
            const std::size_t first = parse_entry(argv[++i]);
            const std::size_t second = parse_entry(argv[++i]);
            options.compare_profiles = std::pair{first, second};
            continue;
        }
        if (is_argument(argument, "-o") || is_argument(argument, "--output")) {
            if (++i >= argc) {
                throw std::runtime_error("Missing path after --output");
            }
            options.output = argv[i];
            continue;
        }
        if (!argument.empty() && argument.front() == static_cast<NativeChar>('-')) {
            throw std::runtime_error(
                "Unknown option: " +
                nvbr::path_to_utf8(std::filesystem::path(argv[i])));
        }
        if (!options.rom.empty()) {
            throw std::runtime_error("Only one ROM file can be analyzed at a time");
        }
        options.rom = argv[i];
    }
    if (options.rom.empty()) {
        throw std::runtime_error("No ROM file was provided");
    }
    return options;
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#else
int main(int argc, char** argv) {
#endif
    try {
        const Options options = parse_options(argc, argv);
        const nvbr::Document document = nvbr::inspect_vbios(options.rom);
        const std::string report = options.compare_profiles
            ? nvbr::compare_profiles(
                document,
                options.compare_profiles->first,
                options.compare_profiles->second)
            : (options.ramcfg
                ? nvbr::ramcfg_report(document)
                : (options.timings ? document.detailed_report : document.report));
        std::cout << report;
        if (options.output) {
            std::ofstream file(*options.output, std::ios::binary);
            if (!file) {
                throw std::runtime_error("Could not create output report");
            }
            file << report;
            std::cout << "\nReport saved to: "
                      << nvbr::path_to_utf8(*options.output) << '\n';
        }
        if (options.interactive) {
            std::cout << "\nPress Enter to close...";
            std::cin.get();
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
