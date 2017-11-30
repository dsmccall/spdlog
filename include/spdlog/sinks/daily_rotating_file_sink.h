#pragma once

#include "file_sinks.h"

namespace spdlog
{
namespace sinks
{
    template<class Mutex, class FileNameCalc = dateonly_daily_file_name_calculator>
    class daily_rotating_file_sink SPDLOG_FINAL : public base_sink < Mutex >
    {
    public:
        daily_rotating_file_sink(const filename_t &base_filename, std::size_t max_size,
            std::size_t max_files = std::numeric_limits<size_t>::max(), int rotation_hour = 0, int rotation_minute = 0, int rotation_period_hours = 24, int rotation_period_minutes = 0) :
            _base_filename(base_filename),
            _current_base_filename(FileNameCalc::calc_filename(_base_filename)),
            _max_size(max_size),
            _max_files(max_files),
            _rotation_h(rotation_hour),
            _rotation_m(rotation_minute),
            _rotation_period_hours(rotation_period_hours),
            _rotation_period_minutes(rotation_period_minutes),
            _current_size(0),
            _file_helper()
        {
            if (rotation_hour < 0 || rotation_hour > 23 || rotation_minute < 0 || rotation_minute > 59)
            {
                throw spdlog_ex("daily_rotating_file_sink: Invalid rotation time in ctor");
            }
            _rotation_tp = _next_rotation_tp();
            _file_helper.open(calc_filename(_current_base_filename, 0));
            _current_size = _file_helper.size(); //expensive. called only once
        }

    protected:
        void _sink_it(const details::log_msg& msg) override
        {
            _current_size += msg.formatted.size();

            if (std::chrono::system_clock::now() >= _rotation_tp)
            {
                _current_base_filename = calc_filename(FileNameCalc::calc_filename(_base_filename), 0);
                _file_helper.open(_current_base_filename);
                _rotation_tp = _next_rotation_tp();
                _current_size = msg.formatted.size();
            }
            else if (_current_size > _max_size)
            {
                _rotate();
                _current_size = msg.formatted.size();
            }
            _file_helper.write(msg);
        }

        void _flush() override
        {
            _file_helper.flush();
        }

    private:
        static filename_t calc_filename(const filename_t& filename, std::size_t index)
        {
            std::conditional<std::is_same<filename_t::value_type, char>::value, fmt::MemoryWriter, fmt::WMemoryWriter>::type w;
            if (index)
                w.write(SPDLOG_FILENAME_T("{}.{}"), filename, index);
            else
                w.write(SPDLOG_FILENAME_T("{}"), filename);
            return w.str();
        }

        // Rotate files:
        // log.txt -> log.txt.1
        // log.txt.1 -> log.txt.2
        // log.txt.2 -> log.txt.3
        // lo3.txt.3 -> delete

        void _rotate()
        {
            using details::os::filename_to_str;
            _file_helper.close();

            auto files = [this]()
            {
                std::vector<std::pair<filename_t, filename_t>> result;
                auto src = calc_filename(_current_base_filename, 0);
                auto target = calc_filename(_current_base_filename, 1);

                result.emplace_back(src, target);
                std::size_t i = 1;

                while (details::file_helper::file_exists(target) && i <= _max_files)
                {
                    src = calc_filename(_current_base_filename, i);
                    target = calc_filename(_current_base_filename, ++i);
                    result.emplace_back(src, target);
                }

                return result;
            }();

            // Remove all files above _max_files
            auto iter = files.begin() + std::min(files.size(), _max_files);
            for (auto file = iter; file != files.end(); ++file)
            {
                if (details::os::remove(file->first) != 0)
                {
                    throw spdlog_ex("daily_rotating_file_sink: failed removing " + filename_to_str(file->first), errno);
                }
            }

            files.erase(iter, files.end());
            
            for (auto file = files.rbegin(); file != files.rend(); ++file)
            {
                if (details::file_helper::file_exists(file->first) && details::os::rename(file->first, file->second))
                {
                    throw spdlog_ex("daily_rotating_file_sink: failed renaming " + filename_to_str(file->first) + " to " + filename_to_str(file->second), errno);
                }
            }

            _file_helper.reopen(true);
        }

        std::chrono::system_clock::time_point _next_rotation_tp()
        {
            auto now = std::chrono::system_clock::now();
            auto tnow = std::chrono::system_clock::to_time_t(now);
            auto date = spdlog::details::os::localtime(tnow);
            date.tm_hour = _rotation_h;
            date.tm_min = _rotation_m;
            date.tm_sec = 0;
            auto rotation_time = std::chrono::system_clock::from_time_t(std::mktime(&date));
            if (rotation_time > now)
            {
                return rotation_time;
            }
            else
            {
                return std::chrono::system_clock::time_point(rotation_time + std::chrono::hours(_rotation_period_hours) + std::chrono::minutes(_rotation_period_minutes));
            }
        }

        filename_t _base_filename;
        filename_t _current_base_filename;
        std::size_t _max_size;
        std::size_t _max_files;
        std::size_t _current_size;

        int _rotation_h;
        int _rotation_m;
        int _rotation_period_hours;
        int _rotation_period_minutes;
        std::chrono::system_clock::time_point _rotation_tp;

        details::file_helper _file_helper;
    };

    using daily_rotating_file_sink_mt = daily_rotating_file_sink<std::mutex>;
    using daily_rotating_file_sink_st = daily_rotating_file_sink<details::null_mutex>;
}
}
