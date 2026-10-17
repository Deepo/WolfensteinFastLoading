#include "stdafx.h"
#include "logo_cut.h"

#include <safetyhook.hpp>

// ---------------------------------------------------------------------------
// The Old Blood only: with com_skipIntroVideo set (IntroSkip::Apply), the start-up logo is started
// and stopped at once, so no frame of it shows.
//
// Why it must be started at all: The Old Blood's cvar bink_dontfree ("keep bink textures around
// since we need to be able to play videos whenever") defaults to "1"; The New Order's to "0". With
// it, the render system's PlayBinkVideo (0x481810, render vtable slot 13) creates the Bink
// textures (_binkY0-2, _binkCr0-2, _binkCb0-2, at 0x4822e0) for the first video it plays and keeps
// them for every later one; FreeBinkVideo (0x482be0, slot 18) leaves them alone. Init2 (0x1650b0)
// therefore plays the logo while "common" loads even when told to skip it (intro_skip.cpp), and
// textures created then last the whole session. Without the logo (TOBFastLoading 1.0.0 dropped it),
// the first chapter video creates them during a map load, and the next video after a level change
// draws solid green (0,136,0: Y, Cb and Cr all read as zero). A user reported it at the end of
// chapter 1 (2026-10-17); reproduced on Steam, and gone with the logo back.
//
// The resource-block load (0x166780) starts its video here, with the video's idStr in a local:
//
//   0x166b4b  mov   r9d, [rbp+0x48c0]            ; flags (2 for Init2's skip: end with the load)
//   0x166b52  mov   r8,  [rbp-0x70]              ; the video (idStr*: its chars at +8)
//   0x166b56  mov   rcx, [rip+...]               ; renderSystem (0x1789230)
//   0x166b5d  call  [r15+0x68]                   ; PlayBinkVideo
//   0x166b61  mov   rdi, [rbp-0x48]              ; <- hook
//   0x166b65  mov   r14, [rbp-0x38]
//
// When the video is bink/loadvideo_pc_xboxone.bik, which only Init2 names, the hook calls
// renderSystem->FreeBinkVideo right away: the textures were made inside PlayBinkVideo, at the same
// moment as in the unmodified game, and no frame has been drawn yet. Both steps are the game's own
// paths: PlayBinkVideo calls FreeBinkVideo on itself when it rejects a video (0x4822cf), and Init2
// calls it again once "common" is loaded (0x165316). Any other video is left alone; the hook costs
// a pointer read and a string compare per video a load starts.
//
// The Game Pass build has the same code: the call at 0x175fee, the hook at 0x175ff2, the video
// read straight from the function's argument slot ([rbp+0x4898]); PlayBinkVideo at 0x4a2110,
// FreeBinkVideo at 0x4a3470.
// ---------------------------------------------------------------------------

#include <span>

namespace LogoCut
{
    constexpr char kLogo[] = "bink/loadvideo_pc_xboxone.bik";
    constexpr std::size_t kSlotPlayBinkVideo = 13, kSlotFreeBinkVideo = 18;
    constexpr std::size_t kNameChars = 8;  // idStr: the chars' pointer

    // The Old Blood, GOG and Steam (2015-05-08).
    constexpr std::uint32_t kTimeDateStampTOB = 0x554C7C23;
    constexpr std::uintptr_t kSiteTOB = 0x166B4B;
    constexpr std::uint8_t kSiteTOBBytes[] = {
        0x44, 0x8B, 0x8D, 0xC0, 0x48, 0x00, 0x00,  // mov  r9d, [rbp+0x48c0]
        0x4C, 0x8B, 0x45, 0x90,                    // mov  r8,  [rbp-0x70]   (the video)
        0x48, 0x8B, 0x0D, 0xD3, 0x26, 0x62, 0x01,  // mov  rcx, [rip+0x16226d3]  (renderSystem)
        0x41, 0xFF, 0x57, 0x68,                    // call [r15+0x68]  (PlayBinkVideo)
        0x48, 0x8B, 0x7D, 0xB8,                    // mov  rdi, [rbp-0x48]   <- hook
        0x4C, 0x8B, 0x75, 0xC8,                    // mov  r14, [rbp-0x38]
    };
    constexpr std::size_t kHookOffsetTOB = 0x16;

    // The Old Blood, Game Pass (2021-04-14).
    constexpr std::uint32_t kTimeDateStampTOBGP = 0x60770B00;
    constexpr std::uintptr_t kSiteTOBGP = 0x175FD9;
    constexpr std::uint8_t kSiteTOBGPBytes[] = {
        0x44, 0x8B, 0x8D, 0xA0, 0x48, 0x00, 0x00,  // mov  r9d, [rbp+0x48a0]
        0x4C, 0x8B, 0x85, 0x98, 0x48, 0x00, 0x00,  // mov  r8,  [rbp+0x4898]  (the video)
        0x48, 0x8B, 0x0D, 0xB2, 0x8B, 0x06, 0x01,  // mov  rcx, [rip+0x1068bb2]  (renderSystem)
        0x41, 0xFF, 0x57, 0x68,                    // call [r15+0x68]  (PlayBinkVideo)
        0x48, 0x8B, 0x7D, 0xA8,                    // mov  rdi, [rbp-0x58]   <- hook
        0x48, 0x8B, 0x5D, 0xF0,                    // mov  rbx, [rbp-0x10]
    };
    constexpr std::size_t kHookOffsetTOBGP = 0x19;

