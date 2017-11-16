#pragma once

#include <spdlog/spdlog.h>
#include <sstream>

#define SPD_AUTO_BUILD_STRING(...) spdlog::detail::utilities::make_string{} << __VA_ARGS__ >> spdlog::detail::utilities::make_string::to_string

#define SPD_AUTO_LOG(level, ...)                               \
spdlog::apply_all([&](std::shared_ptr<spdlog::logger> lgr)  \
{                                                           \
    std::string message;                                    \
    if (lgr->should_log(level))                             \
    {                                                       \
        if (message.empty())                                \
        {                                                   \
            message.assign(SPD_AUTO_BUILD_STRING(__VA_ARGS__)); \
        }                                                       \
        lgr->log(level, message.c_str());                   \
    }                                                       \
});

#define SPD_AUTO_LOG_FMT(level, msg, ...)                      \
spdlog::apply_all([&](std::shared_ptr<spdlog::logger> lgr)  \
{                                                           \
    if (lgr->should_log(level))                             \
    {                                                       \
        lgr->log(level, msg, __VA_ARGS__);                  \
    }                                                       \
});

#define SPD_AUTO_TRACE(...) SPD_AUTO_LOG(spdlog::level::trace, __VA_ARGS__)
#define SPD_AUTO_DEBUG(...) SPD_AUTO_LOG(spdlog::level::debug, __VA_ARGS__)
#define SPD_AUTO_INFO(...) SPD_AUTO_LOG(spdlog::level::info, __VA_ARGS__)
#define SPD_AUTO_WARN(...) SPD_AUTO_LOG(spdlog::level::warn, __VA_ARGS__)
#define SPD_AUTO_ERROR(...) SPD_AUTO_LOG(spdlog::level::err, __VA_ARGS__)
#define SPD_AUTO_CRITICAL(...) SPD_AUTO_LOG(spdlog::level::critical, __VA_ARGS__)

#define SPD_AUTO_TRACE_FMT(msg, ...) SPD_AUTO_LOG_FMT(spdlog::level::trace, msg, __VA_ARGS__)
#define SPD_AUTO_DEBUG_FMT(msg, ...) SPD_AUTO_LOG_FMT(spdlog::level::debug, msg, __VA_ARGS__)
#define SPD_AUTO_INFO_FMT(msg, ...) SPD_AUTO_LOG_FMT(spdlog::level::info, msg, __VA_ARGS__)
#define SPD_AUTO_WARN_FMT(msg, ...) SPD_AUTO_LOG_FMT(spdlog::level::warn, msg, __VA_ARGS__)
#define SPD_AUTO_ERROR_FMT(msg, ...) SPD_AUTO_LOG_FMT(spdlog::level::err, msg, __VA_ARGS__)
#define SPD_AUTO_CRITICAL_FMT(msg, ...) SPD_AUTO_LOG_FMT(spdlog::level::critical, msg, __VA_ARGS__)
