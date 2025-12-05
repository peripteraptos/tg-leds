#pragma once

#include "esphome/core/defines.h"

#ifdef USE_NETWORK

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/network/util.h"
#include "../brightsign_sync/brightsign_sync.h"

#include <memory>
#include <string>
#include <vector>

namespace esphome
{
    namespace light_sync
    {

        struct SequenceSample
        {
            uint16_t time_ms; // time in milliseconds
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };

        class LightSyncComponent : public Component
        {
        public:
            void setup() override;
            void loop() override;
            void dump_config() override;
            float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

            void set_server_host(const std::string &host) { this->server_host_ = host; }
            void set_server_port(uint16_t port) { this->server_port_ = port; }
            void set_client_id(const std::string &id) { this->client_id_ = id; }

            void set_brightsign_sync(brightsign_sync::BrightsignSyncComponent *bs)
            {
                this->brightsign_sync_ = bs;
            }

            /// Returns true if a valid current RGB value is available.
            bool get_current_rgb(uint8_t &r, uint8_t &g, uint8_t &b) const
            {
                if (!this->sequence_loaded_ || !this->has_color_)
                    return false;
                r = this->current_r_;
                g = this->current_g_;
                b = this->current_b_;
                return true;
            }

        protected:
            void ensure_connected_();
            bool connect_to_server_();
            void send_hello_();
            brightsign_sync::BrightsignSyncComponent *brightsign_sync_{nullptr};
            void process_tcp_();
            void process_line_(const std::string &line);
            void update_playback_();

            static std::string trim_(const std::string &s);

            std::string server_host_;
            uint16_t server_port_{0};
            std::string client_id_;

            std::unique_ptr<socket::Socket> tcp_;
            bool connected_{false};
            std::string recv_buffer_;

            std::vector<SequenceSample> sequence_;
            uint32_t sequence_length_ms_{0};
            uint32_t sequence_start_time_ms_{0};

            bool sequence_loaded_{false};
            float total_length_s_{0.0f};  // keep for logging
            uint32_t total_length_ms_{0}; // for looping

            bool have_sync_{false};
            uint32_t base_sequence_time_ms_{0}; // SYNC time in ms
            uint32_t base_clock_ms_{0};
            void on_sync_time(uint32_t time_ms, uint32_t start_time_ms);

            // current color state
            bool has_color_{false};
            uint8_t current_r_{0};
            uint8_t current_g_{0};
            uint8_t current_b_{0};
        };

    } // namespace light_sync
} // namespace esphome

#endif // USE_NETWORK