    struct Build
    {
        std::uint32_t stamp;
        std::uintptr_t site;
        std::span<const std::uint8_t> siteBytes;
        std::size_t hookOffset;
        std::ptrdiff_t videoSlot;  // rbp + this holds the video's idStr*
        std::uintptr_t renderSystemPtr, renderSystemObj, renderSystemVtbl;
        std::uintptr_t playBinkVideo, freeBinkVideo;
    };
    constexpr Build kBuilds[] = {
        {kTimeDateStampTOB, kSiteTOB, kSiteTOBBytes, kHookOffsetTOB, -0x70, 0x1789230, 0x1F20440, 0xEC8448, 0x481810,
         0x482BE0},
        {kTimeDateStampTOBGP, kSiteTOBGP, kSiteTOBGPBytes, kHookOffsetTOBGP, 0x4898, 0x11DEBA0, 0x1C98460, 0xD1C178,
         0x4A2110, 0x4A3470},
    };

    static std::uint8_t* gBase = nullptr;
    static const Build* gBuild = nullptr;
    static bool gWarnedObject = false;

    // Runs on the loading thread right after PlayBinkVideo returns, for every video a load starts.
    static void OnVideoStarted(SafetyHookContext& ctx)
    {
        const auto* video = *reinterpret_cast<const std::uint8_t* const*>(ctx.rbp + gBuild->videoSlot);
        const char* name = video ? *reinterpret_cast<const char* const*>(video + kNameChars) : nullptr;
        if (!name || std::strcmp(name, kLogo) != 0) return;

        auto* rs = *reinterpret_cast<std::uint8_t**>(gBase + gBuild->renderSystemPtr);
        if (!rs || *reinterpret_cast<std::uint8_t**>(rs) != gBase + gBuild->renderSystemVtbl) {
            if (!gWarnedObject) {
                LOG_WARN("Logo video: the render system at {} is not the expected object: the logo plays.",
                         static_cast<void*>(rs));
                gWarnedObject = true;
            }
            return;
        }
        auto** vtbl = *reinterpret_cast<void***>(rs);
        reinterpret_cast<void (*)(void*)>(vtbl[kSlotFreeBinkVideo])(rs);
        LOG_INFO("Logo video: started (its textures now exist, as in the unmodified game) and stopped at once: no frame "
                 "of it shown.");
    }

    static std::string Hex(const std::uint8_t* p, std::size_t n)
    {
        std::string s;
        for (std::size_t i = 0; i < n; ++i) s += std::format("{:02x}", p[i]);
        return s;
    }

    Result Install(HMODULE exe)
    {
        auto* base = reinterpret_cast<std::uint8_t*>(exe);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        const Build* b = nullptr;
        for (const Build& candidate : kBuilds)
            if (candidate.stamp == nt->FileHeader.TimeDateStamp) b = &candidate;
        if (!b) {
            LOG_WARN("Logo video: not the analysed build (PE timestamp 0x{:08x}): no hook.", nt->FileHeader.TimeDateStamp);
            return Result::UnknownBuild;
        }
        const std::uintptr_t hookAt = b->site + b->hookOffset;
        if (base[hookAt] == 0xE9) {
            LOG_WARN("Logo video: 0x{:x} already jumps elsewhere (a second copy of the plugin, or another mod): no hook.",
                     hookAt);
            return Result::UnknownBuild;
        }
        if (std::memcmp(base + b->site, b->siteBytes.data(), b->siteBytes.size()) != 0) {
            LOG_WARN("Logo video: the call that starts a loading video at 0x{:x} differs from the analysed build ({}): "
                     "no hook.", b->site, Hex(base + b->site, b->siteBytes.size()));
            return Result::UnknownBuild;
        }
        // Static data, already relocated: the renderSystem pointer and the two vtable slots.
        auto* rs = *reinterpret_cast<std::uint8_t**>(base + b->renderSystemPtr);
        auto** vtbl = reinterpret_cast<std::uint8_t**>(base + b->renderSystemVtbl);
        if (rs != base + b->renderSystemObj || vtbl[kSlotPlayBinkVideo] != base + b->playBinkVideo ||
            vtbl[kSlotFreeBinkVideo] != base + b->freeBinkVideo) {
            LOG_WARN("Logo video: the render system pointer or vtable differs from the analysed build: no hook.");
            return Result::UnknownBuild;
        }

        gBase = base;
        gBuild = b;
        auto hook = safetyhook::MidHook::create(base + hookAt, OnVideoStarted);
        if (!hook) {
            gBase = nullptr;
            gBuild = nullptr;
            LOG_ERROR("Logo video: hook at 0x{:x} failed (safetyhook error {}): nothing changed.", hookAt,
                      static_cast<int>(hook.error().type));
            return Result::Failed;
        }
        // Never unhooked: it lives as long as the game. Leaked on purpose, so nothing of this
        // plugin runs or rewrites game code during the game's shutdown.
        new SafetyHookMid(std::move(*hook));
        LOG_INFO("Hooked 0x{:x}: the start-up logo is stopped as soon as it starts (its textures are made, no frame "
                 "is shown).", hookAt);
        return Result::Patched;
    }
}
