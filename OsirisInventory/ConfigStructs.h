#pragma once

#include <array>
#include <sstream>
#include <string>

#include "nlohmann/json.hpp"
#include "JsonForward.h"

enum class WeaponId : short;

using value_t = json::value_t;

// WRITE macro requires:
// - json object named 'j'
// - object holding default values named 'dummy'
// - object to write to json named 'o'
#define WRITE(name, valueName) to_json(j[name], o.valueName, dummy.valueName)

template <typename T>
static void to_json(json& j, const T& o, const T& dummy)
{
    if (o != dummy)
        j = o;
}

template <value_t Type, typename T>
static typename std::enable_if_t<!std::is_same_v<T, bool>> read(const json& j, const char* key, T& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.type() == Type)
        val.get_to(o);
}

void read(const json& j, const char* key, bool& o) noexcept;
void read(const json& j, const char* key, float& o) noexcept;
void read(const json& j, const char* key, int& o) noexcept;
void read(const json& j, const char* key, WeaponId& o) noexcept;
