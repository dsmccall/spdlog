#pragma once

#include "spdlog/configuration.h"
#include "spdlog/spdlog.h"

// Extra sinks that aren't included in spdlog_impl
#include "spdlog/sinks/msvc_sink.h"
#include "spdlog/sinks/null_sink.h"
// End extra sinks

#include <set>
#include <sstream>

namespace spdlog
{
    namespace detail
    {
        namespace utilities
        {
            class make_string
            {
            public:
                /// convert automatically to be a string
                operator std::string() const
                {
                    return m_stream.str();
                }

                /// allow any class to pipe
                template<class T> inline
                make_string& operator<<(const T& value)
                {
                    m_stream << value;
                    return *this;
                }

                /// Explicitly convert to a string
                enum to_string_enum
                {
                    to_string
                };

                std::string operator >> (to_string_enum)
                {
                    return m_stream.str();
                }

            private:
                std::stringstream m_stream;
            };

            // Parse a CSV into a vector of components.  Commas within a quotes string are ignored
            inline std::vector<std::string> parse_csv(const std::string& csv, size_t max = std::numeric_limits<size_t>::max())
            {
                std::vector<std::string> result;

                bool in_quote(false);

                std::string token;
                auto current = csv.begin();
                auto count = size_t{ 0 };

                while (current != csv.end() && count < max)
                {
                    switch (*current)
                    {
                    case('"'):
                        if (in_quote)
                        {
                            // Get an iterator to the NEXT character in the string.
                            auto next = current + 1;

                            // Have a look at the next character - this must be either a double quote, or a comma.
                            // Anything else is incorrect.
                            if (next != csv.end())
                            {
                                if (*next == '"')
                                {
                                    // Double quotes are to be converted to a single quote, and added to the token
                                    token += '"';
                                    ++current;
                                    break;
                                }
                                else if (*next == ',')
                                {
                                    // We drop out of quote mode
                                    in_quote = false;
                                }
                                else
                                {
                                    throw std::runtime_error{ make_string{} << "Malformed string passed to csv parser: " << csv };
                                }
                            }
                            else
                            {
                                // Add the current token to the vector.
                                result.push_back(token);
                                token.clear();
                                ++count;
                            }
                        }
                        else
                        {
                            // This is the start of a quoted string sequence
                            in_quote = true;
                        }
                        break;
                    case(','):
                        if (in_quote)
                        {
                            // If in a quoted string, add the character to the token
                            token += *current;
                        }
                        else
                        {
                            // Add the current token to the vector.
                            result.push_back(token);
                            token.clear();
                            ++count;
                        }
                        break;
                    default:
                        // Add the character to the string.
                        token += *current;
                        break;
                    }
                    ++current;
                }

                // If the token is not empty, save it to the list.
                if (token.size())
                {
                    result.push_back(token);
                }

                if (current != csv.end() && count < std::numeric_limits<size_t>::max())
                {
                    result.push_back(std::string{ current, csv.end() });
                }

                return result;
            }

            // Split a string on any given range of characters.
            // Optionally parse only the first n instances of the characters
            inline std::vector<std::string> tokenize(const std::string& input, const std::string& delimiters, size_t max = std::numeric_limits<size_t>::max())
            {
                // handle empty string
                if (input.empty())
                {
                    return{};
                }

                std::vector<std::string> result;

                // Find first delimiter
                auto pos = input.find_first_of(delimiters, 0);
                auto last = std::string::size_type{ 0 };
                auto count = size_t{ 0 };

                while ((std::string::npos != pos || std::string::npos != last) && (count < max))
                {
                    // Found a token, add it to the vector.
                    result.push_back(input.substr(last, pos - last));
                    if (std::string::npos != pos)
                    {
                        // Skip delimiter
                        last = pos + 1;

                        // Find next delimiter
                        pos = input.find_first_of(delimiters, last);
                    }
                    else
                    {
                        last = std::string::npos;
                    }
                    ++count;
                }

                if (last != std::string::npos && count != std::string::npos)
                {
                    result.push_back(input.substr(last, std::string::npos));
                }

                return result;
            }

