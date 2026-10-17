#pragma once

#include <windows.h>

#include "patch_result.h"  // Result, ToString

namespace LogoCut
{
    using Result = Patch::Result;

    // The Old Blood only, and only after IntroSkip::Apply has made com_skipIntroVideo 1. exe:
    // WolfOldBlood_x64.exe in memory. Checks that it is one of the analysed builds, then hooks the
    // resource-block load just after it starts a loading video: when that video is the start-up
    // logo, it is stopped at once, so its textures exist but no frame of it is shown
    // (logo_cut.cpp). Patched = hooked. Any other build, a site that already jumps elsewhere, or
    // anything unexpected: no hook (the logo then shows while "common" loads), and the log says
    // why. The hook stays for the life of the process.
    Result Install(HMODULE exe);
}
