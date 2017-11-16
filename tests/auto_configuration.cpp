#include "includes.h"
#include <sstream>

namespace spd = spdlog;

class throwing_sink : public spdlog::sinks::sink
{
    void log(const spdlog::details::log_msg& msg) override
    {
        std::stringstream error_message;
        error_message << "Error in 'throwing_sink': " << msg.raw.str();
        throw std::runtime_error{ error_message.str() };
    }

    void flush()
    {}
};

template<typename Mutex>
class test_sink : public spd::sinks::base_sink<Mutex>
{
public:
    test_sink()
    {}

    const std::vector<std::string>& messages() const
    {
        return m_messages;
    }

protected:
    virtual void _sink_it(const spd::details::log_msg& msg) override
    {
        // Just write the raw message
        m_messages.push_back(msg.raw.str());
    }

    virtual void _flush() override
    {}

private:
    std::vector<std::string> m_messages;
};

TEST_CASE("basic", "[auto_configuration]")
{
    {
        auto conf = spd::configuration{};
        conf.add_sink("test_stdout_sink", spd::configuration::sink_config{ "stdout_sink_st" });
        conf.add_sink("test_stderr_sink", spd::configuration::sink_config{ "stderr_sink_st" });
        conf.add_logger("test_logger", spd::configuration::logger_config{ "INFO,[sinks=test_stdout_sink:test_stderr_sink]" });
        conf.configure();
    }

    auto logger = spd::get("test_logger");

    REQUIRE(logger);

    const auto& sinks = logger->sinks();

    REQUIRE(sinks.size() == 2);

    spd::drop_all();
}

TEST_CASE("custom_sink", "[auto_configuration]")
{
    spd::configuration::register_custom_sink("test_sink_mt", [](const spdlog::configuration::sink_config&)
    {
        return std::make_shared<test_sink<std::mutex>>();
    });

    spd::configuration::register_custom_sink("test_sink_st", [](const spdlog::configuration::sink_config&)
    {
        return std::make_shared<test_sink<spd::details::null_mutex>>();
    });

    {
        auto conf = spd::configuration{};
        conf.add_sink("test_stdout_sink", spd::configuration::sink_config{ "test_sink_mt" });
        conf.add_sink("test_stderr_sink", spd::configuration::sink_config{ "test_sink_st" });
        conf.add_logger("test_logger", spd::configuration::logger_config{ R"(INFO,[sinks=test_stdout_sink:test_stderr_sink,pattern="%v"])" });

        conf.configure();
    }
    
    auto logger = spd::get("test_logger");

    REQUIRE(logger);

    const auto& sinks = logger->sinks();

    REQUIRE(sinks.size() == 2);

    auto mt_sink = std::dynamic_pointer_cast<test_sink<std::mutex>>(sinks[0]);
    auto st_sink = std::dynamic_pointer_cast<test_sink<spd::details::null_mutex>>(sinks[1]);

    REQUIRE(mt_sink);
    REQUIRE(st_sink);

    logger->trace("trace message");
    logger->debug("debug message");
    logger->info("info message");
    logger->warn("warn message");
    logger->error("error message");
    logger->critical("critical message");

    // Should only be logging info and above
    REQUIRE(mt_sink->messages().size() == 4);
    REQUIRE(st_sink->messages().size() == 4);

    REQUIRE(mt_sink->messages()[0] == "info message");
    REQUIRE(st_sink->messages()[0] == "info message");
    REQUIRE(mt_sink->messages()[1] == "warn message");
    REQUIRE(st_sink->messages()[1] == "warn message");
    REQUIRE(mt_sink->messages()[2] == "error message");
    REQUIRE(st_sink->messages()[2] == "error message");
    REQUIRE(mt_sink->messages()[3] == "critical message");
    REQUIRE(st_sink->messages()[3] == "critical message");

    spd::drop_all();
}

