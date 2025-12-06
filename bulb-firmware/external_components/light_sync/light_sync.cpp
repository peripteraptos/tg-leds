#include "light_sync.h"

#ifdef USE_NETWORK

namespace esphome
{
    namespace light_sync
    {

        static const char *const TAG = "light_sync";

        void LightSyncComponent::setup()
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

            // this->parent_->add_listener([this](std::vector<uint8_t> &buf)
            //                             { this->on_packet_received(buf); });

            // this->parent_->set_listen_port(1234);
            // this->parent_->set_broadcast_port(1235);
            // this->parent_->set_should_broadcast();
            // this->parent_->set_should_listen();
            // this->parent_->setup();
        }

        void LightSyncComponent::send_ping_()
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

            // build message doc as string
            std::string message = R"({"message": "HELLO", "id": )" + std::to_string(this->client_id_) + "}";

            // You can set the destination address/port here
            // this->parent_->send_packet(
            //     reinterpret_cast<const uint8_t *>(message.c_str()),
            //     strlen(message.c_str()));

            struct sockaddr_in dest_addr{};
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(1235); // destination port
            // no inet_pton does not exist on libretiny!
            // inet_pton(AF_INET, this->server_addr_.c_str(), &dest_addr.sin_addr);
            dest_addr.sin_addr.s_addr = inet_addr(this->server_addr_.c_str());
            this->sock_->sendto(message.c_str(), message.size(), 0,
                                reinterpret_cast<struct sockaddr *>(&dest_addr),
                                sizeof(dest_addr));

            ESP_LOGD(TAG, "Sent HELLO to %s:1235", this->server_addr_.c_str());
        }

        void LightSyncComponent::loop()
        {

            send_ping_();
            update_playback_();
            handle_packet_();
        }
        void LightSyncComponent::handle_packet_()
        {
            if (!this->sock_)
                return;

            // Optional: if you compiled with select support and used socket_ip_loop_monitored,
            // you can check .ready() here. On LibreTiny you often don’t have that, so just try.
            uint8_t buf[512];

            auto len = this->sock_->read(buf, sizeof(buf));
            if (len <= 0)
            {
                // -1 with EAGAIN/EWOULDBLOCK is normal for non-blocking
                return;
            }

            // try json parse

            auto doc = json::parse_json(std::string(reinterpret_cast<const char *>(buf), (size_t)len));
            auto obj = doc.as<JsonObjectConst>();
            if (obj["message"].is<std::string>())
            {
                std::string message = obj["message"].as<std::string>();

                if (message == "DISCOVER")
                {
                    // set server ip
                    ESP_LOGD(TAG, "DISCOVER received");
                    if (this->server_addr_.empty())
                    {

                        ESP_LOGI(TAG, "Set server address to %s",
                                 this->server_addr_.c_str());
                        this->server_addr_ = obj["ip"].as<std::string>();
                        // this->parent_->add_address(this->server_addr_.c_str());
                    }
                    this->server_addr_ = obj["ip"].as<std::string>();
                    this->total_frames_ = obj["total_frames"];
                }
                else if (message == "SYNC")
                {
                    auto time = obj["time"];
                    auto frame = obj["frame"];
                    ESP_LOGI(TAG, "SYNC received time=%u, frame=%u", (unsigned)time, (unsigned)frame);
                    this->current_frame_ = frame;
                }
                else if (message == "SAMPLES")
                {

                    this->received_samples_n_++;
                    if (this->received_samples_n_ % 100 == 0)
                        ESP_LOGI(TAG, "Total samples received: %u", this->received_samples_n_);

                    uint16_t frame_mod = doc["f"];
                    uint8_t r = doc["r"];
                    uint8_t g = doc["g"];
                    uint8_t b = doc["b"];
                    on_frame_received_(frame_mod, r, g, b);
                }
                else
                {
                    ESP_LOGI(TAG, "Received message: %s", message.c_str());
                }
            }
            else
            {
                // ESP_LOGI(TAG, "JSON does not contain 'message' key");
            }
        }

        uint32_t LightSyncComponent::unwrap_frame_id_(uint16_t frame_mod)
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

        void LightSyncComponent::update_playback_()
        {
            uint32_t now = millis();
            if ((now - last_step_ms_) < frame_interval_ms_)
                return;

            last_step_ms_ = now;

            // advance playhead
            current_frame_ = (uint16_t)((current_frame_ + 1) % total_frames_);

            // Find the slot for current_frame_ and clean up old ones
            SequenceFrame *slot_for_current = nullptr;

            for (uint16_t i = 0; i < BUF_SIZE; i++)
            {
                SequenceFrame &s = buffer_[i];
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
                // ESP_LOGD(TAG, "No data for current frame %u", (unsigned)current_frame_);
                // No new frame for this index → keep old color, or fade, etc.
            }
        }

        void LightSyncComponent::on_frame_received_(uint16_t frame_index, uint8_t r, uint8_t g, uint8_t b)
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
                // ESP_LOGD(TAG, "Dropping old frame %u (playhead %u)",
                //          (unsigned)frame_index, (unsigned)current_frame_);
                return;
            }

            if (d > BUF_SIZE)
            {
                // Too far in the future for our buffer window, just drop.
                // ESP_LOGD(TAG, "Dropping frame %u too far in the future (playhead %u)",
                //          (unsigned)frame_index, (unsigned)current_frame_);
                return;
            }

            // 2) Find a free slot
            for (uint16_t i = 0; i < BUF_SIZE; i++)
            {
                SequenceFrame &s = buffer_[i];
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

            // ESP_LOGD(TAG, "No free slot for frame %u: buffer full", (unsigned)frame_index);
            // 3) No free slot: buffer full of other future frames.
            // You can either drop this one or replace the farthest future frame.
            // Simplest: drop.
        }
        void LightSyncComponent::apply_pixel_(uint8_t r, uint8_t g, uint8_t b)
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

    } // namespace light_sync
} // namespace esphome

#endif // USE_NETWORK
