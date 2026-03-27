#include "Log.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>

namespace engine {
namespace {

static std::mutex s_mutex;
static std::ofstream s_file;
static std::vector<std::string> s_lines;
static bool s_initialized = false;
static constexpr size_t kMaxLines = 2000;

static const char* levelToString(Log::Level level)
{
    switch (level)
    {
        case Log::Level::Trace: return "TRACE";
        case Log::Level::Info: return "INFO";
        case Log::Level::Warn: return "WARN";
        case Log::Level::Error: return "ERROR";
        case Log::Level::Critical: return "CRITICAL";
        default: return "INFO";
    }
}

static std::string timeNowHHMMSS()
{
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t t = clock::to_time_t(now);

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    char buf[16] = {};
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

static void pushLineLocked(std::string line)
{
    s_lines.emplace_back(std::move(line));
    if (s_lines.size() > kMaxLines)
    {
        const size_t overflow = s_lines.size() - kMaxLines;
        s_lines.erase(s_lines.begin(), s_lines.begin() + static_cast<std::ptrdiff_t>(overflow));
    }
}

static void writeLocked(Log::Level level, std::string_view message)
{
    std::string line;
    line.reserve(message.size() + 32);
    line += "[";
    line += timeNowHHMMSS();
    line += "] [";
    line += levelToString(level);
    line += "] ";
    line.append(message.data(), message.size());

    // In-memory (UI console)
    pushLineLocked(line);

    // Stdout/stderr
    FILE* out = (level == Log::Level::Error || level == Log::Level::Critical) ? stderr : stdout;
    std::fprintf(out, "%s\n", line.c_str());
    std::fflush(out);

    // File
    if (s_file.is_open())
    {
        s_file << line << '\n';
        if (level == Log::Level::Warn || level == Log::Level::Error || level == Log::Level::Critical)
            s_file.flush();
    }
}

} // namespace

void Log::Init()
{
    if (s_initialized)
        return;

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_lines.clear();
        s_file.open("Engine3D.log", std::ios::out | std::ios::trunc);
        s_initialized = true;
        writeLocked(Level::Info, "Logger initialized");
    }
}

void Log::Shutdown()
{
    if (!s_initialized)
        return;

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_file.is_open())
            s_file.close();
        s_lines.clear();
    }
    s_initialized = false;
}

bool Log::IsInitialized()
{
    return s_initialized;
}

void Log::Trace(std::string_view message)
{
    if (!s_initialized) Init();
    std::lock_guard<std::mutex> lock(s_mutex);
    writeLocked(Level::Trace, message);
}

void Log::Info(std::string_view message)
{
    if (!s_initialized) Init();
    std::lock_guard<std::mutex> lock(s_mutex);
    writeLocked(Level::Info, message);
}

void Log::Warn(std::string_view message)
{
    if (!s_initialized) Init();
    std::lock_guard<std::mutex> lock(s_mutex);
    writeLocked(Level::Warn, message);
}

void Log::Error(std::string_view message)
{
    if (!s_initialized) Init();
    std::lock_guard<std::mutex> lock(s_mutex);
    writeLocked(Level::Error, message);
}

void Log::Critical(std::string_view message)
{
    if (!s_initialized) Init();
    std::lock_guard<std::mutex> lock(s_mutex);
    writeLocked(Level::Critical, message);
}

void Log::ClearInMemory()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_lines.clear();
}

std::vector<std::string> Log::GetInMemoryLines()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_lines;
}

} // namespace engine
