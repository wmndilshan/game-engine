#pragma once

#include <string>

// miniaudio single-header lives in /include/miniaudio.h
#include "miniaudio.h"

namespace engine {

class AudioSystem
{
public:
    AudioSystem()  = default;
    ~AudioSystem() = default;

    void init();
    void shutdown();

    bool playSound(const std::string& filepath);

private:
    ma_engine m_engine{};
    bool      m_initialized = false;
};

} // namespace engine
