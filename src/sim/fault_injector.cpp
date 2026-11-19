#include "tickpath/sim/fault_injector.hpp"

#include <algorithm>
#include <cstddef>

#include "tickpath/sim/rng.hpp"

namespace tickpath::sim {

std::vector<Sequenced> sequence(std::span<const itch::AnyMessage> msgs) {
    std::vector<Sequenced> out;
    out.reserve(msgs.size());
    std::uint64_t seq = 0;
    for (const auto& m : msgs) {
        out.push_back(Sequenced{.seq = ++seq, .msg = m});
    }
    return out;
}

std::vector<Sequenced> inject_faults(std::span<const Sequenced> in, const FaultConfig& cfg,
                                     FaultStats* stats) {
    Rng rng(cfg.seed);
    FaultStats local{};
    constexpr std::uint64_t ppm = 1'000'000;

    // Pass 1: drops and duplicates.
    std::vector<Sequenced> out;
    out.reserve(in.size() + in.size() / 8U);
    for (const auto& s : in) {
        if (rng.chance(cfg.drop_ppm, ppm)) {
            ++local.dropped;
            continue;
        }
        out.push_back(s);
        if (rng.chance(cfg.duplicate_ppm, ppm)) {
            ++local.duplicated;
            out.push_back(s);
        }
    }

    // Pass 2: reorder. A delayed message moves back by 1..max distance.
    if (cfg.max_reorder_distance > 0) {
        for (std::size_t i = 0; i < out.size(); ++i) {
            if (!rng.chance(cfg.reorder_ppm, ppm)) {
                continue;
            }
            const auto dist = static_cast<std::size_t>(rng.between(1, cfg.max_reorder_distance));
            const std::size_t to = std::min(i + dist, out.size() - 1U);
            if (to == i) {
                continue;
            }
            const auto first = out.begin() + static_cast<std::ptrdiff_t>(i);
            std::rotate(first, first + 1, out.begin() + static_cast<std::ptrdiff_t>(to) + 1);
            ++local.reordered;
            i = to; // do not move the same message twice
        }
    }

    if (stats != nullptr) {
        *stats = local;
    }
    return out;
}

std::vector<itch::AnyMessage> messages_of(std::span<const Sequenced> in) {
    std::vector<itch::AnyMessage> out;
    out.reserve(in.size());
    for (const auto& s : in) {
        out.push_back(s.msg);
    }
    return out;
}

} // namespace tickpath::sim
