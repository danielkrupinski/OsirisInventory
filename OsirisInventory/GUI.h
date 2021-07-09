#pragma once

#include <memory>

class GUI {
public:
    GUI() noexcept;
    void render() noexcept;
    void handleToggle() noexcept;
    bool isOpen() const noexcept { return open; }
private:
    bool open = true;

    void renderConfigWindow() noexcept;
    void renderGuiStyle2() noexcept;

    float timeToNextConfigRefresh = 0.1f;
};

inline std::unique_ptr<GUI> gui;
