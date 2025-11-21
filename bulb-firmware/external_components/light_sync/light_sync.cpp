#include "light_sync.h"

#ifdef USE_NETWORK

#include <cstdlib>  // std::strtof
#include <cstring>

#ifdef USE_HOST
#include <netdb.h>
#endif

namespace esphome {
namespace light_sync {

static const char *const TAG = "light_sync";

void LightSync::setup() {
  ESP_LOGCONFIG(TAG, "LightSync: setup, host=%s port=%u id=%s",
                this->server_host_.c_str(), this->server_port_,
                this->client_id_.c_str());
  // We do NOT block here; connection happens in loop() once network is up.
}

void LightSync::dump_config() {
  ESP_LOGCONFIG(TAG, "LightSync:");
  ESP_LOGCONFIG(TAG, "  Server: %s:%u", this->server_host_.c_str(), this->server_port_);
  ESP_LOGCONFIG(TAG, "  Client ID: %s", this->client_id_.c_str());
  if (this->sequence_loaded_) {
    ESP_LOGCONFIG(TAG, "  Sequence loaded: %u samples, length=%.3fs",
                  (unsigned) this->sequence_.size(), this->total_length_s_);
  } else {
    ESP_LOGCONFIG(TAG, "  Sequence not loaded yet");
  }
}

void LightSync::loop() {
  if (network::is_disabled()) {
    return;
  }
  if (!network::is_connected()) {
    // Network not ready yet; reset state
    if (this->tcp_) {
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

void LightSync::ensure_connected_() {
  if (this->connected_) {
    return;
  }
  // Try to connect if we don't have a socket yet
  if (!this->tcp_) {
    if (!this->connect_to_server_()) {
      // Retry later
      return;
    }
  }

  // Send initial hello with client id
  this->send_hello_();
  this->connected_ = true;

  // From now on the server is expected to send:
  //   SEQLEN <seconds>
  //   SAMPLE <t> <r> <g> <b>
  //   ...
  //   SEQEND
  // followed by:
  //   SYNC <time_s>
  // messages over the same TCP connection.
}

bool LightSync::connect_to_server_() {
  ESP_LOGI(TAG, "Connecting to %s:%u ...",
           this->server_host_.c_str(), this->server_port_);

  this->tcp_ = socket::socket_ip(SOCK_STREAM, 0);
  if (!this->tcp_) {
    ESP_LOGE(TAG, "Failed to create TCP socket");
    return false;
  }

  this->tcp_->setblocking(true);

#ifdef USE_HOST
  // On host: use getaddrinfo() so hostnames and mDNS-capable resolvers work.
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *res = nullptr;
  int err = getaddrinfo(this->server_host_.c_str(), nullptr, &hints, &res);
  if (err != 0 || res == nullptr) {
    ESP_LOGE(TAG, "DNS lookup failed for %s: %s",
             this->server_host_.c_str(), gai_strerror(err));
    this->tcp_->close();
    this->tcp_.reset();
    if (res != nullptr)
      freeaddrinfo(res);
    return false;
  }

  struct sockaddr_in server_addr = *reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
  server_addr.sin_port = htons(this->server_port_);
  freeaddrinfo(res);

  int ret = this->tcp_->connect(reinterpret_cast<struct sockaddr *>(&server_addr),
                                sizeof(server_addr));
#else
  // On embedded targets (ESP), keep it simple for now:
  // - server_host_ should currently be a numeric IP string.
  // - If you want true DNS/mDNS on-device, you can adapt MQTT's
  //   dns_gethostbyname_addrtype() pattern here and call set_sockaddr()
  //   with the resolved IP string.
  struct sockaddr_storage server_addr{};
  socklen_t len =
      socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&server_addr),
                           sizeof(server_addr),
                           this->server_host_, this->server_port_);
  if (len == 0) {
    ESP_LOGE(TAG, "Invalid server IP address: %s", this->server_host_.c_str());
    this->tcp_->close();
    this->tcp_.reset();
    return false;
  }

  int ret = this->tcp_->connect(reinterpret_cast<struct sockaddr *>(&server_addr), len);
#endif

  if (ret != 0) {
    ESP_LOGE(TAG, "TCP connect() failed");
    this->tcp_->close();
    this->tcp_.reset();
    return false;
  }

  ESP_LOGI(TAG, "Connected to server");
  return true;
}

void LightSync::send_hello_() {
  if (!this->tcp_) {
    return;
  }
  std::string line = "HELLO " + this->client_id_ + "\n";
  auto written = this->tcp_->write(line.c_str(), line.size());
  if (written < 0) {
    ESP_LOGW(TAG, "Failed to send HELLO");
  } else {
    ESP_LOGD(TAG, "Sent HELLO (%d bytes)", (int) written);
  }

  // Reset sequence state, we expect a new one after this
  this->sequence_.clear();
  this->sequence_loaded_ = false;
  this->have_sync_ = false;
}

void LightSync::process_tcp_() {
  if (!this->tcp_) {
    return;
  }

  // Make the socket non-blocking-ish: read what's readily available.
  this->tcp_->setblocking(false);

  uint8_t buf[256];
  while (this->tcp_->ready()) {
    auto len = this->tcp_->read(buf, sizeof(buf));
    if (len <= 0) {
      break;  // nothing or error; we'll reconnect later if needed
    }
    this->recv_buffer_.append(reinterpret_cast<char *>(buf), (size_t) len);
  }

  // Put it back to blocking for connect/other calls (just to be safe).
  this->tcp_->setblocking(true);

  // Parse complete lines
  while (true) {
    auto pos = this->recv_buffer_.find('\n');
    if (pos == std::string::npos)
      break;
    std::string line = this->recv_buffer_.substr(0, pos);
    this->recv_buffer_.erase(0, pos + 1);
    this->process_line_(trim_(line));
  }
}

void LightSync::process_line_(const std::string &line_in) {
  if (line_in.empty())
    return;

  const std::string line = line_in;  // already trimmed

  if (line.rfind("SEQLEN ", 0) == 0) {
    const char *p = line.c_str() + 7;
    char *endptr = nullptr;
    float length = std::strtof(p, &endptr);
    if (length > 0.0f) {
      this->total_length_s_ = length;
      ESP_LOGI(TAG, "Sequence length set to %.3fs", this->total_length_s_);
    }
    return;
  }

  if (line.rfind("SAMPLE ", 0) == 0) {
    // SAMPLE <t> <r> <g> <b>
    float t = 0.0f, r = 0.0f, g = 0.0f, b = 0.0f;
    int parsed = std::sscanf(line.c_str(), "SAMPLE %f %f %f %f", &t, &r, &g, &b);
    if (parsed == 4) {
      this->sequence_.push_back(SequenceSample{t, r, g, b});
      ESP_LOGD(TAG, "Sample t=%.3f rgb=(%.3f,%.3f,%.3f)", t, r, g, b);
    }
    return;
  }

  if (line == "SEQEND") {
    this->sequence_loaded_ = !this->sequence_.empty();
    if (this->sequence_loaded_) {
      ESP_LOGI(TAG, "Sequence loaded: %u samples",
               (unsigned) this->sequence_.size());
    } else {
      ESP_LOGW(TAG, "SEQEND received but no samples");
    }
    return;
  }

  if (line.rfind("SYNC ", 0) == 0) {
    const char *p = line.c_str() + 5;
    char *endptr = nullptr;
    float t = std::strtof(p, &endptr);
    this->on_sync_time_(t);
    return;
  }

  ESP_LOGD(TAG, "Unknown line from server: '%s'", line.c_str());
}

void LightSync::on_sync_time_(float time_s) {
  ESP_LOGD(TAG, "SYNC time=%.3f", time_s);
  this->have_sync_ = true;
  this->base_sequence_time_ = time_s;
  this->base_clock_ms_ = millis();
}

void LightSync::update_playback_() {
  if (!this->sequence_loaded_ || !this->have_sync_ || this->sequence_.empty()) {
    // No sequence or no sync yet
    return;
  }

  uint32_t now = millis();
  float elapsed_s = (now - this->base_clock_ms_) / 1000.0f;
  float t = this->base_sequence_time_ + elapsed_s;

  if (this->total_length_s_ > 0.0f) {
    // Loop
    t = std::fmod(t, this->total_length_s_);
    if (t < 0.0f)
      t += this->total_length_s_;
  }

  // Find surrounding samples
  const auto &seq = this->sequence_;
  if (t <= seq.front().time_s) {
    this->set_rgb_(seq.front().r, seq.front().g, seq.front().b);
    return;
  }
  if (t >= seq.back().time_s) {
    this->set_rgb_(seq.back().r, seq.back().g, seq.back().b);
    return;
  }

  // Linear search (fine for small sequences; optimize if needed)
  size_t i = 0;
  for (; i + 1 < seq.size(); i++) {
    if (t < seq[i + 1].time_s)
      break;
  }

  const auto &a = seq[i];
  const auto &b = seq[i + 1];

  float span = b.time_s - a.time_s;
  if (span <= 0.0f) {
    this->set_rgb_(a.r, a.g, a.b);
    return;
  }

  float u = (t - a.time_s) / span;
  this->set_rgb_(a.r + (b.r - a.r) * u,
                 a.g + (b.g - a.g) * u,
                 a.b + (b.b - a.b) * u);
}

std::string LightSync::trim_(const std::string &s) {
  size_t start = 0;
  while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r'))
    ++start;
  size_t end = s.size();
  while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r'))
    --end;
  return s.substr(start, end - start);
}

void LightSync::set_outputs(output::FloatOutput *r_out, output::FloatOutput *g_out, output::FloatOutput *b_out) {
  this->r_out_ = r_out;
  this->g_out_ = g_out;
  this->b_out_ = b_out;
}

void LightSync::set_rgb_(float r, float g, float b) {
  if (!r_out_ || !g_out_ || !b_out_) return;

  auto clamp01 = [](float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
  };

  ESP_LOGD(TAG, "\033[48;2;%d;%d;%dm             \033[0m  (R=%d G=%d B=%d)",
                   (int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(r * 255), (int)(g * 255), (int)(b * 255));

  r_out_->set_level(clamp01(r));
  g_out_->set_level(clamp01(g));
  b_out_->set_level(clamp01(b));
}

}  // namespace light_sync
}  // namespace esphome

#endif  // USE_NETWORK
