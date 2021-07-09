#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#elif __linux__
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "Interfaces.h"
#include "Memory.h"
#include "SDK/LocalPlayer.h"

template <typename T>
static constexpr auto relativeToAbsolute(uintptr_t address) noexcept
{
    return (T)(address + 4 + *reinterpret_cast<std::int32_t*>(address));
}

static std::pair<void*, std::size_t> getModuleInformation(const char* name) noexcept
{
#ifdef _WIN32
    if (HMODULE handle = GetModuleHandleA(name)) {
        if (MODULEINFO moduleInfo; GetModuleInformation(GetCurrentProcess(), handle, &moduleInfo, sizeof(moduleInfo)))
            return std::make_pair(moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage);
    }
    return {};
#elif __linux__
    struct ModuleInfo {
        const char* name;
        void* base = nullptr;
        std::size_t size = 0;
    } moduleInfo;

    moduleInfo.name = name;

    dl_iterate_phdr([](struct dl_phdr_info* info, std::size_t, void* data) {
        const auto moduleInfo = reinterpret_cast<ModuleInfo*>(data);
        if (!std::string_view{ info->dlpi_name }.ends_with(moduleInfo->name))
            return 0;

        if (const auto fd = open(info->dlpi_name, O_RDONLY); fd >= 0) {
            if (struct stat st; fstat(fd, &st) == 0) {
                if (const auto map = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0); map != MAP_FAILED) {
                    const auto ehdr = (ElfW(Ehdr)*)map;
                    const auto shdrs = (ElfW(Shdr)*)(std::uintptr_t(ehdr) + ehdr->e_shoff);
                    const auto strTab = (const char*)(std::uintptr_t(ehdr) + shdrs[ehdr->e_shstrndx].sh_offset);

                    for (auto i = 0; i < ehdr->e_shnum; ++i) {
                        const auto shdr = (ElfW(Shdr)*)(std::uintptr_t(shdrs) + i * ehdr->e_shentsize);

                        if (std::strcmp(strTab + shdr->sh_name, ".text") != 0)
                            continue;

                        moduleInfo->base = (void*)(info->dlpi_addr + shdr->sh_offset);
                        moduleInfo->size = shdr->sh_size;
                        munmap(map, st.st_size);
                        close(fd);
                        return 1;
                    }
                    munmap(map, st.st_size);
                }
            }
            close(fd);
        }

        moduleInfo->base = (void*)(info->dlpi_addr + info->dlpi_phdr[0].p_vaddr);
        moduleInfo->size = info->dlpi_phdr[0].p_memsz;
        return 1;
    }, &moduleInfo);

    return std::make_pair(moduleInfo.base, moduleInfo.size);
#endif
}

[[nodiscard]] static auto generateBadCharTable(std::string_view pattern) noexcept
{
    assert(!pattern.empty());

    std::array<std::size_t, (std::numeric_limits<std::uint8_t>::max)() + 1> table;

    auto lastWildcard = pattern.rfind('?');
    if (lastWildcard == std::string_view::npos)
        lastWildcard = 0;

    const auto defaultShift = (std::max)(std::size_t(1), pattern.length() - 1 - lastWildcard);
    table.fill(defaultShift);

    for (auto i = lastWildcard; i < pattern.length() - 1; ++i)
        table[static_cast<std::uint8_t>(pattern[i])] = pattern.length() - 1 - i;

    return table;
}

static std::uintptr_t findPattern(const char* moduleName, std::string_view pattern, bool reportNotFound = true) noexcept
{
    static auto id = 0;
    ++id;

    const auto [moduleBase, moduleSize] = getModuleInformation(moduleName);

    if (moduleBase && moduleSize) {
        const auto lastIdx = pattern.length() - 1;
        const auto badCharTable = generateBadCharTable(pattern);

        auto start = static_cast<const char*>(moduleBase);
        const auto end = start + moduleSize - pattern.length();

        while (start <= end) {
            int i = lastIdx;
            while (i >= 0 && (pattern[i] == '?' || start[i] == pattern[i]))
                --i;

            if (i < 0)
                return reinterpret_cast<std::uintptr_t>(start);

            start += badCharTable[static_cast<std::uint8_t>(start[lastIdx])];
        }
    }

    assert(false);
#ifdef _WIN32
    if (reportNotFound)
        MessageBoxA(nullptr, ("Failed to find pattern #" + std::to_string(id) + '!').c_str(), "Osiris", MB_OK | MB_ICONWARNING);
#endif
    return 0;
}

