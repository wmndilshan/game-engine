#pragma once

#include <sstream>
#include <memory>
#include <string_view>
#include <string>
#include <vector>

#include <utility>

namespace engine {

namespace detail {

inline void AppendEscapedBraces(std::string& out, std::string_view text)
{
    out.append(text.data(), text.size());
}

template <typename T>
inline std::string ToString(T&& value)
{
    std::ostringstream oss;
    oss << std::forward<T>(value);
    return oss.str();
}

inline std::string ToString(const char* value)
{
    return value ? std::string(value) : std::string("(null)");
}

inline std::string ToString(char* value)
{
    return value ? std::string(value) : std::string("(null)");
}

inline std::string ToString(const std::string& value)
{
    return value;
}

inline std::string ToString(std::string_view value)
{
    return std::string(value);
}

inline void ReplaceNextPlaceholder(std::string& out, std::string_view fmt, size_t& pos, std::string_view replacement)
{
    const size_t n = fmt.size();

    while (pos < n)
    {
        const char c = fmt[pos];

        // Escaped braces
        if (c == '{' && pos + 1 < n && fmt[pos + 1] == '{')
        {
            out.push_back('{');
            pos += 2;
            continue;
        }
        if (c == '}' && pos + 1 < n && fmt[pos + 1] == '}')
        {
            out.push_back('}');
            pos += 2;
            continue;
        }

        if (c == '{')
        {
            // Support {} and also ignore format specs like {:.1f}
            size_t end = pos + 1;
            while (end < n && fmt[end] != '}')
                ++end;

            if (end < n && fmt[end] == '}')
            {
                out.append(replacement.data(), replacement.size());
                pos = end + 1;
                return;
            }
        }

        out.push_back(c);
        ++pos;
    }
}

template <typename... Args>
inline std::string Format(std::string_view fmt, Args&&... args)
{
    std::string out;
    out.reserve(fmt.size() + 32);

    size_t pos = 0;
    (ReplaceNextPlaceholder(out, fmt, pos, ToString(std::forward<Args>(args))), ...);

    if (pos < fmt.size())
        out.append(fmt.substr(pos).data(), fmt.size() - pos);

    return out;
}

} // namespace detail

class Log
{
public:
    enum class Level
    {
        Trace,
        Info,
        Warn,
        Error,
        Critical
    };

    static void Init();
    static void Shutdown();

    static bool IsInitialized();

    static void Trace(std::string_view message);
    static void Info(std::string_view message);
    static void Warn(std::string_view message);
    static void Error(std::string_view message);
    static void Critical(std::string_view message);

    template <typename... Args>
    static void Trace(std::string_view fmt, Args&&... args)
    {
        Trace(detail::Format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Info(std::string_view fmt, Args&&... args)
    {
        Info(detail::Format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Warn(std::string_view fmt, Args&&... args)
    {
        Warn(detail::Format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Error(std::string_view fmt, Args&&... args)
    {
        Error(detail::Format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Critical(std::string_view fmt, Args&&... args)
    {
        Critical(detail::Format(fmt, std::forward<Args>(args)...));
    }

    // UI console support
    static void ClearInMemory();
    static std::vector<std::string> GetInMemoryLines();
};

} // namespace engine

#define GE_TRACE(...)    ::engine::Log::Trace(__VA_ARGS__)
#define GE_INFO(...)     ::engine::Log::Info(__VA_ARGS__)
#define GE_WARN(...)     ::engine::Log::Warn(__VA_ARGS__)
#define GE_ERROR(...)    ::engine::Log::Error(__VA_ARGS__)
#define GE_CRITICAL(...) ::engine::Log::Critical(__VA_ARGS__)
