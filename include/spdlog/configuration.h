#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace spdlog
{
    namespace sinks
    {
        class sink;
    }

    //
    // Allows for automatic configuration of sinks, loggers, global functions, etc
    // A logging configuration can be generated automatically from a collection of strings, all of which take the following form:
    //     spdlog.{entity}={value},[{attributes}]
    //  An entity may be a global function, a sink, or a logger
    //  The importance of {value} will change depending on the entity type, according to the following rules:
    //      global function : non-optional argument for the global function
    //      sink            : sink type (file sink, stdout, etc)
    //      logger          : logger threshold
    //
    //  Examples:
    //      - Create an instruction to call set_async, with a queue size of 16384, an overflow_policy of block_retry, and custom warmup/teardown functions
    //
    //      spdlog.set_async=16384,[overflow_policy=block_retry,flush_interval_ms=0,worker_warmup_cb=custom_warmup_function,worker_teardown_cb=custom_teardown_function]
    //      
    //      - Create a simple file sink named "my_file_sink", where the filename is "c:\library.log", with a truncate setting of false
    //
    //      spdlog.sink.my_file_sink=simple_file_sink,[file_path="C:\library.log",truncate=false]
    //
    //      - Create a logger named "my_logger", which has a threshold of "TRACE", a pattern of "%v", and has two sinks named "sink_a" and "sink_b"
    //
    //      spdlog.logger.my_logger=TRACE,[sinks=sink_a:sink_b,pattern="%v"]
    //
    class configuration
    {
    public:

        configuration() {}

        //
        // Holds the configuration for a sink - this consists of:
        //  a value
        //  a map of optional attributes
        //
        struct global_config
        {
            // Construct from a string
            global_config(const std::string&);
            std::string value;
            std::map<std::string, std::string> attributes;
        };

        //
        // Holds the configuration for a sink - this consists of:
        //  a type (string containing the name of a type of sink)
        //  a map of optional attributes
        //
        struct sink_config
        {
            // Construct from a string
            sink_config(const std::string&);
            std::string type;
            std::map<std::string, std::string> attributes;
        };

        //
        // Holds the configuration for a logger - this consists of:
        //  a threshold (string containing the threshold at which this logger will log)
        //  a vector of sink names
        //  a map of optional attributes
        //
        struct logger_config
        {
            // Construct from a string
            logger_config(const std::string&);
            logger_config(){};
            std::string threshold;
            std::vector<std::string> sinks;
            std::map<std::string, std::string> attributes;
        };

        //
        // Add a named global function
        //
        void add_global(const std::string& name, const global_config& g);

        //
        // Add a named sink
        //
        void add_sink(const std::string& name, const sink_config& s);

        //
        // Add a named logger
        //
        void add_logger(const std::string& name, const logger_config& l);

        //
        // Path components are elements from a configuration line that have been separated on the '.' character
        // e.g. logger.pattern has two path components
        //
        using path_components = std::vector<std::string>;

        //
        // Config attributes are map of string to string - these are generated from comma-separated strings
        //
        using config_attributes = std::map<std::string, std::string>;

        //
        // A config line comprises a value and an optional map of attributes
        // This might be generated by something like:
        //  logger.my_logger=TRACE,[sinks=sink_a:sink_b,pattern="%v,%v"]
        // The config line is everything to the right of the "=", therefore it will contain:
        //  value : "TRACE"
        //  attributes : { sinks : "sink_a:sink_b" , pattern : "%v,%v" }
        // The attributes string is parsed as a CSV, so if an attribute value contains a comma, it must
        // be contained within double quotes
        struct config_line
        {
            static config_line parse(const std::string&);
            std::string value;
            config_attributes attributes;
        };

        //
        // Map of named loggers to logger_config objects
        //
        using loggers = std::map<std::string, logger_config>;

        //
        // Map of named sinks to sink_config objects
        //
        using sinks = std::map<std::string, sink_config>;

        //
        // Map of global function names to global_config objects
        //
        using globals = std::map<std::string, global_config>;

        // 
        // A function signature which returns a shared_ptr to a sink.  The argument is a sink_config
        //
        using sink_function = std::function<std::shared_ptr<spdlog::sinks::sink>(const configuration::sink_config&)>;

        //
        // A function signature for a global function.  The argument is a global_config
        //
        using global_function = std::function<void(const global_config&)>;

        //
        // Signature for warmup function
        //
        using warmup_function = std::function<void()>;

        //
        // Signature for teardown function
        //
        using teardown_function = std::function<void()>;

        //
        // Signature for error_handler function
        //
        using error_handler = std::function<void(const std::string&)>;

        //
        // Register a custom global function
        //
        static void register_custom_global_function(const std::string&, global_function);

        //
        // Register a custom sink - the first argument is the 'type', the second is a function that will return
        // the sink
        //
        static void register_custom_sink(const std::string&, sink_function);

        //
        // Register a custom worker warmup function
        //
        static void register_worker_warmup(const std::string&, warmup_function);

        //
        // Register a custom worker teardown function
        //
        static void register_worker_teardown(const std::string&, teardown_function);

        //
        // Register a custom error handler
        //
        static void register_error_handler(const std::string&, error_handler);

        //
        // Automatically build a configuration setup from an istream
        //
        static configuration create(std::istream&);

        //
        // Automatically build a configuration setup from a vector of configuration lines
        //
        static configuration create(const std::vector<config_line>&);

        //
        // Action time!
        // Run all global functions, create all sinks, create all loggers
        //
        void configure();

    private:
        globals m_globals;
        loggers m_loggers;
        sinks m_sinks;
    };
}

#include "spdlog/details/configuration_impl.h"