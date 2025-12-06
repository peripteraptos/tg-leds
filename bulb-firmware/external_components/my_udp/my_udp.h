#include "esphome/core/component.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/log.h"

namespace esphome
{
    namespace my_udp
    {

        static constexpr uint16_t MAX_FRAMES = 15000;
        static constexpr uint16_t BUF_SIZE = 1024;

        struct FrameSlot
        {
            bool valid = false;
            uint16_t frame_index = 0; // 0..total_frames-1
            uint8_t r = 0, g = 0, b = 0;
        };

        FrameSlot buffer[BUF_SIZE];

        static const char *const TAG = "my_udp";

        class MyUDPComponent : public Component
        {
        public:
            void setup() override
            {
                using socket::set_sockaddr_any;
                using socket::socket_ip;

                // Create UDP socket (IPv4 or IPv6 depending on config)
                this->sock_ = socket_ip(SOCK_DGRAM, IPPROTO_UDP);
                if (!this->sock_)
                {
                    status_set_error("Failed to create UDP socket");
                    mark_failed();
                    return;
                }

                // Bind to any address on your port
                struct sockaddr addr{};
                auto len = socket::set_sockaddr_any(&addr, sizeof(addr), this->listen_port_);
                if (len == 0)
                {
                    status_set_error("set_sockaddr_any failed");
                    mark_failed();
                    return;
                }

                if (this->sock_->bind(&addr, len) != 0)
                {
                    status_set_error("bind() failed");
                    mark_failed();
                    return;
                }

                // Non-blocking is usually nice
                this->sock_->setblocking(false);
            }

            void sendPing()
            {

                uint32_t now = millis();
                if (now - this->last_ping_ms_ < 3000)
                    return; // send ping every 5 seconds

                this->last_ping_ms_ = now;

                if (this->server_addr_.empty())
                {
                    ESP_LOGD(TAG, "Server address not set, cannot respond");
                    return;
                }

                std::string message = R"({"message": "HELLO", "id": 1})";
                // You can set the destination address/port here
                struct sockaddr_in dest_addr{};
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(1235); // destination port
                inet_pton(AF_INET, this->server_addr_.c_str(), &dest_addr.sin_addr);
                this->sock_->sendto(message.c_str(), message.size(), 0,
                                    reinterpret_cast<struct sockaddr *>(&dest_addr),
                                    sizeof(dest_addr));

                ESP_LOGD(TAG, "Sent HELLO to %s:1235", this->server_addr_.c_str());
            }

            void loop() override
            {
                if (!this->sock_)
                    return;

                sendPing();
                advance_frame();
                // Optional: if you compiled with select support and used socket_ip_loop_monitored,
                // you can check .ready() here. On LibreTiny you often don’t have that, so just try.
                uint8_t buf[512];

                struct sockaddr_storage src_addr{};
                socklen_t src_len = sizeof(src_addr);

                auto len = this->sock_->recvfrom(buf, sizeof(buf),
                                                 reinterpret_cast<struct sockaddr *>(&src_addr),
                                                 &src_len);
                if (len <= 0)
                {
                    // -1 with EAGAIN/EWOULDBLOCK is normal for non-blocking
                    return;
                }

                // We assume IPv4 here; you can handle IPv6 via sockaddr_in6 if needed.
                auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&src_addr);
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, sizeof(ip_str));
                uint16_t src_port = ntohs(addr_in->sin_port);

                // ESP_LOGD(TAG, "Got %d bytes from %s:%u", (int)len, ip_str, src_port);

                // try json parse

                auto doc = json::parse_json(std::string(reinterpret_cast<const char *>(buf), (size_t)len));
                auto obj = doc.as<JsonObjectConst>();
                if (obj.containsKey("message"))
                {
                    std::string message = obj["message"].as<std::string>();

                    if (message == "DISCOVER")
                    {
                        // set server ip
                        ESP_LOGI(TAG, "DISCOVER received from %s:%u", ip_str, src_port);
                        this->server_addr_ = std::string(ip_str);
                        this->total_frames_ = obj["total_frames"].as<uint32_t>();
                        ESP_LOGI(TAG, "Set server address to %s, total_frames=%u",
                                 this->server_addr_.c_str(), (unsigned)this->total_frames_);
                    }
                    else if (message == "SYNC")
                    {
                        auto time = obj["time"].as<uint32_t>();
                        auto frame = obj["frame"].as<uint32_t>();
                        ESP_LOGI(TAG, "SYNC received from %s:%u time=%u, frame=%u", ip_str, src_port, (unsigned)time, (unsigned)frame);
                        this->current_frame_ = frame;
                    }
                    else if (message == "SAMPLES")
                    {
                        // ESP_LOGI(TAG, "SAMPLES received from %s:%u", ip_str, src_port);

                        this->received_samples_n_ += doc["samples"].as<JsonArrayConst>().size();
                        if (this->received_samples_n_ % 100 == 0)
                            ESP_LOGI(TAG, "Total samples received: %u", this->received_samples_n_);

                        uint32_t last_frame_id = 0;
                        for (auto sample : doc["samples"].as<JsonArrayConst>())
                        {
                            uint16_t frame_mod = sample["frame"].as<uint16_t>();
                            uint8_t r = sample["r"].as<uint8_t>();
                            uint8_t g = sample["g"].as<uint8_t>();
                            uint8_t b = sample["b"].as<uint8_t>();

                            on_frame_received(frame_mod, r, g, b);
                            last_frame_id = frame_mod;
                        }
                        // ESP_LOGD(TAG, "Processed SAMPLES up to frame_mod=%u", (unsigned)last_frame_id);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Received message: %s", message.c_str());
                    }
                }
                else
                {
                    ESP_LOGI(TAG, "JSON does not contain 'message' key");
                }
            }

