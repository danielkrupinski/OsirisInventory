#pragma once

#include <numbers>
#include <string>
#include <vector>

#include "imgui/imgui.h"

#include "SDK/WeaponId.h"

namespace Helpers
{
    ImWchar* getFontGlyphRanges() noexcept;

    std::wstring toWideString(const std::string& str) noexcept;
    std::wstring toUpper(std::wstring str) noexcept;

    bool decodeVFONT(std::vector<char>& buffer) noexcept;
    std::vector<char> loadBinaryFile(const std::string& path) noexcept;

    [[nodiscard]] std::size_t calculateVmtLength(const std::uintptr_t* vmt) noexcept;

    constexpr auto isKnife(WeaponId id) noexcept
    {
        return (id >= WeaponId::Bayonet && id <= WeaponId::SkeletonKnife) || id == WeaponId::KnifeT || id == WeaponId::Knife;
    }

    float random(float min, float max) noexcept;
    int random(int min, int max) noexcept;
}
