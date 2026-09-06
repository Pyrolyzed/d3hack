#include "program/d3/resolution_util.hpp"

#include "program/d3/_util.hpp"
#include "program/config.hpp"

#include <algorithm>
#include <cmath>

namespace d3 {

    namespace {
        auto HasExtraDeviceMemory() -> bool { return g_ptDevMemMode != nullptr && *g_ptDevMemMode != 0u; }
    }  // namespace

    auto MaxResolutionHackOutputTarget() -> u32 {
        constexpr u32 kMaxDefault = 1440u;
        constexpr u32 kMaxDevMem  = 2160u;
        if (HasExtraDeviceMemory()) {
            return kMaxDevMem;
        }
        return kMaxDefault;
    }

    auto MaxResolutionHackOutputPixels() -> u64 {
        constexpr u64 kMaxPixelsDefault = 2560ull * 1440ull;  // what the 1440 line cap meant at 16:9
        constexpr u64 kMaxPixelsDevMem  = 3840ull * 2160ull;  // what the 2160 line cap meant at 16:9
        if (HasExtraDeviceMemory()) {
            return kMaxPixelsDevMem;
        }
        return kMaxPixelsDefault;
    }

    auto MaxResolutionHackOutputTargetForAspect(float aspect) -> u32 {
        const u32 line_cap = MaxResolutionHackOutputTarget();
        if (!(aspect > 0.0f)) {
            return line_cap;
        }
        // height = sqrt(budget / aspect). The +0.5 matters: a config that spells 16:9 as
        // 1.7778 rather than 1.777778 lands on 1439.99 and would otherwise be clamped to
        // 1438 -- a silent regression for every existing 16:9 user.
        const double height  = std::sqrt(static_cast<double>(MaxResolutionHackOutputPixels()) / static_cast<double>(aspect)) + 0.5;
        const u32    by_area = static_cast<u32>(height) & ~1u;
        return std::min(line_cap, by_area);
    }

    void ApplyResolutionHackBudget() {
        auto &resolution = global_config.resolution_hack;
        if (!resolution.active) {
            return;
        }
        const u32 requested = resolution.target_resolution;
        const u32 allowed   = MaxResolutionHackOutputTargetForAspect(resolution.AspectRatio());
        if (requested <= allowed) {
            return;
        }
        resolution.SetTargetRes(allowed);
        PRINT(
            "ResolutionHack: target %up -> %up. At aspect %d/1000 that asked for %llu pixels "
            "against a %llu budget; the graphics heap has never been sized for it. Enable the "
            "console's extra device memory to lift the budget.",
            requested,
            allowed,
            static_cast<int>(resolution.AspectRatio() * 1000.0f),
            static_cast<unsigned long long>(static_cast<u64>(resolution.WidthForHeight(requested & ~1u)) * (requested & ~1u)),
            static_cast<unsigned long long>(MaxResolutionHackOutputPixels())
        )
    }

}  // namespace d3
