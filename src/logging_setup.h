#pragma once
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/json.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)



inline void InitJsonLogging() {
    logging::add_console_log(
        std::cout,
        boost::log::keywords::format = [](const logging::record_view& rec, logging::formatting_ostream& strm) {
            using namespace boost::posix_time;
            auto now = microsec_clock::universal_time();
            json::object log_obj;

            log_obj["timestamp"] = to_iso_extended_string(now);

            if (auto msg = rec[expr::smessage])
                log_obj["message"] = *msg;

            if (auto data = rec[additional_data])
                log_obj["data"] = *data;
            else
                log_obj["data"] = json::object{};

            strm << json::serialize(log_obj);
        });
}