TEST_CASE("macros", "[auto_configuration]")
{
    spd::configuration::register_custom_sink("test_sink_mt", [](const spdlog::configuration::sink_config&)
    {
        return std::make_shared<test_sink<std::mutex>>();
    });

    spd::configuration::register_custom_sink("test_sink_st", [](const spdlog::configuration::sink_config&)
    {
        return std::make_shared<test_sink<spd::details::null_mutex>>();
    });

    {
        auto conf = spd::configuration{};
        conf.add_sink("test_stdout_sink", spd::configuration::sink_config{ "test_sink_mt" });
        conf.add_sink("test_stderr_sink", spd::configuration::sink_config{ "test_sink_st" });
        conf.add_logger("test_logger", spd::configuration::logger_config{ R"(INFO,[sinks=test_stdout_sink:test_stderr_sink,pattern="%v"])" });
        
        conf.configure();
    }

    auto logger = spd::get("test_logger");

    REQUIRE(logger);

    const auto& sinks = logger->sinks();

    REQUIRE(sinks.size() == 2);

    auto mt_sink = std::dynamic_pointer_cast<test_sink<std::mutex>>(sinks[0]);
    auto st_sink = std::dynamic_pointer_cast<test_sink<spd::details::null_mutex>>(sinks[1]);

    REQUIRE(mt_sink);
    REQUIRE(st_sink);

    SPD_AUTO_TRACE("trace message");
    SPD_AUTO_DEBUG("debug message");
    SPD_AUTO_INFO("info message");
    SPD_AUTO_WARN("warn message");
    SPD_AUTO_ERROR("error message");
    SPD_AUTO_CRITICAL("critical message");

    // Should only be logging info and above
    REQUIRE(mt_sink->messages().size() == 4);
    REQUIRE(st_sink->messages().size() == 4);

    REQUIRE(mt_sink->messages()[0] == "info message");
    REQUIRE(st_sink->messages()[0] == "info message");
    REQUIRE(mt_sink->messages()[1] == "warn message");
    REQUIRE(st_sink->messages()[1] == "warn message");
    REQUIRE(mt_sink->messages()[2] == "error message");
    REQUIRE(st_sink->messages()[2] == "error message");
    REQUIRE(mt_sink->messages()[3] == "critical message");
    REQUIRE(st_sink->messages()[3] == "critical message");

    spd::drop_all();
}

TEST_CASE("globals", "[auto_configuration]")
{
    std::atomic<size_t> warmup_called = 0;
    std::atomic<size_t> teardown_called = 0;
    std::string error_message = "";

    spd::configuration::register_worker_warmup("test_warmup", [&warmup_called]()
    {
        ++warmup_called;
    });

    spd::configuration::register_worker_teardown("test_teardown", [&teardown_called]()
    {
        ++teardown_called;
    });

    spd::configuration::register_error_handler("test_error_handler", [&error_message](const std::string& msg)
    {
        error_message = msg;
    });

    spd::configuration::register_custom_sink("throwing_sink", [](const spdlog::configuration::sink_config&)
    {
        return std::make_shared<throwing_sink>();
    });

    {
        auto conf = spd::configuration{};

        conf.add_global("set_async", spd::configuration::global_config{ "16384,[worker_warmup_cb=test_warmup,worker_teardown_cb=test_teardown]" });
        conf.add_global("set_error_handler", spd::configuration::global_config{ "test_error_handler" });
        conf.add_sink("test_throwing_sink", spd::configuration::sink_config{ "throwing_sink" });
        conf.add_logger("test_logger", spd::configuration::logger_config{ R"(INFO,[sinks=test_throwing_sink,pattern="%v"])" });

        conf.configure();
    }

    SPD_AUTO_ERROR("my caught error message");

    spd::drop_all();
    spd::set_sync_mode();
    spd::set_error_handler(nullptr);

    REQUIRE(warmup_called == 1);
    REQUIRE(teardown_called == 1);
    REQUIRE(error_message == "Error in 'throwing_sink': my caught error message");
}
