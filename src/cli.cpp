#include "nvbios_reader.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::filesystem::path rom;
    std::optional<std::filesystem::path> output;
    bool timings{};
    bool interactive{};
};

void print_usage() {
    std::cout
        << "NVIDIA BIOS Reader CLI " << nvbr::version << "\n\n"
        << "Usage:\n"
        << "  nvidia-bios-reader-cli <file.rom> [--timings] [-o report.txt]\n\n"
        << "Options:\n"
        << "  --timings         Decode the known CONFIG0..CONFIG5 timing fields\n"
        << "  -o, --output      Save the text report to a file\n"
        << "  -v, --version     Show the program version\n"
        << "  -h, --help        Show this help\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    if (argc == 1) {
        options.interactive = true;
        std::cout << "ROM path: ";
        std::string path;
        std::getline(std::cin, path);
        if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.size() - 2);
        }
        options.rom = path;
        return options;
    }
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "-h" || argument == "--help") {
            print_usage();
            std::exit(0);
        }
        if (argument == "-v" || argument == "--version") {
            std::cout << nvbr::version << '\n';
            std::exit(0);
        }
        if (argument == "--timings") {
            options.timings = true;
            continue;
        }
        if (argument == "-o" || argument == "--output") {
            if (++i >= argc) {
                throw std::runtime_error("Missing path after --output");
            }
            options.output = argv[i];
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            throw std::runtime_error("Unknown option: " + std::string(argument));
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

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        const nvbr::Document document = nvbr::inspect_vbios(options.rom);
        const std::string& report = options.timings
            ? document.detailed_report
            : document.report;
        std::cout << report;
        if (options.output) {
            std::ofstream file(*options.output, std::ios::binary);
            if (!file) {
                throw std::runtime_error("Could not create output report");
            }
            file << report;
            std::cout << "\nReport saved to: " << options.output->string() << '\n';
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
