// brightsign_sync/brightsign_sync.cpp
#include "brightsign_sync.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#include <cctype>

#ifdef _WIN32
#define timegm _mkgmtime
#endif

namespace esphome
{
    namespace brightsign_sync
    {

        static const char *const TAG = "brightsign_sync";

        // Simple integer parser for a substring [pos, pos+len)
        // Returns -1 on error (non-digit or out of range).
        int BrightsignSyncComponent::parse_int_range(const std::string &s, size_t pos, size_t len)
        {
            if (pos + len > s.size())
                return -1;

            int value = 0;
            for (size_t i = 0; i < len; i++)
            {
                char c = s[pos + i];
                if (c < '0' || c > '9')
                    return -1;
                value = value * 10 + (c - '0');
            }
            return value;
        }

        // ParseISO8601 for timestamps like "20251201T231938.029876"
        // Returns milliseconds since Unix epoch (UTC) as uint64_t.
        uint32_t BrightsignSyncComponent::ParseISO8601(const std::string &input)
        {
            // if input has more then one line, take the last line
            size_t last_newline = input.rfind('\n');
            if (last_newline != std::string::npos)
            {
                return ParseISO8601(input.substr(last_newline + 1));
            }

            // Minimum: "YYYYMMDDThhmmss" = 15 chars (no fractional part)
            if (input.size() < 15 || input[8] != 'T')
            {
                ESP_LOGW(TAG, "ParseISO8601: invalid format '%s'", input.c_str());
                return 0;
            }

            int year = parse_int_range(input, 0, 4);
            int month = parse_int_range(input, 4, 2);
            int mday = parse_int_range(input, 6, 2);
            int hour = parse_int_range(input, 9, 2);
            int min = parse_int_range(input, 11, 2);
            int sec = parse_int_range(input, 13, 2);
            int msec = parse_int_range(input, 16, 3);

            if (year < 0 || month < 0 || mday < 0 || hour < 0 || min < 0 || sec < 0)
            {
                ESP_LOGW(TAG, "ParseISO8601: failed to parse main fields '%s'", input.c_str());
                return 0;
            }

            return month * 12 * 31 * 24 * 3600 * 1000ULL +
                   (mday - 1) * 24 * 3600 * 1000ULL +
                   hour * 3600 * 1000ULL +
                   min * 60 * 1000ULL +
                   sec * 1000ULL +
                   (msec >= 0 ? msec : 0);
        }

        void BrightsignSyncComponent::setup()
        {

            // Configure UDP for BrightSign multicast
            // Group: 224.0.126.10
            // Port:  1539

            // Register packet listener
            this->parent_->add_listener([this](std::vector<uint8_t> &buf)
                                        { this->handle_packet_(buf); });
            // Call base UDP setup (creates sockets, joins multicast, etc.)
            this->parent_->setup();
        }

        void BrightsignSyncComponent::handle_packet_(std::vector<uint8_t> &buf)
        {
            // Convert buffer to string
            std::string data(reinterpret_cast<const char *>(buf.data()), buf.size());

            // ---- Outer JSON ---------------------------------------------------------
            auto doc = json::parse_json(data);
            auto obj = doc.as<JsonObjectConst>();
            // "t" field
            auto subdoc = json::parse_json(obj["p"]);
            uint32_t t = ParseISO8601(subdoc["d"]["t"].as<std::string>());

            ESP_LOGD(TAG, "Parsed t=%u", t);
            // "x" field
            uint32_t x = ParseISO8601(subdoc["d"]["x"].as<std::string>());
            ESP_LOGD(TAG, "Parsed x=%u", x);

            // Notify listeners
            BrightsignSyncTime msg{t, x};
            this->on_msg_t_callback_.call(msg);
        }

        float BrightsignSyncComponent::get_setup_priority() const
        {
            return setup_priority::LATE;
        }
    } // namespace brightsign_sync
} // namespace esphome
