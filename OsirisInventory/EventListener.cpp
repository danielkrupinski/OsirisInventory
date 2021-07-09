#include <cassert>
#include <utility>

#include "EventListener.h"
#include "fnv.h"
#include "InventoryChanger/InventoryChanger.h"
#include "Interfaces.h"
#include "Memory.h"
#include "SDK/GameEvent.h"
#include "SDK/UtlVector.h"

namespace
{
    class EventListenerImpl : public GameEventListener {
    public:
        void fireGameEvent(GameEvent* event) override
        {
            switch (fnv::hashRuntime(event->getName())) {
            case fnv::hash("player_death"):
                InventoryChanger::updateStatTrak(*event);
                InventoryChanger::overrideHudIcon(*event);
                break;
            case fnv::hash("round_mvp"):
                InventoryChanger::onRoundMVP(*event);
                break;
            }
        }

        static EventListenerImpl& instance() noexcept
        {
            static EventListenerImpl impl;
            return impl;
        }
    };
}

void EventListener::init() noexcept
{
    assert(interfaces);

    const auto gameEventManager = interfaces->gameEventManager;
    gameEventManager->addListener(&EventListenerImpl::instance(), "player_death");
    gameEventManager->addListener(&EventListenerImpl::instance(), "round_mvp");

    // Move our player_death listener to the first position to override killfeed icons (InventoryChanger::overrideHudIcon()) before HUD gets them
    if (const auto desc = memory->getEventDescriptor(gameEventManager, "player_death", nullptr))
        std::swap(desc->listeners[0], desc->listeners[desc->listeners.size - 1]);
    else
        assert(false);

    // Move our round_mvp listener to the first position to override event data (InventoryChanger::onRoundMVP()) before HUD gets them
    if (const auto desc = memory->getEventDescriptor(gameEventManager, "round_mvp", nullptr))
        std::swap(desc->listeners[0], desc->listeners[desc->listeners.size - 1]);
    else
        assert(false);
}

void EventListener::remove() noexcept
{
    assert(interfaces);

    interfaces->gameEventManager->removeListener(&EventListenerImpl::instance());
}
