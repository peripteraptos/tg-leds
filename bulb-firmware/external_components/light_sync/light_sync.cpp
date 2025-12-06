#include "light_sync.h"

#ifdef USE_NETWORK

#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include <cstdlib> // std::strtof, std::sscanf
#include <cstring>
#include <cmath>

#ifdef USE_HOST
#include <netdb.h>
#endif

namespace esphome
{
    namespace light_sync
    {

        static const char *const TAG = "light_sync";

        void LightSyncComponent::setup()
        {

            // Register sync time callback
            this->brightsign_sync_->addListener(
                [this](brightsign_sync::BrightsignSyncTime msg)
                {
                    this->on_sync_time(msg.t, msg.x);
                });
        }

        void LightSyncComponent::dump_config()
        {
            ESP_LOGCONFIG(TAG, "LightSync:");
            ESP_LOGCONFIG(TAG, "  Server: %s:%u", this->server_host_.c_str(), this->server_port_);
            ESP_LOGCONFIG(TAG, "  Client ID: %u", this->client_id_);
            if (this->sequence_loaded_)
            {
                ESP_LOGCONFIG(TAG, "  Sequence loaded: %u samples, length=%.3fs",
                              (unsigned)this->sequence_.size(), this->total_length_s_);
            }
            else
            {
                ESP_LOGCONFIG(TAG, "  Sequence not loaded yet");
            }
        }

        void LightSyncComponent::loop()
        {
            if (last_reconnect_attempt_ms_ != 0 && millis() - last_reconnect_attempt_ms_ < 3000)
            {
                // wait 3 seconds between reconnect attempts
                return;
            }

            if (network::is_disabled())
                return;

            if (!network::is_connected())
            {
                // Network not ready yet; reset state
                if (this->tcp_)
                {
                    this->tcp_->close();
                    this->tcp_.reset();
                }
                this->connected_ = false;
                return;
            }

            // Make sure we have a TCP connection and handshaked
            this->ensure_connected_();

            // Process any incoming data from the server (including SYNC lines)
            this->process_tcp_();

            // Update local playback clock and current RGB
            this->update_playback_();
        }

        void LightSyncComponent::ensure_connected_()
        {
            if (this->connected_)
                return;

            // Try to connect if we don't have a socket yet
            if (!this->tcp_)
            {
                if (!this->connect_to_server_())
                {
                    // Retry later
                    return;
                }
            }

            // Send initial hello with client id
            this->send_hello_();
            this->connected_ = true;

            // Reset sequence state, we expect a new one after this
            // this->sequence_.clear();
            // this->sequence_loaded_ = false;
            // this->have_sync_ = false;
            // this->has_color_ = false;
        }

        bool LightSyncComponent::connect_to_server_()
        {
            ESP_LOGI(TAG, "Connecting to %s:%u ...",
                     this->server_host_.c_str(), this->server_port_);

            this->tcp_ = socket::socket_ip(SOCK_STREAM, 0);
            if (!this->tcp_)
            {
                ESP_LOGE(TAG, "Failed to create TCP socket");
                return false;
            }

            this->tcp_->setblocking(true);

            // On embedded targets (ESP), keep it simple for now:
            // - server_host_ should currently be a numeric IP string.
            struct sockaddr_storage server_addr{};
            socklen_t len =
                socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&server_addr),
                                     sizeof(server_addr),
                                     this->server_host_, this->server_port_);
            if (len == 0)
            {
                ESP_LOGE(TAG, "Invalid server IP address: %s", this->server_host_.c_str());
                this->tcp_->close();
                this->tcp_.reset();
                return false;
            }

            int ret = this->tcp_->connect(reinterpret_cast<struct sockaddr *>(&server_addr), len);

            if (ret != 0)
            {
                ESP_LOGE(TAG, "TCP connect() failed, errno=%d", errno);
                this->tcp_->close();
                this->tcp_.reset();

                this->reconnect_attempts_++;
                this->last_reconnect_attempt_ms_ = millis();
                return false;
            }