            uint32_t unwrap_frame_id(uint16_t frame_mod)
            {
                static bool initialized = false;
                static uint32_t last_abs = 0;

                if (!initialized)
                {
                    initialized = true;
                    last_abs = frame_mod;
                    return last_abs;
                }

                uint16_t last_mod = last_abs % total_frames_;

                // signed diff in "ring space"
                int32_t diff = (int32_t)frame_mod - (int32_t)last_mod;

                // normalize to (-total_frames/2, +total_frames/2]
                int32_t half = total_frames_ / 2;
                if (diff < -half)
                {
                    diff += total_frames_; // we wrapped forward
                }
                else if (diff > half)
                {
                    diff -= total_frames_; // (would be a wrap backward – rare, but whatever)
                }

                last_abs += diff;
                return last_abs;
            }

            void advance_frame()
            {
                uint32_t now = millis();
                if ((now - last_step_ms_) < frame_interval_ms_)
                    return;

                last_step_ms_ = now;

                // advance playhead
                current_frame_ = (uint16_t)((current_frame_ + 1) % total_frames_);

                // Find the slot for current_frame_ and clean up old ones
                FrameSlot *slot_for_current = nullptr;

                for (uint16_t i = 0; i < BUF_SIZE; i++)
                {
                    FrameSlot &s = buffer[i];
                    if (!s.valid)
                    {
                        // ESP_LOGD(TAG, "Slot %u is free", (unsigned)i);
                        continue;
                    }

                    if (s.frame_index == current_frame_)
                    {
                        // ESP_LOGD(TAG, "Slot %u is for current frame %u", (unsigned)i, (unsigned)current_frame_);
                        slot_for_current = &s;
                        continue;
                    }

                    // if this frame is behind the playhead, we don't need it anymore
                    if (is_behind(current_frame_, s.frame_index, total_frames_))
                    {
                        // ESP_LOGD(TAG, "Dropping old frame %u (playhead %u)",
                        //          (unsigned)s.frame_index, (unsigned)current_frame_);
                        s.valid = false; // free slot
                    }
                }

                if (slot_for_current != nullptr)
                {
                    // We have data for this frame: show it and free slot
                    apply_pixel_(slot_for_current->r, slot_for_current->g, slot_for_current->b);
                    slot_for_current->valid = false;
                }
                else
                {
                    ESP_LOGD(TAG, "No data for current frame %u", (unsigned)current_frame_);
                    // No new frame for this index → keep old color, or fade, etc.
                }
            }

            void on_frame_received(uint16_t frame_index, uint8_t r, uint8_t g, uint8_t b)
            {
                // Decide if this frame is useful: ahead of current and not too far
                uint16_t d = forward_dist(current_frame_, frame_index, total_frames_);

                if (d == 0)
                {
                    // It's "now": you can either apply immediately or let the next tick pick it up.
                    apply_pixel_(r, g, b);
                    return;
                }

                if (d >= total_frames_ / 2)
                {
                    // This means it's behind the current_frame_ (old frame from previous cycle).
                    // We don't need it.
                    return;
                }

                if (d > BUF_SIZE)
                {
                    // Too far in the future for our buffer window, just drop.
                    return;
                }

                // 2) Find a free slot
                for (uint16_t i = 0; i < BUF_SIZE; i++)
                {
                    FrameSlot &s = buffer[i];
                    if (!s.valid)
                    {
                        // ESP_LOGD(TAG, "Using free slot %u for frame %u", (unsigned)i, (unsigned)frame_index);
                        s.valid = true;
                        s.frame_index = frame_index;
                        s.r = r;
                        s.g = g;
                        s.b = b;
                        return;
                    }
                }

                // 3) No free slot: buffer full of other future frames.
                // You can either drop this one or replace the farthest future frame.
                // Simplest: drop.
            }

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

        protected:
            std::unique_ptr<socket::Socket> sock_;
            uint16_t listen_port_{1234}; // whatever port you need
            std::string server_addr_{};
            uint32_t last_ping_ms_{0};
            uint32_t received_samples_n_{0};
            uint32_t current_frame_{0};

            FrameSlot buffer_[BUF_SIZE];
            uint32_t playhead_id_{0};
            uint32_t last_step_ms_{0};
            uint32_t frame_interval_ms_{42}; // 25 fps, adjust to your video
            uint32_t total_frames_{0};

            void apply_pixel_(uint8_t r, uint8_t g, uint8_t b)
            {
                this->current_r_ = r;
                this->current_g_ = g;
                this->current_b_ = b;
                this->has_color_ = true;

                ESP_LOGD(TAG, "\033[48;2;%d;%d;%dm             \033[0m  (R=%d G=%d B=%d)",
                         (int)(r), (int)(g), (int)(b), (int)(r), (int)(g), (int)(b));
                // hook this into your ESPHome light / raw PWM / whatever
                // e.g. light->current_values = {r,g,b};
            }
        };

    } // namespace my_udp
} // namespace esphome