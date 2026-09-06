#pragma once

#include "types.h"

namespace d3 {

    // Vertical line cap. Unchanged: this is what the config parser and the GUI slider
    // clamp against, and it is aspect-blind on purpose -- the aspect ratio is not
    // guaranteed to have been parsed yet when a key is applied.
    auto MaxResolutionHackOutputTarget() -> u32;

    // d3hack-custom: the same ceiling expressed in pixels.
    //
    // The line caps above were written when width was locked to height * 16/9, which made a
    // line count a pixel count. Configurable AspectRatio broke that: "1440p" at 21:9 is
    // 3440x1440 = 4.95 Mpx against 16:9's 3.69 Mpx, 34% more render-target memory for an
    // unchanged cap and an unchanged heap bump. The budgets below are exactly the two
    // configurations the line caps already sanctioned at 16:9 -- 2560x1440 and 3840x2160 --
    // so nothing about a 16:9 setup moves.
    auto MaxResolutionHackOutputPixels() -> u64;

    // Largest vertical target that keeps `aspect` inside the pixel budget. Returns the line
    // cap unchanged at 16:9.
    auto MaxResolutionHackOutputTargetForAspect(float aspect) -> u32;

    // Applies that budget to global_config, once, at the end of config load.
    //
    // It has to happen here rather than in PatchResolutionTargets, because the display mode
    // is not the only consumer: the swapchain texture-create hook in hooks/nvn.hpp resizes
    // every backbuffer to OutputWidthPx() x OutputHeightPx() straight out of global_config at
    // runtime. A clamp local to the patch would program a safe display mode and then have the
    // hook allocate the oversized swapchain anyway.
    void ApplyResolutionHackBudget();

}  // namespace d3