Memory::Memory() noexcept
{
#ifdef _WIN32
    present = findPattern("gameoverlayrenderer", "\xFF\x15????\x8B\xF0\x85\xFF") + 2;
    reset = findPattern("gameoverlayrenderer", "\xC7\x45?????\xFF\x15????\x8B\xD8") + 9;

    globalVars = **reinterpret_cast<GlobalVars***>((*reinterpret_cast<uintptr_t**>(interfaces->client))[11] + 10);
    auto temp = reinterpret_cast<std::uintptr_t*>(findPattern(CLIENT_DLL, "\xB9????\xE8????\x8B\x5D\x08") + 1);
    hud = *temp;
    findHudElement = relativeToAbsolute<decltype(findHudElement)>(reinterpret_cast<uintptr_t>(temp) + 5);
    clearHudWeapon = reinterpret_cast<decltype(clearHudWeapon)>(findPattern(CLIENT_DLL, "\x55\x8B\xEC\x51\x53\x56\x8B\x75\x08\x8B\xD9\x57\x6B\xFE\x2C"));
    itemSystem = relativeToAbsolute<decltype(itemSystem)>(findPattern(CLIENT_DLL, "\xE8????\x0F\xB7\x0F") + 1);
    const auto tier0 = GetModuleHandleW(L"tier0");
    debugMsg = reinterpret_cast<decltype(debugMsg)>(GetProcAddress(tier0, "Msg"));
    conColorMsg = reinterpret_cast<decltype(conColorMsg)>(GetProcAddress(tier0, "?ConColorMsg@@YAXABVColor@@PBDZZ"));
    equipWearable = reinterpret_cast<decltype(equipWearable)>(findPattern(CLIENT_DLL, "\x55\x8B\xEC\x83\xEC\x10\x53\x8B\x5D\x08\x57\x8B\xF9"));
    weaponSystem = *reinterpret_cast<WeaponSystem**>(findPattern(CLIENT_DLL, "\x8B\x35????\xFF\x10\x0F\xB7\xC0") + 2);
    getEventDescriptor = relativeToAbsolute<decltype(getEventDescriptor)>(findPattern(ENGINE_DLL, "\xE8????\x8B\xD8\x85\xDB\x75\x27") + 1);
    playerResource = *reinterpret_cast<PlayerResource***>(findPattern(CLIENT_DLL, "\x74\x30\x8B\x35????\x85\xF6") + 4);
    registeredPanoramaEvents = reinterpret_cast<decltype(registeredPanoramaEvents)>(*reinterpret_cast<std::uintptr_t*>(findPattern(CLIENT_DLL, "\xE8????\xA1????\xA8\x01\x75\x21") + 6) - 36);
    makePanoramaSymbolFn = relativeToAbsolute<decltype(makePanoramaSymbolFn)>(findPattern(CLIENT_DLL, "\xE8????\x0F\xB7\x45\x0E\x8D\x4D\x0E") + 1);
    inventoryManager = *reinterpret_cast<InventoryManager**>(findPattern(CLIENT_DLL, "\x8D\x44\x24\x28\xB9") + 5);
    createEconItemSharedObject = *reinterpret_cast<decltype(createEconItemSharedObject)*>(findPattern(CLIENT_DLL, "\x55\x8B\xEC\x83\xEC\x1C\x8D\x45\xE4\xC7\x45") + 20);
    addEconItem = relativeToAbsolute<decltype(addEconItem)>(findPattern(CLIENT_DLL, "\xE8????\x84\xC0\x74\xE7") + 1);
    clearInventoryImageRGBA = reinterpret_cast<decltype(clearInventoryImageRGBA)>(findPattern(CLIENT_DLL, "\x55\x8B\xEC\x81\xEC????\x57\x8B\xF9\xC7\x47"));
    panoramaMarshallHelper = *reinterpret_cast<decltype(panoramaMarshallHelper)*>(findPattern(CLIENT_DLL, "\x68????\x8B\xC8\xE8????\x8D\x4D\xF4\xFF\x15????\x8B\xCF\xFF\x15????\x5F\x5E\x8B\xE5\x5D\xC3") + 1);
    setStickerToolSlotGetArgAsNumberReturnAddress = findPattern(CLIENT_DLL, "\xFF\xD2\xDD\x5C\x24\x10\xF2\x0F\x2C\x7C\x24") + 2;
    setStickerToolSlotGetArgAsStringReturnAddress = setStickerToolSlotGetArgAsNumberReturnAddress - 49;
    wearItemStickerGetArgAsNumberReturnAddress = findPattern(CLIENT_DLL, "\xDD\x5C\x24\x18\xF2\x0F\x2C\x7C\x24?\x85\xFF");
    wearItemStickerGetArgAsStringReturnAddress = wearItemStickerGetArgAsNumberReturnAddress - 80;
    setNameToolStringGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\x8B\xF8\xC6\x45\x08?\x33\xC0");
    clearCustomNameGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\xFF\x50\x1C\x8B\xF0\x85\xF6\x74\x21") + 3;
    deleteItemGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\x85\xC0\x74\x22\x51");
    setStatTrakSwapToolItemsGetArgAsStringReturnAddress1 = findPattern(CLIENT_DLL, "\x85\xC0\x74\x7E\x8B\xC8\xE8????\x8B\x37");
    setStatTrakSwapToolItemsGetArgAsStringReturnAddress2 = setStatTrakSwapToolItemsGetArgAsStringReturnAddress1 + 44;
    acknowledgeNewItemByItemIDGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\x85\xC0\x74\x33\x8B\xC8\xE8????\xB9");

    findOrCreateEconItemViewForItemID = relativeToAbsolute<decltype(findOrCreateEconItemViewForItemID)>(findPattern(CLIENT_DLL, "\xE8????\x8B\xCE\x83\xC4\x08") + 1);
    getInventoryItemByItemID = relativeToAbsolute<decltype(getInventoryItemByItemID)>(findPattern(CLIENT_DLL, "\xE8????\x8B\x33\x8B\xD0") + 1);
    useToolGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\x85\xC0\x0F\x84????\x8B\xC8\xE8????\x8B\x37");
    useToolGetArg2AsStringReturnAddress = useToolGetArgAsStringReturnAddress + 52;
    getSOCData = relativeToAbsolute<decltype(getSOCData)>(findPattern(CLIENT_DLL, "\xE8????\x32\xC9") + 1);
    setCustomName = relativeToAbsolute<decltype(setCustomName)>(findPattern(CLIENT_DLL, "\xE8????\x8B\x46\x78\xC1\xE8\x0A\xA8\x01\x74\x13\x8B\x46\x34") + 1);
    setDynamicAttributeValueFn = findPattern(CLIENT_DLL, "\x55\x8B\xEC\x83\xE4\xF8\x83\xEC\x3C\x53\x8B\x5D\x08\x56\x57\x6A");
    createBaseTypeCache = relativeToAbsolute<decltype(createBaseTypeCache)>(findPattern(CLIENT_DLL, "\xE8????\x8D\x4D\x0F") + 1);
    uiComponentInventory = *reinterpret_cast<void***>(findPattern(CLIENT_DLL, "\xC6\x44\x24??\x83\x3D") + 7);
    setItemSessionPropertyValue = relativeToAbsolute<decltype(setItemSessionPropertyValue)>(findPattern(CLIENT_DLL, "\xE8????\x8B\x4C\x24\x2C\x46") + 1);

    localPlayer.init(*reinterpret_cast<Entity***>(findPattern(CLIENT_DLL, "\xA1????\x89\x45\xBC\x85\xC0") + 1));
#else
    const auto tier0 = dlopen(TIER0_DLL, RTLD_NOLOAD | RTLD_NOW);
    debugMsg = decltype(debugMsg)(dlsym(tier0, "Msg"));
    conColorMsg = decltype(conColorMsg)(dlsym(tier0, "_Z11ConColorMsgRK5ColorPKcz"));
    dlclose(tier0);

    const auto libSDL = dlopen("libSDL2-2.0.so.0", RTLD_LAZY | RTLD_NOLOAD);
    pollEvent = relativeToAbsolute<uintptr_t>(uintptr_t(dlsym(libSDL, "SDL_PollEvent")) + 2);
    swapWindow = relativeToAbsolute<uintptr_t>(uintptr_t(dlsym(libSDL, "SDL_GL_SwapWindow")) + 2);
    dlclose(libSDL);

    globalVars = *relativeToAbsolute<GlobalVars**>((*reinterpret_cast<std::uintptr_t**>(interfaces->client))[11] + 16);
    itemSystem = relativeToAbsolute<decltype(itemSystem)>(findPattern(CLIENT_DLL, "\xE8????\x4D\x63\xEC") + 1);
    weaponSystem = *relativeToAbsolute<WeaponSystem**>(findPattern(CLIENT_DLL, "\x48\x8B\x58\x10\x48\x8B\x07\xFF\x10") + 12);
   
    hud = relativeToAbsolute<decltype(hud)>(findPattern(CLIENT_DLL, "\x53\x48\x8D\x3D????\x48\x83\xEC\x10\xE8") + 4);
    findHudElement = relativeToAbsolute<decltype(findHudElement)>(findPattern(CLIENT_DLL, "\xE8????\x48\x8D\x50\xE0") + 1);

    playerResource = relativeToAbsolute<PlayerResource**>(findPattern(CLIENT_DLL, "\x74\x38\x48\x8B\x3D????\x89\xDE") + 5);

    getEventDescriptor = relativeToAbsolute<decltype(getEventDescriptor)>(findPattern(ENGINE_DLL, "\xE8????\x48\x85\xC0\x74\x62") + 1);
  
    clearHudWeapon = relativeToAbsolute<decltype(clearHudWeapon)>(findPattern(CLIENT_DLL, "\xE8????\xC6\x45\xB8\x01") + 1);
    equipWearable = reinterpret_cast<decltype(equipWearable)>(findPattern(CLIENT_DLL, "\x55\x48\x89\xE5\x41\x56\x41\x55\x41\x54\x49\x89\xF4\x53\x48\x89\xFB\x48\x83\xEC\x10\x48\x8B\x07"));

    registeredPanoramaEvents = relativeToAbsolute<decltype(registeredPanoramaEvents)>(relativeToAbsolute<std::uintptr_t>(findPattern(CLIENT_DLL, "\xE8????\x8B\x50\x10\x49\x89\xC6") + 1) + 12);
    makePanoramaSymbolFn = relativeToAbsolute<decltype(makePanoramaSymbolFn)>(findPattern(CLIENT_DLL, "\xE8????\x0F\xB7\x45\xA0\x31\xF6") + 1);

    inventoryManager = relativeToAbsolute<decltype(inventoryManager)>(findPattern(CLIENT_DLL, "\x48\x8D\x35????\x48\x8D\x3D????\xE9????\x90\x90\x90\x55") + 3);
    createEconItemSharedObject = relativeToAbsolute<decltype(createEconItemSharedObject)>(findPattern(CLIENT_DLL, "\x55\x48\x8D\x05????\x31\xD2\x4C\x8D\x0D") + 50);
    addEconItem = relativeToAbsolute<decltype(addEconItem)>(findPattern(CLIENT_DLL, "\xE8????\x45\x3B\x65\x28\x72\xD6") + 1);
    clearInventoryImageRGBA = relativeToAbsolute<decltype(clearInventoryImageRGBA)>(findPattern(CLIENT_DLL, "\xE8????\x83\xC3\x01\x49\x83\xC4\x08\x41\x3B\x5D\x50") + 1);
    panoramaMarshallHelper = relativeToAbsolute<decltype(panoramaMarshallHelper)>(findPattern(CLIENT_DLL, "\xF3\x0F\x11\x05????\x48\x89\x05????\x48\xC7\x05????????\xC7\x05") + 11);
    setStickerToolSlotGetArgAsNumberReturnAddress = findPattern(CLIENT_DLL, "\xF2\x44\x0F\x2C\xF0\x45\x85\xF6\x78\x32");
    setStickerToolSlotGetArgAsStringReturnAddress = setStickerToolSlotGetArgAsNumberReturnAddress - 46;
    findOrCreateEconItemViewForItemID = relativeToAbsolute<decltype(findOrCreateEconItemViewForItemID)>(findPattern(CLIENT_DLL, "\xE8????\x4C\x89\xEF\x48\x89\x45\xC8") + 1);
    getInventoryItemByItemID = relativeToAbsolute<decltype(getInventoryItemByItemID)>(findPattern(CLIENT_DLL, "\xE8????\x45\x84\xED\x49\x89\xC1") + 1);
    getSOCData = relativeToAbsolute<decltype(getSOCData)>(findPattern(CLIENT_DLL, "\xE8????\x5B\x44\x89\xEE") + 1);
    setCustomName = relativeToAbsolute<decltype(setCustomName)>(findPattern(CLIENT_DLL, "\xE8????\x41\x8B\x84\x24????\xE9????\x8B\x98") + 1);
    useToolGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\x48\x85\xC0\x74\xDA\x48\x89\xC7\xE8????\x48\x8B\x0B");
    useToolGetArg2AsStringReturnAddress = useToolGetArgAsStringReturnAddress + 55;
    wearItemStickerGetArgAsNumberReturnAddress = findPattern(CLIENT_DLL, "\xF2\x44\x0F\x2C\xF8\x45\x39\xFE");
    wearItemStickerGetArgAsStringReturnAddress = wearItemStickerGetArgAsNumberReturnAddress - 57;
    setNameToolStringGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\xBA????\x4C\x89\xF6\x48\x89\xC7\x49\x89\xC4");
    clearCustomNameGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\x48\x85\xC0\x74\xE5\x48\x89\xC7\xE8????\x49\x89\xC4");
    deleteItemGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\x48\x85\xC0\x74\xDE\x48\x89\xC7\xE8????\x48\x89\xC3\xE8????\x48\x89\xDE");
    setDynamicAttributeValueFn = findPattern(CLIENT_DLL, "\x41\x8B\x06\x49\x8D\x7D\x08") - 95;
    createBaseTypeCache = relativeToAbsolute<decltype(createBaseTypeCache)>(findPattern(CLIENT_DLL, "\xE8????\x48\x89\xDE\x5B\x48\x8B\x10") + 1);
    uiComponentInventory = relativeToAbsolute<decltype(uiComponentInventory)>(findPattern(CLIENT_DLL, "\xE8????\x4C\x89\x3D????\x4C\x89\xFF\xEB\x9E") + 8);
    setItemSessionPropertyValue = relativeToAbsolute<decltype(setItemSessionPropertyValue)>(findPattern(CLIENT_DLL, "\xE8????\x48\x8B\x85????\x41\x83\xC4\x01") + 1);

    setStatTrakSwapToolItemsGetArgAsStringReturnAddress1 = findPattern(CLIENT_DLL, "\x74\x84\x4C\x89\xEE\x4C\x89\xF7\xE8????\x48\x85\xC0") - 86;
    setStatTrakSwapToolItemsGetArgAsStringReturnAddress2 = setStatTrakSwapToolItemsGetArgAsStringReturnAddress1 + 55;
    acknowledgeNewItemByItemIDGetArgAsStringReturnAddress = findPattern(CLIENT_DLL, "\x48\x89\xC7\xE8????\x4C\x89\xEF\x48\x89\xC6\xE8????\x48\x8B\x0B") - 5;

    localPlayer.init(relativeToAbsolute<Entity**>(findPattern(CLIENT_DLL, "\x83\xFF\xFF\x48\x8B\x05") + 6));
#endif
}
