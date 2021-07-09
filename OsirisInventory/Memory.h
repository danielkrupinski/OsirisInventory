#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "SDK/Platform.h"

class ClientMode;
template <typename T> class ClientSharedObjectCache;
class CSPlayerInventory;
class EconItem;
class EconItemAttributeDefinition;
class EconItemView;
class Entity;
class GameEventDescriptor;
class GameEventManager;
class Input;
class ItemSystem;
class InventoryManager;
class KeyValues;
class MoveHelper;
class MoveData;
class PanoramaMarshallHelper;
class PlantedC4;
class PlayerResource;
template <typename T> class SharedObjectTypeCache;
class ViewRender;
class ViewRenderBeams;
class WeaponSystem;
template <typename Key, typename Value>
struct UtlMap;
template <typename T>
class UtlVector;

struct ActiveChannels;
struct Channel;
struct GlobalVars;
struct GlowObjectManager;
struct PanoramaEventRegistration;
struct Vector;

class Memory {
public:
    Memory() noexcept;

#ifdef _WIN32
    std::uintptr_t present;
    std::uintptr_t reset;
#else
    std::uintptr_t pollEvent;
    std::uintptr_t swapWindow;
#endif

    GlobalVars* globalVars;
    UtlMap<short, PanoramaEventRegistration>* registeredPanoramaEvents;
    std::uintptr_t hud;
    int*(__THISCALL* findHudElement)(std::uintptr_t, const char*);
    int(__THISCALL* clearHudWeapon)(int*, int);
    std::add_pointer_t<ItemSystem* __CDECL()> itemSystem;
    std::add_pointer_t<void __CDECL(const char* msg, ...)> debugMsg;
    std::add_pointer_t<void __CDECL(const std::array<std::uint8_t, 4>& color, const char* msg, ...)> conColorMsg;
    int(__THISCALL* equipWearable)(void* wearable, void* player);
    WeaponSystem* weaponSystem;
    GameEventDescriptor* (__THISCALL* getEventDescriptor)(GameEventManager* _this, const char* name, int* cookie);
    PlayerResource** playerResource;
    InventoryManager* inventoryManager;
    std::add_pointer_t<EconItem* __STDCALL()> createEconItemSharedObject;
    bool(__THISCALL* addEconItem)(CSPlayerInventory* _this, EconItem* item, bool updateAckFile, bool writeAckFile, bool checkForNewItems);
    void(__THISCALL* clearInventoryImageRGBA)(void* itemView);
    PanoramaMarshallHelper* panoramaMarshallHelper;
    std::uintptr_t setStickerToolSlotGetArgAsNumberReturnAddress;
    std::uintptr_t setStickerToolSlotGetArgAsStringReturnAddress;
    std::uintptr_t wearItemStickerGetArgAsNumberReturnAddress;
    std::uintptr_t wearItemStickerGetArgAsStringReturnAddress;
    std::uintptr_t setNameToolStringGetArgAsStringReturnAddress;
    std::uintptr_t clearCustomNameGetArgAsStringReturnAddress;
    std::uintptr_t deleteItemGetArgAsStringReturnAddress;
    std::uintptr_t setStatTrakSwapToolItemsGetArgAsStringReturnAddress1;
    std::uintptr_t setStatTrakSwapToolItemsGetArgAsStringReturnAddress2;
    std::uintptr_t acknowledgeNewItemByItemIDGetArgAsStringReturnAddress;

    std::add_pointer_t<EconItemView* __CDECL(std::uint64_t itemID)> findOrCreateEconItemViewForItemID;
    void*(__THISCALL* getInventoryItemByItemID)(CSPlayerInventory* _this, std::uint64_t itemID);
    std::uintptr_t useToolGetArgAsStringReturnAddress;
    std::uintptr_t useToolGetArg2AsStringReturnAddress;
    EconItem*(__THISCALL* getSOCData)(void* itemView);
    void(__THISCALL* setCustomName)(EconItem* _this, const char* name);
    SharedObjectTypeCache<EconItem>*(__THISCALL* createBaseTypeCache)(ClientSharedObjectCache<EconItem>* _this, int classID);
    void** uiComponentInventory;
    void(__THISCALL* setItemSessionPropertyValue)(void* _this, std::uint64_t itemID, const char* type, const char* value);

    short makePanoramaSymbol(const char* name) const noexcept
    {
        short symbol;
        makePanoramaSymbolFn(&symbol, name);
        return symbol;
    }

    void setDynamicAttributeValue(EconItem* _this, EconItemAttributeDefinition* attribute, void* value) const noexcept
    {
#ifdef _WIN32
        reinterpret_cast<void(__thiscall*)(EconItem*, EconItemAttributeDefinition*, void*)>(setDynamicAttributeValueFn)(_this, attribute, value);
#else
        reinterpret_cast<void(*)(void*, EconItem*, EconItemAttributeDefinition*, void*)>(setDynamicAttributeValueFn)(nullptr, _this, attribute, value);
#endif
    }

private:
    void(__THISCALL* makePanoramaSymbolFn)(short* symbol, const char* name);

    std::uintptr_t setDynamicAttributeValueFn;
};

inline std::unique_ptr<const Memory> memory;
