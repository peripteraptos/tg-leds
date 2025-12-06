#pragma once

#include "esphome/core/defines.h"

#ifdef USE_NETWORK

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/socket/socket.h"

namespace esphome
{
    namespace light_sync
    {

        static constexpr uint16_t MAX_FRAMES = 15000;
        static constexpr uint16_t BUF_SIZE = 1024;

        struct SequenceFrame
        {
            bool valid = false;
            uint16_t frame_index = 0; // 0..total_frames-1
            uint8_t r = 0, g = 0, b = 0;
        };

        class LightSyncComponent : public Component
        {
        public:
            void setup() override;
            void loop() override;
            float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

            bool get_current_rgb(uint8_t &r, uint8_t &g, uint8_t &b) const
            {
                if (!this->has_color_)
                    return false;
                r = this->current_r_;
                g = this->current_g_;
                b = this->current_b_;
                return true;
            }

            void set_client_id(uint8_t client_id)
            {
                this->client_id_ = client_id;
            }

            void set_listen_port(uint16_t port)
            {
                this->listen_port_ = port;
            }
            void set_broadcast_port(uint16_t port)
            {
                this->broadcast_port_ = port;
            }

        protected:
            void update_playback_();
            void handle_packet_();
            void send_ping_();
            void on_frame_received_(uint16_t frame_index, uint8_t r, uint8_t g, uint8_t b);
            uint32_t unwrap_frame_id_(uint16_t frame_mod);

            uint8_t client_id_;

            std::unique_ptr<socket::Socket> sock_;

            std::string server_addr_{};
            uint32_t last_ping_ms_{0};
            uint32_t received_samples_n_{0};
            uint32_t current_frame_{0};

            SequenceFrame buffer_[BUF_SIZE];
            uint32_t playhead_id_{0};
            uint32_t bufferhead_id_{0};
            uint32_t last_step_ms_{0};
            uint32_t frame_interval_ms_{42}; // 25 fps, adjust to your video
            uint32_t total_frames_{0};

            void apply_pixel_(uint8_t r, uint8_t g, uint8_t b);

            uint16_t listen_port_{1234}; // whatever port you need
            uint16_t broadcast_port_{1235};

            // current color state
            bool has_color_{false};
            uint8_t current_r_{0};
            uint8_t current_g_{0};
            uint8_t current_b_{0};
        };

        // forward distance when moving from `from` to `to` along the ring
        static inline uint16_t forward_dist(uint16_t from, uint16_t to, uint16_t total)
        {
            return (uint16_t)((to + total - from) % total);
        }

        // is `candidate` ahead of `base` but not more than half a cycle?
        static inline bool is_ahead(uint16_t base, uint16_t candidate, uint16_t total)
        {
            uint16_t d = forward_dist(base, candidate, total);
            return d > 0 && d < total / 2;
        }

        // is `candidate` behind `base` (i.e. base ahead of candidate)?
        static inline bool is_behind(uint16_t base, uint16_t candidate, uint16_t total)
        {
            return is_ahead(candidate, base, total);
        }
    } // namespace light_sync
} // namespace esphome

#endif // USE_NETWORK
