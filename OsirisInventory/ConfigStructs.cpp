#include "ConfigStructs.h"

void read(const json& j, const char* key, bool& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.type() == value_t::boolean)
        val.get_to(o);
}

void read(const json& j, const char* key, float& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.type() == value_t::number_float)
        val.get_to(o);
}

void read(const json& j, const char* key, int& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.is_number_integer())
        val.get_to(o);
}

void read(const json& j, const char* key, WeaponId& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.is_number_integer())
        val.get_to(o);
}

void read(const json& j, const char* key, char* o, std::size_t size) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.is_string()) {
        std::strncpy(o, val.get<std::string>().c_str(), size);
        o[size - 1] = '\0';
    }
}