            ESP_LOGI(TAG, "Connected to server");
            return true;
        }

        void LightSyncComponent::send_hello_()
        {
            if (!this->tcp_)
                return;

            std::string line = "HELLO " + std::to_string(this->client_id_) + "\n";
            auto written = this->tcp_->write(line.c_str(), line.size());
            if (written < 0)
            {
                ESP_LOGW(TAG, "Failed to send HELLO");
            }
            else
            {
                ESP_LOGD(TAG, "Sent HELLO (%d bytes)", (int)written);
            }

            // Reset sequence state, we expect a new one after this
            this->sequence_.clear();
            this->sequence_loaded_ = false;
            this->have_sync_ = false;
            this->has_color_ = false;
        }

        void LightSyncComponent::process_tcp_()
        {
            if (!this->tcp_)
                return;

            // Make the socket non-blocking-ish: read what's readily available.
            this->tcp_->setblocking(false);

            uint8_t buf[256];
            size_t total_read = 0;
            while (this->tcp_->ready())
            {
                auto len = this->tcp_->read(buf, sizeof(buf));
                if (len <= 0)
                    break; // nothing or error; we'll reconnect later if needed
                this->recv_buffer_.append(reinterpret_cast<char *>(buf), (size_t)len);
            }

            // Put it back to blocking for connect/other calls (just to be safe).
            this->tcp_->setblocking(true);

            // Parse complete lines
            while (true)
            {
                auto pos = this->recv_buffer_.find('\n');
                if (pos == std::string::npos)
                    break;
                std::string line = this->recv_buffer_.substr(0, pos);
                this->recv_buffer_.erase(0, pos + 1);
                this->process_line_(trim_(line));
            }
        }

        void LightSyncComponent::process_line_(const std::string &line_in)
        {
            if (line_in.empty())
                return;

            const std::string line = trim_(line_in);

            ESP_LOGD(TAG, "Line from server: '%s'", line.c_str());

            // --- SEQLEN <seconds> ---
            if (line.rfind("SEQLEN", 0) == 0)
            {
                const char *p = line.c_str() + 6; // skip "SEQLEN"
                while (*p == ' ' || *p == '\t')
                    ++p;

                char *endptr = nullptr;
                float length = std::strtof(p, &endptr);
                if (endptr == p || !std::isfinite(length))
                {
                    ESP_LOGW(TAG, "SEQLEN parse failed: '%s'", line.c_str());
                    return;
                }

                if (length > 0.0f)
                {
                    this->total_length_s_ = length;
                    this->total_length_ms_ = static_cast<uint32_t>(length);

                    ESP_LOGI(TAG, "Sequence length set to %.3fs (%u ms)",
                             this->total_length_s_, (unsigned)this->total_length_ms_);
                }
                else
                {
                    ESP_LOGW(TAG, "SEQLEN length <= 0: %.3f", length);
                }
                return;
            }

            // --- SAMPLE <t_ms> <r> <g> <b> ---
            if (line.rfind("SAMPLE", 0) == 0)
            {
                uint32_t r_i = 0, g_i = 0, b_i = 0;

                // "SAMPLE 281300 223 222 241"
                int parsed = std::sscanf(line.c_str(), "SAMPLE %u %u %u",
                                         &r_i, &g_i, &b_i);
                if (parsed != 3)
                {
                    ESP_LOGW(TAG, "SAMPLE parse failed (%d fields): '%s'", parsed, line.c_str());
                    return;
                }

                auto clamp255 = [](int v) -> uint8_t
                {
                    if (v < 0)
                        v = 0;
                    if (v > 255)
                        v = 255;
                    return static_cast<uint8_t>(v);
                };

                SequenceSample s{
                    clamp255(r_i),
                    clamp255(g_i),
                    clamp255(b_i),
                };

                this->sequence_.push_back(s);
                this->sequence_loaded_ = true;

                ESP_LOGD(TAG, "Sample rgb=(%d,%d,%d)",
                         (int)s.r, (int)s.g, (int)s.b);
                return;
            }

            // --- SEQEND ---
            if (line == "SEQEND")
            {
                if (this->sequence_.empty())
                {
                    ESP_LOGW(TAG, "SEQEND received but no samples");
                }
                else
                {
                    ESP_LOGI(TAG, "Sequence finished: %u samples",
                             (unsigned)this->sequence_.size());
                    // sequence_loaded_ already set when first sample arrived
                }
                return;
            }

            ESP_LOGD(TAG, "Unknown line from server: '%s'", line.c_str());
        }

        void LightSyncComponent::on_sync_time(uint32_t now_ms, uint32_t start_time_ms)
        {
            auto time_ms = now_ms - start_time_ms;
            if (time_ms > this->total_length_ms_ && this->total_length_ms_ > 0)
            {
                time_ms %= this->total_length_ms_;
            }
            ESP_LOGI(TAG, "SYNC time=%u ms", (unsigned)time_ms);
            this->have_sync_ = true;
            this->base_sequence_time_ms_ = time_ms;
            this->base_clock_ms_ = millis();
        }

        void LightSyncComponent::update_playback_()
        {
            if (!this->sequence_loaded_ || !this->have_sync_ || this->sequence_.empty())
                return;

            uint32_t now_ms = millis();
            uint32_t elapsed_ms = now_ms - this->base_clock_ms_;
            uint32_t t_ms = this->base_sequence_time_ms_ + elapsed_ms;

            if (elapsed_ms < (1000.0f / this->fps_))
                return; // no need to update yet

            uint32_t frame_duration_ms = static_cast<uint32_t>(1000.0f / this->fps_);
            uint32_t total_frames = static_cast<uint32_t>(this->total_length_ms_ / frame_duration_ms);
            if (total_frames == 0)
                total_frames = 1;

            uint8_t r = 0, g = 0, b = 0;
            uint32_t frame_index = (t_ms / frame_duration_ms) % total_frames;
            if (frame_index >= this->sequence_.size())
                frame_index = static_cast<uint32_t>(this->sequence_.size() - 1);
            const SequenceSample &sample = this->sequence_[frame_index];

            ESP_LOGD(TAG, "Playback t_ms=%u frame_index=%u", (unsigned)t_ms, (unsigned)frame_index);

            this->current_r_ = sample.r;
            this->current_g_ = sample.g;
            this->current_b_ = sample.b;
            this->has_color_ = true;
        }

        std::string LightSyncComponent::trim_(const std::string &s)
        {
            size_t start = 0;
            while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r'))
                ++start;
            size_t end = s.size();
            while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r'))
                --end;
            return s.substr(start, end - start);
        }

    } // namespace light_sync
} // namespace esphome

#endif // USE_NETWORK