            using Item = std::pair<const char*, spdlog::level::level_enum>;
            constexpr Item ItemMap[] =
            {
                { "TRACE", spdlog::level::trace},
                { "DEBUG", spdlog::level::debug },
                { "INFO", spdlog::level::info },
                { "WARNINGS", spdlog::level::warn },
                { "ERROR", spdlog::level::err },
                { "FATAL", spdlog::level::critical },
                { "OFF", spdlog::level::off }
            };

            constexpr auto ItemMapSize = sizeof ItemMap / sizeof ItemMap[0];

            constexpr spdlog::level::level_enum find_log_level(const char* key, unsigned int range = ItemMapSize)
            {
                return (range == 0) ? spdlog::level::info : (strcmp(ItemMap[range - 1].first, key) == 0) ? ItemMap[range - 1].second : find_log_level(key, range - 1);
            };

            // Implement less than ordering on strings, ignoring case
            struct caseless_less_than : std::function<bool(const std::string&, const std::string&)>
            {
            public:
                bool operator() (const std::string & s1, const std::string & s2) const
                {
                    return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), [](char a, char b)
                    {
                        return tolower(a) < tolower(b);
                    });
                }
            };
        }

        namespace attributes
        {
            static const std::set<std::string, utilities::caseless_less_than> TRUE_SET{ "1", "True", "T", "Yes", "Y" };
            static const std::set<std::string, utilities::caseless_less_than> FALSE_SET{ "0", "False", "F", "No", "N" };

            template<typename T = std::string>
            T get_attribute(const std::string& name, const std::map<std::string, std::string>& attributes)
            {
                auto search = attributes.find(name);
                if (search == attributes.end())
                {
                    throw std::logic_error{ utilities::make_string{} << "Attribute " << name << " is required but cannot be found" };
                }

                return search->second;
            }

            template<typename T>
            T get_attribute_default(const std::string& name, const std::map<std::string, std::string>& attributes, const T& def)
            {
                try
                {
                    return get_attribute<T>(name, attributes);
                }
                catch (...)
                {
                    return def;
                }
            }

            template<>
            inline bool get_attribute<bool>(const std::string& name, const std::map<std::string, std::string>& attributes)
            {
                auto attribute = get_attribute<std::string>(name, attributes);
                if (TRUE_SET.count(attribute) == 1)
                {
                    return true;
                }
                else if (FALSE_SET.count(attribute) == 1)
                {
                    return false;
                }

                throw std::logic_error{ "Attribute " + name + " is not a valid boolean" };
            }

            template<>
            inline int get_attribute<int>(const std::string& name, const std::map<std::string, std::string>& attributes)
            {
                auto attribute = get_attribute<std::string>(name, attributes);
                try
                {
                    return std::stoi(attribute);
                }
                catch (std::invalid_argument&)
                {
                    throw std::logic_error{ "Attribute " + name + " is not a valid integer" };
                }
            }

            template<>
            inline size_t get_attribute<size_t>(const std::string& name, const std::map<std::string, std::string>& attributes)
            {
                auto attribute = get_attribute<std::string>(name, attributes);
                try
                {
                    return static_cast<size_t>(std::stoul(attribute));
                }
                catch (std::invalid_argument&)
                {
                    throw std::logic_error{ "Attribute " + name + " is not a valid size_t" };
                }
            }
        }

        namespace sink_functions
        {

#ifdef __ANDROID__
            inline auto make_android_sink(const configuration::sink_config& config) -> std::shared_ptr<spdlog::sinks::sink>
            {
                auto tag = get_attribute_default<std::string>("tag", config.attributes, "spdlog");
                auto use_raw_msg = get_attribute_default<bool>("use_raw_msg", config.attributes, false);
                return std::make_shared<android_sink>(tag, use_raw_msg);
            }
#endif // __ANDROID__

#ifdef _MSC_VER
            template<typename Mutex>
            auto make_msvc_sink(const configuration::sink_config&) -> std::shared_ptr<spdlog::sinks::sink>
            {
                return std::make_shared<spdlog::sinks::msvc_sink<Mutex>>();
            }
#endif // _MSC_VER

            template<typename Mutex>
            auto make_stdout_sink(const configuration::sink_config&) -> std::shared_ptr<spdlog::sinks::sink>
            {
                return spdlog::sinks::stdout_sink<Mutex>::instance();
            }

            template<typename Mutex>
            auto make_stderr_sink(const configuration::sink_config&) -> std::shared_ptr<spdlog::sinks::sink>
            {
                return spdlog::sinks::stderr_sink<Mutex>::instance();
            }

#ifdef _WIN32
            template<typename Mutex>
            auto make_wincolor_stdout_sink(const configuration::sink_config&) -> std::shared_ptr<spdlog::sinks::sink>
            {
                return std::make_shared<spdlog::sinks::wincolor_stdout_sink<Mutex>>();
            }

            template<typename Mutex>
            auto make_wincolor_stderr_sink(const configuration::sink_config&) -> std::shared_ptr<spdlog::sinks::sink>
            {
                return std::make_shared<spdlog::sinks::wincolor_stderr_sink<Mutex>>();
            }
#else
            template<typename Mutex>
            auto make_ansicolor_stdout_sink(const configuration::sink_config&) -> std::shared_ptr<spdlog::sinks::sink>
            {
                return std::make_shared<spdlog::sinks::ansicolor_stdout_sink<Mutex>>();
            }

            template<typename Mutex>
            auto make_ansicolor_stderr_sink(const configuration::sink_config&) -> std::shared_ptr<spdlog::sinks::sink>
            {
                return std::make_shared<spdlog::sinks::ansicolor_stderr_sink<Mutex>>();
            }
#endif

#ifdef SPDLOG_ENABLE_SYSLOG
            inline auto make_syslog_sink(const configuration::sink_config& config) -> std::shared_ptr<spdlog::sinks::sink>
            {
                auto ident = get_attribute_default<std::string>("ident", config.attributes, "");
                auto syslog_option = get_attribute_default<int>("syslog_option", config.attributes, 0);
                auto syslog_facility = get_attribute_default<int>("syslog_facility", config.attributes, LOG_USER);
                return std::make_shared<syslog_sink>(ident, syslog_option, syslog_facility);
            }
#endif

            template<typename Mutex>
            auto make_simple_file_sink(const configuration::sink_config& config) -> std::shared_ptr<spdlog::sinks::sink>
            {
                auto file_path = attributes::get_attribute<std::string>("file_path", config.attributes);
                auto truncate = attributes::get_attribute_default<bool>("truncate", config.attributes, false);
                return std::make_shared<spdlog::sinks::simple_file_sink<Mutex>>(file_path, truncate);
            }

            template<typename Mutex>
            auto make_periodically_rotating_file_sink(const configuration::sink_config& config) -> spdlog::sink_ptr
            {
                // Rotates at the same time every day and after a specified set of hours/minutes
                auto file_path = attributes::get_attribute<std::string>("file_path", config.attributes);
                auto max_size = attributes::get_attribute<size_t>("max_size", config.attributes);
                auto max_files = attributes::get_attribute_default<size_t>("max_files", config.attributes, std::numeric_limits<size_t>::max());
                auto force_flush = attributes::get_attribute_default<bool>("force_flush", config.attributes, false);
                auto rotation_hour = attributes::get_attribute_default<int>("rotation_hour", config.attributes, 0);
                auto rotation_minute = attributes::get_attribute_default<int>("rotation_minute", config.attributes, 0);
                auto rotation_period_hours = attributes::get_attribute_default<int>("rotation_period_hours", config.attributes, 24);
                auto rotation_period_minutes = attributes::get_attribute_default<int>("rotation_period_minutes", config.attributes, 0);
                return std::make_shared<spdlog::sinks::daily_rotating_file_sink<Mutex, sinks::default_daily_file_name_calculator>>(file_path, max_size, max_files, force_flush, rotation_hour, rotation_minute, rotation_period_hours, rotation_period_minutes);
            }

            template<typename Mutex>
            auto make_daily_rotating_file_sink(const configuration::sink_config& config) -> spdlog::sink_ptr
            {
                // Always rotates at midnight
                auto file_path = attributes::get_attribute<std::string>("file_path", config.attributes);
                auto max_size = attributes::get_attribute<size_t>("max_size", config.attributes);
                auto max_files = attributes::get_attribute_default<size_t>("max_files", config.attributes, std::numeric_limits<size_t>::max());
                auto force_flush = attributes::get_attribute_default<bool>("force_flush", config.attributes, false);
                return std::make_shared<spdlog::sinks::daily_rotating_file_sink<Mutex, sinks::dateonly_daily_file_name_calculator>>(file_path, max_size, max_files, force_flush, 0, 0, 24, 0);
            }
			
			template<typename Mutex>
            auto make_null_sink(const configuration::sink_config&) -> std::shared_ptr<spdlog::sinks::sink>
            {
                return std::make_shared<spdlog::sinks::null_sink<Mutex>>();
            }

            inline auto make_sink_registry() -> std::map<std::string, configuration::sink_function>
            {
                std::map<std::string, configuration::sink_function> result;
                result.emplace(std::make_pair("stdout_sink_st", make_stdout_sink<details::null_mutex>));
                result.emplace(std::make_pair("stdout_sink_mt", make_stdout_sink<std::mutex>));
                result.emplace(std::make_pair("stderr_sink_st", make_stdout_sink<details::null_mutex>));
                result.emplace(std::make_pair("stderr_sink_mt", make_stdout_sink<std::mutex>));
				result.emplace(std::make_pair("null_sink_st", make_null_sink<details::null_mutex>));
                result.emplace(std::make_pair("null_sink_mt", make_null_sink<std::mutex>));
                result.emplace(std::make_pair("simple_file_sink_st", make_simple_file_sink<details::null_mutex>));
                result.emplace(std::make_pair("simple_file_sink_mt", make_simple_file_sink<std::mutex>));
                result.emplace(std::make_pair("daily_rotating_file_sink_st", make_daily_rotating_file_sink<details::null_mutex>));
                result.emplace(std::make_pair("daily_rotating_file_sink_mt", make_daily_rotating_file_sink<std::mutex>));
                result.emplace(std::make_pair("periodically_rotating_file_sink_st", make_periodically_rotating_file_sink<details::null_mutex>));
                result.emplace(std::make_pair("periodically_rotating_file_sink_mt", make_periodically_rotating_file_sink<std::mutex>));
#ifdef _WIN32
                result.emplace(std::make_pair("stdout_color_sink_st", make_wincolor_stdout_sink<details::null_mutex>));
                result.emplace(std::make_pair("stdout_color_sink_mt", make_wincolor_stdout_sink<std::mutex>));
                result.emplace(std::make_pair("stderr_color_sink_st", make_wincolor_stderr_sink<details::null_mutex>));
                result.emplace(std::make_pair("stderr_color_sink_mt", make_wincolor_stderr_sink<std::mutex>));
#else
                result.emplace(std::make_pair("stdout_color_sink_st", make_ansicolor_stdout_sink<details::null_mutex>));
                result.emplace(std::make_pair("stdout_color_sink_mt", make_ansicolor_stdout_sink<std::mutex>));
                result.emplace(std::make_pair("stderr_color_sink_st", make_ansicolor_stderr_sink<details::null_mutex>));
                result.emplace(std::make_pair("stderr_color_sink_mt", make_ansicolor_stderr_sink<std::mutex>));
#endif // _WIN32

#ifdef SPDLOG_ENABLE_SYSLOG
                result.emplace(std::make_pair("syslog_sink", make_syslog_sink));
#endif

#ifdef __ANDROID__
                result.emplace(std::make_pair("android_sink", make_android_sink));
#endif

#ifdef _MSC_VER
                result.emplace(std::make_pair("msvc_sink_st", make_msvc_sink<details::null_mutex>));
                result.emplace(std::make_pair("msvc_sink_mt", make_msvc_sink<std::mutex>));
#endif
                return result;
            }

            inline auto get_sink_registry() -> std::map<std::string, configuration::sink_function>&
            {
                static auto sink_registry = make_sink_registry();
                return sink_registry;
            }

            inline auto make_sink(const configuration::sink_config& config) -> sink_ptr
            {
                const auto& sink_registry = get_sink_registry();
                auto search = sink_registry.find(config.type);
                if (search == sink_registry.end())
                {
                    throw std::logic_error{ utilities::make_string{} << "Cannot create sink of type '" << config.type << "'" };
                }

                return search->second(config);
            }
        }

        namespace error_handlers
        {
            inline auto make_error_handler_registry()
            {
                return std::map<std::string, configuration::error_handler>{};
            }

            inline auto& get_error_handler_registry()
            {
                static auto error_handler_registry = make_error_handler_registry();
                return error_handler_registry;
            }
        }

        namespace global_functions
        {
            // Placeholder for warmup functions
            inline auto make_warmup_registry()
            {
                return std::map<std::string, configuration::warmup_function>{};
            }

            inline auto& get_warmup_registry()
            {
                static auto warmup_functions = make_warmup_registry();
                return warmup_functions;
            }

            // Placeholder for teardown functions
            inline auto make_teardown_registry()
            {
                return std::map<std::string, configuration::teardown_function>{};
            }

            inline auto& get_teardown_registry()
            {
                static auto teardown_functions = make_teardown_registry();
                return teardown_functions;
            }

            inline auto make_overflow_policy_registry() -> std::map<std::string, async_overflow_policy>
            {
                std::map<std::string, async_overflow_policy> result;
                result.insert(std::make_pair("block_retry", async_overflow_policy::block_retry));
                result.insert(std::make_pair("discard_log_msg", async_overflow_policy::discard_log_msg));
                return result;
            }

            inline auto get_worker_warmup_cb(const std::string& name)
            {
                const auto& warmup_functions = get_warmup_registry();
                auto search = warmup_functions.find(name);
                if (search == warmup_functions.end())
                {
                    return configuration::warmup_function{ nullptr };
                }
                
                return search->second;
            }

            inline auto get_worker_teardown_cb(const std::string& name)
            {
                const auto& teardown_functions = get_teardown_registry();
                auto search = teardown_functions.find(name);
                if (search == teardown_functions.end())
                {
                    return configuration::teardown_function{ nullptr };
                }

                return search->second;
            }

            inline auto get_overflow_policy(const std::string& name)
            {
                static auto overflow_policy_registry = make_overflow_policy_registry();
                auto search = overflow_policy_registry.find(name);
                if (search == overflow_policy_registry.end())
                {
                    throw std::logic_error{ utilities::make_string{} << "Cannot find overflow_policy matching '" << name << "'" };
                }

                return search->second;
            }

            inline void global_func_set_async(const configuration::global_config& config)
            {
                // First, get the queue size
                auto queue_size = stoul(config.value);

                // Defaults to block_retry
                auto overflow_policy = [&config]()
                {
                    auto search = config.attributes.find("overflow_policy");
                    if (search == config.attributes.end() || search->second.empty())
                    {
                        return async_overflow_policy::block_retry;
                    }
                    return get_overflow_policy(search->second);
                }();

                // Defaults to null
                auto worker_warmup = [&config]()
                {
                    auto search = config.attributes.find("worker_warmup_cb");
                    if (search == config.attributes.end() || search->second.empty())
                    {
                        return configuration::warmup_function{ nullptr };
                    }
                    return get_worker_warmup_cb(search->second);
                }();

                // Defaults to 0 ms
                auto flush_interval_ms = [&config]()
                {
                    auto search = config.attributes.find("flush_interval_ms");
                    if (search == config.attributes.end() || search->second.empty())
                    {
                        return std::chrono::milliseconds::zero();
                    }
                    return std::chrono::milliseconds{ stoul(search->second) };
                }();

                // Defaults to null
                auto worker_teardown = [&config]()
                {
                    auto search = config.attributes.find("worker_teardown_cb");
                    if (search == config.attributes.end() || search->second.empty())
                    {
                        return configuration::teardown_function{ nullptr };
                    }
                    return get_worker_teardown_cb(search->second);
                }();

                spdlog::set_async_mode(queue_size, overflow_policy, worker_warmup, flush_interval_ms, worker_teardown);
            }

            inline void global_func_set_pattern(const configuration::global_config& config)
            {
                spdlog::set_pattern(config.value);
            }

            inline void global_func_set_error_handler(const configuration::global_config& config)
            {
                const auto& error_handler_registry = error_handlers::get_error_handler_registry();
                auto search = error_handler_registry.find(config.value);
                if (search == error_handler_registry.end())
                {
                    throw std::logic_error{ utilities::make_string{} << "Cannot find error handler '" << config.value << "'" };
                }

                spdlog::set_error_handler(search->second);
            }

            inline auto make_global_func_registry()
            {
                std::map<std::string, configuration::global_function> result;
                result.emplace(std::make_pair("set_async", global_func_set_async));
                result.emplace(std::make_pair("set_pattern", global_func_set_pattern));
                result.emplace(std::make_pair("set_error_handler", global_func_set_error_handler));
                return result;
            }

            inline auto& get_global_func_registry()
            {
                static auto global_function_registry = make_global_func_registry();
                return global_function_registry;
            }

            inline void call_global_func(const std::string& name, const configuration::global_config& config)
            {
                static auto& global_func_registry = get_global_func_registry();

                auto search = global_func_registry.find(name);

                // If the named function isn't available, just carry on and do nothing
                if (search == global_func_registry.end())
                {
                    return;
                }

                search->second(config);
            }
        }

        namespace logger_functions
        {
            inline auto make_logger(const std::string& name, const configuration::logger_config& config, const std::map<std::string, spdlog::sink_ptr>& sinks) -> std::shared_ptr<spdlog::logger>
            {
                // Get all the sinks for this particular logger
                auto logger_sinks = [&]()
                {
                    std::vector<spdlog::sink_ptr> result;
                    for (const auto& n : config.sinks)
                    {
                        auto search = sinks.find(n);
                        if (search == sinks.end())
                        {
                            throw std::logic_error{ utilities::make_string{} << "Trying to construct logger '" << name << "', but cannot find sink '" << n << "'" };
                        }
                        result.push_back(search->second);
                    }

                    return result;
                }();

                auto logger = spdlog::details::registry::instance().create(name, std::begin(logger_sinks), std::end(logger_sinks));
                logger->set_level(utilities::find_log_level(config.threshold.c_str()));

                // See if a pattern has been specified for this logger
                {
                    auto pattern = attributes::get_attribute_default<std::string>("pattern", config.attributes, "");
                    if (!pattern.empty())
                    {
                        logger->set_pattern(pattern);
                    }
                }

                // See if an error handler has been specified for this logger
                {
                    auto handler = attributes::get_attribute_default<std::string>("set_error_handler", config.attributes, "");
                    if (!handler.empty())
                    {
                        const auto& error_handler_registry = error_handlers::get_error_handler_registry();
                        auto search = error_handler_registry.find(handler);
                        if (search == error_handler_registry.end())
                        {
                            throw std::logic_error{ utilities::make_string{} << "cannot find error handler '" << name << "'" };
                        }

                        logger->set_error_handler(search->second);
                    }
                }

                return logger;
            }
        }
    }

    inline configuration configuration::create(std::istream& in)
    {
        namespace du = detail::utilities;

        auto result = configuration{};

        std::string line;
        while (std::getline(in, line))
        {
            // If the line doesn't start with "spdlog." then ignore it
            if (line.find("spdlog.") != 0)
            {
                continue;
            }

            // Process the line - first, split on the first "=" - this will give us the primary key and value for this string
            auto key_value = du::tokenize(line, "=", 1);

            // If there aren't two tokens, then ignore the line
            if (key_value.size() != 2)
            {
                continue;
            }

            // Split the key on "."
            auto key_elements = du::tokenize(key_value.front(), ".");

            // "spdlog" is the first element, as we've determined earlier
            // Now to make some choices - if there is only a single element following "spdlog", then this must be a global function
            // If there are two, then this is either a key or logger
            if (key_elements.size() == 2)
            {
                // Create a global function
                result.add_global(key_elements.back(), key_value.back());
            }
            else if (key_elements.size() == 3)
            {
                auto type = key_elements[1];
                auto name = key_elements[2];
                if (type == "logger")
                {
                    result.add_logger(name, logger_config{ key_value.back() });
                }
                else if (type == "sink")
                {
                    result.add_sink(name, sink_config{ key_value.back() });
                }
                else
                {
                    throw std::runtime_error{ du::make_string{} << "Cannot understand this configuration string: " << line };
                }
            }
            else
            {
                // Otherwise this string is nonsense
                throw std::runtime_error{ du::make_string{} << "Cannot understand this configuration string: " << line };
            }
        }
        return result;
    }

    inline void configuration::configure()
    {
        // Call all the globals
        for (const auto& g : m_globals)
        {
            detail::global_functions::call_global_func(g.first, g.second);
        }

        // Construct all the sinks
        auto sinks = [this]
        {
            std::map<std::string, sink_ptr> result;
            for (const auto& s : m_sinks)
            {
                result.insert(std::make_pair(s.first, detail::sink_functions::make_sink(s.second)));
            }
            return result;
        }();

        // Construct all the loggers
        auto loggers = [&, this]
        {
            std::map<std::string, std::shared_ptr<spdlog::logger>> result;
            for (const auto& l : m_loggers)
            {
                result.insert(std::make_pair(l.first, detail::logger_functions::make_logger(l.first, l.second, sinks)));
            }

            return result;
        }();
    }

    inline void configuration::add_global(const std::string& name, const configuration::global_config& g)
    {
        m_globals.emplace(name, g);
    }

    inline void configuration::add_sink(const std::string& name, const configuration::sink_config& s)
    {
        m_sinks.emplace(name, s);
    }

    inline void configuration::add_logger(const std::string& name, const configuration::logger_config& l)
    {
        m_loggers.emplace(name, l);
    }

    inline configuration::config_line configuration::config_line::parse(const std::string& config)
    {
        namespace du = detail::utilities;

        // Need to parse this as a CSV initially - the first parameter might contain one or more commas
        auto tokens = du::parse_csv(config, 1);
        if (tokens.empty())
        {
            throw std::runtime_error{ du::make_string{} << "Empty config line found" };
        }

        if (tokens.size() > 2)
        {
            throw std::runtime_error{ du::make_string{} << "Invalid config line found" };
        }

        config_line result;
        result.value = tokens.front();

        if (tokens.size() == 2)
        {
            // Now get the attributes
            auto attribute_tokens = [token = tokens.back()]()
            {
                auto start = token.find('[');
                auto end = token.rfind(']');
                auto attr = token.substr(start + 1, end - start - 1);
                return du::parse_csv(attr);
            }();

            for (const auto& at : attribute_tokens)
            {
                auto kv = du::tokenize(at, "=");
                if (kv.size() != 2)
                {
                    throw std::runtime_error{ du::make_string{} << "Invalid attribute definition found: " << at };
                }

                result.attributes.insert(std::make_pair(kv.front(), kv.back()));
            }
        }

        return result;
    }

    inline configuration::global_config::global_config(const std::string& config)
    {
        // Parse the string: {name}(,[{attributes}])
        auto line = config_line::parse(config);

        // Here, simply move the attributes over
        value = std::move(line.value);
        attributes = std::move(line.attributes);
    }

    inline configuration::sink_config::sink_config(const std::string& config)
    {
        // Parse the string: {name}(,[{attributes}])
        auto line = config_line::parse(config);

        // Here, simply copy the attributes over
        type = std::move(line.value);
        attributes = std::move(line.attributes);
    }

    inline configuration::logger_config::logger_config(const std::string& config)
    {
        // Parse the string: {threshold}(,[{attributes}])
        auto line = config_line::parse(config);

        // Find the sinks - this is a mandatory attribute
        sinks = [&line]()
        {
            namespace da = detail::attributes;
            namespace du = detail::utilities;

            // This will throw if "sinks" isn't present
            auto attribute = da::get_attribute<std::string>("sinks", line.attributes);
            return du::tokenize(attribute, ":");
        }();
        
        // Then, simply copy the rest of the attributes over
        threshold = std::move(line.value);
        attributes = std::move(line.attributes);
    }

    inline void configuration::register_custom_global_function(const std::string& name, global_function func)
    {
        detail::global_functions::get_global_func_registry()[name] = func;
    }

    inline void configuration::register_custom_sink(const std::string& name, sink_function func)
    {
        detail::sink_functions::get_sink_registry()[name] = func;
    }

    inline void configuration::register_worker_warmup(const std::string& name, std::function<void()> func)
    {
        detail::global_functions::get_warmup_registry()[name] = func;
    }

    inline void configuration::register_worker_teardown(const std::string& name, std::function<void()> func)
    {
        detail::global_functions::get_teardown_registry()[name] = func;
    }

    inline void configuration::register_error_handler(const std::string& name, std::function<void(const std::string&)> func)
    {
        detail::error_handlers::get_error_handler_registry()[name] = func;
    }
}
