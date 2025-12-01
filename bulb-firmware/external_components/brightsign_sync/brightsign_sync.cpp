// brightsign_sync/brightsign_sync.cpp
#include "brightsign_sync.h"
#include "esphome/core/log.h"

namespace esphome
{
    namespace brightsign_sync
    {

        static const char *const TAG = "brightsign_sync";

        void BrightsignSyncComponent::setup()
        {
            // Configure UDP for BrightSign multicast
            // Group: 224.0.126.10
            // Port:  1539
            this->set_listen_port(1539);
            this->set_listen_address("224.0.126.10");
            this->set_should_listen();

            // Register packet listener
            this->add_listener([this](std::vector<uint8_t> &buf)
                               { this->handle_packet_(buf); });

            // Call base UDP setup (creates sockets, joins multicast, etc.)
            udp::UDPComponent::setup();
        }

        void BrightsignSyncComponent::handle_packet_(std::vector<uint8_t> &buf)
        {
            // Convert buffer to string
            std::string data(reinterpret_cast<const char *>(buf.data()), buf.size());
            ESP_LOGD(TAG, "Raw packet: %s", data.c_str());

            // ---- Outer JSON ---------------------------------------------------------
            DynamicJsonDocument outer_doc(4096);
            DeserializationError err = deserializeJson(outer_doc, data);
            if (err)
            {
                ESP_LOGW(TAG, "Outer JSON parse failed: %s", err.c_str());
                return;
            }

            if (!outer_doc.containsKey("p"))
            {
                ESP_LOGW(TAG, "No 'p' field in outer JSON");
                return;
            }

            const char *inner_json = outer_doc["p"];
            if (inner_json == nullptr)
            {
                ESP_LOGW(TAG, "'p' field is null");
                return;
            }

            ESP_LOGD(TAG, "Inner JSON: %s", inner_json);

            // ---- Inner JSON (string inside "p") -------------------------------------
            DynamicJsonDocument inner_doc(4096);
            err = deserializeJson(inner_doc, inner_json);
            if (err)
            {
                ESP_LOGW(TAG, "Inner JSON parse failed: %s", err.c_str());
                return;
            }

            JsonVariant d = inner_doc["d"];
            if (d.isNull())
            {
                ESP_LOGW(TAG, "Inner JSON has no 'd' object");
                return;
            }

            // "t" timestamp, e.g. 20251201T161907.238142
            const char *t_value = d["t"] | nullptr;
            if (t_value != nullptr && t_ != t_value)
            {
                ESP_LOGI(TAG, "t timestamp: %s", t_value);
                t_ = t_value;
            }

            // "x" e.g. "BS1\nsync-id-1\n20251130T221934.758333"
            const char *x_value = d["x"] | nullptr;
            if (x_value != nullptr && x_ != x_value)
            {
                std::string x_str(x_value);

                // Extract last line as timestamp part
                std::string x_timestamp = x_str;
                auto pos = x_str.rfind('\n');
                if (pos != std::string::npos && pos + 1 < x_str.size())
                {
                    x_timestamp = x_str.substr(pos + 1);
                }

                ESP_LOGI(TAG, "x timestamp: %s", x_timestamp.c_str());
                x_ = x_timestamp;
            }
        }

    } // namespace brightsign_sync
} // namespace esphome
