// tickpath-gen: writes a synthetic ITCH 5.0 feed in sample-file framing.
//
//   tickpath-gen --scenario random|edge|violations [--seed N] [--messages N]
//                [--symbols N] --out FILE
//
// Prints the message count, byte count and FNV-1a hash of the file.

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "tickpath/itch/encode.hpp"
#include "tickpath/sim/generator.hpp"
#include "tickpath/util/fnv1a.hpp"

namespace {

struct UsageError : std::runtime_error {
    UsageError()
        : std::runtime_error("usage") {}
};

[[noreturn]] void usage() {
    throw UsageError();
}

std::uint64_t to_u64(std::string_view s) {
    try {
        return std::stoull(std::string(s));
    } catch (...) {
        usage();
    }
}

int run(const std::vector<std::string_view>& args) {
    std::string scenario = "random";
    std::string out_path;
    tickpath::sim::GeneratorConfig cfg;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto value = [&]() {
            if (i + 1 >= args.size()) {
                usage();
            }
            return args[++i];
        };
        if (args[i] == "--scenario") {
            scenario = value();
        } else if (args[i] == "--seed") {
            cfg.seed = to_u64(value());
        } else if (args[i] == "--messages") {
            cfg.book_messages = to_u64(value());
        } else if (args[i] == "--symbols") {
            const auto n = to_u64(value());
            if (n == 0 || n > 65'535) {
                usage();
            }
            cfg.symbols = static_cast<std::uint16_t>(n);
        } else if (args[i] == "--out") {
            out_path = value();
        } else {
            usage();
        }
    }
    if (out_path.empty()) {
        usage();
    }

    std::vector<tickpath::itch::AnyMessage> msgs;
    if (scenario == "random") {
        msgs = tickpath::sim::generate_random(cfg);
    } else if (scenario == "edge") {
        msgs = tickpath::sim::generate_edge_cases();
    } else if (scenario == "violations") {
        msgs = tickpath::sim::generate_contract_violations();
    } else {
        usage();
    }

    const auto bytes = tickpath::itch::encode_framed(msgs);
    std::ofstream f(out_path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    if (!f) {
        std::cerr << "write failed: " << out_path << '\n';
        return 1;
    }
    tickpath::util::Fnv1a h;
    h.bytes(bytes);
    std::cout << "messages=" << msgs.size() << " bytes=" << bytes.size() << " fnv1a=" << std::hex
              << std::setw(16) << std::setfill('0') << h.value() << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(std::vector<std::string_view>(argv + 1, argv + argc));
    } catch (const UsageError&) {
        std::cerr << "usage: tickpath-gen --scenario random|edge|violations [--seed N] "
                     "[--messages N] [--symbols N] --out FILE\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
