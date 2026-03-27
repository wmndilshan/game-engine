// miniaudio requires the implementation macro to be defined before the first
// include in *exactly one* translation unit.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "AudioSystem.h"

#include "../core/Log.h"

#include <string>

namespace engine {

void AudioSystem::init()
{
    if (m_initialized)
        return;

    const ma_result result = ma_engine_init(NULL, &m_engine);
    if (result != MA_SUCCESS)
    {
        GE_ERROR(std::string("[AudioSystem] ma_engine_init failed (") + std::to_string(static_cast<int>(result)) + ")");
        m_initialized = false;
        return;
    }

    m_initialized = true;
    GE_INFO("[AudioSystem] Initialized.");
}

void AudioSystem::shutdown()
{
    if (!m_initialized)
        return;

    ma_engine_uninit(&m_engine);
    m_initialized = false;
    GE_INFO("[AudioSystem] Shutdown.");
}

bool AudioSystem::playSound(const std::string& filepath)
{
    if (!m_initialized)
        return false;

    const ma_result result = ma_engine_play_sound(&m_engine, filepath.c_str(), NULL);
    if (result != MA_SUCCESS)
    {
        GE_WARN(std::string("[AudioSystem] Failed to play '") + filepath + "' (" + std::to_string(static_cast<int>(result)) + ")");
        return false;
    }

    GE_INFO(std::string("[AudioSystem] Playing '") + filepath + "'");

    return true;
}

} // namespace engine
