#include "sync_sequence_light.h"

#include <algorithm>

namespace esphome {
namespace sync_sequence_light {

using namespace esphome;

void SyncSequenceLight::setup() {
  // Generate default unique_id if none was set
  if (unique_id_.empty()) {
    unique_id_ = get_mac_address();
  }

  ESP_LOGI("sync_seq", "Unique ID: %s", unique_id_.c_str());

  // Start listening for sync packets
  if (sync_udp_.begin(sync_port_)) {
    ESP_LOGI("sync_seq", "Listening for sync packets on UDP port %d", sync_port_);
  } else {
    ESP_LOGW("sync_seq", "Failed to open UDP port %d for sync", sync_port_);
  }
}

void SyncSequenceLight::loop() {
  // Wait for network
  if (!network::is_connected()) {
    return;
  }

  // Resolve server if needed
  if (!server_resolved_) {
    try_resolve_server_();
  }

  // Connect to server if resolved but not connected
  if (server_resolved_ && !server_connected_) {
    try_connect_server_();
  }

  // Download sequence once connected
  if (server_connected_ && !sequence_loaded_) {
    handle_sequence_download_();
  }

  // Handle sync + playback when sequence is ready
  if (sequence_loaded_) {
    handle_sync_packets_();
    update_playback_();
  }
}

void SyncSequenceLight::try_resolve_server_() {
  if (server_host_.empty())
    return;

  ESP_LOGD("sync_seq", "Resolving server host %s", server_host_.c_str());

  if (WiFi.hostByName(server_host_.c_str(), server_ip_)) {
    server_resolved_ = true;
    ESP_LOGI("sync_seq", "Resolved %s to %s", server_host_.c_str(), server_ip_.toString().c_str());
  } else {
    // Try again next loop
  }
}

void SyncSequenceLight::try_connect_server_() {
  ESP_LOGI("sync_seq", "Connecting to %s:%d", server_ip_.toString().c_str(), server_port_);

  if (client_.connect(server_ip_, server_port_)) {
    server_connected_ = true;
    ESP_LOGI("sync_seq", "Connected to sequence server");

    // Simple text protocol: send ID line: "ID <unique_id>\n"
    client_.print("ID ");
    client_.println(unique_id_.c_str());
  } else {
    ESP_LOGW("sync_seq", "Failed to connect to sequence server");
  }
}

void SyncSequenceLight::handle_sequence_download_() {
  // Very simple line-based protocol:
  //   TOTAL <ms>
  //   <time_ms> <r> <g> <b>
  //   ...
  //   (blank line or connection close ends the sequence)
  static bool have_total = false;

  while (client_.available()) {
    String line = client_.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      // Empty line = end of sequence
      if (!sequence_.empty() && total_length_ms_ > 0) {
        sequence_loaded_ = true;
        ESP_LOGI("sync_seq", "Sequence loaded: %d frames, total %u ms",
                 (int) sequence_.size(), (unsigned) total_length_ms_);
      }
      return;
    }

    if (!have_total) {
      if (line.startsWith("TOTAL")) {
        // "TOTAL <ms>"
        line.remove(0, 5);  // remove "TOTAL"
        line.trim();
        total_length_ms_ = line.toInt();
        have_total = true;
        ESP_LOGI("sync_seq", "Got total_length_ms = %u", (unsigned) total_length_ms_);
      }
      continue;
    }

    // Parse frame line: "<time_ms> <r> <g> <b>"
    Frame f{};
    int first_space = line.indexOf(' ');
    if (first_space < 0) continue;

    String t_str = line.substring(0, first_space);
    f.t_ms = t_str.toInt();

    line = line.substring(first_space + 1);
    line.trim();

    int s1 = line.indexOf(' ');
    int s2 = line.lastIndexOf(' ');

    if (s1 < 0 || s2 <= s1) continue;

    f.r = line.substring(0, s1).toFloat();
    f.g = line.substring(s1 + 1, s2).toFloat();
    f.b = line.substring(s2 + 1).toFloat();

    sequence_.push_back(f);
  }

  // If server closed connection, also mark sequence done
  if (!client_.connected() && !sequence_loaded_ && total_length_ms_ > 0 && !sequence_.empty()) {
    sequence_loaded_ = true;
    ESP_LOGI("sync_seq", "Sequence loaded (on disconnect): %d frames, total %u ms",
             (int) sequence_.size(), (unsigned) total_length_ms_);
  }
}

void SyncSequenceLight::handle_sync_packets_() {
  int packetSize = sync_udp_.parsePacket();
  if (packetSize <= 0) return;

  char buf[32];
  int len = sync_udp_.read(buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = '\0';

  uint32_t remote_ms = (uint32_t) strtoul(buf, nullptr, 10);
  if (total_length_ms_ == 0) return;

  remote_ms %= total_length_ms_;
  uint32_t now = millis();
  sequence_start_ms_ = now - remote_ms;
  have_sync_ = true;

  ESP_LOGD("sync_seq", "Sync packet: remote_ms=%u, sequence_start_ms=%u",
           (unsigned) remote_ms, (unsigned) sequence_start_ms_);
}

void SyncSequenceLight::update_playback_() {
  if (!have_sync_ || !sequence_loaded_ || sequence_.empty() || total_length_ms_ == 0)
    return;

  uint32_t now = millis();
  uint32_t elapsed = now - sequence_start_ms_;
  uint32_t playback_ms = elapsed % total_length_ms_;

  // Find last frame with t_ms <= playback_ms
  Frame current = sequence_.front();
  for (const auto &f : sequence_) {
    if (f.t_ms <= playback_ms) {
      current = f;
    } else {
      break;
    }
  }

  set_rgb_(current.r, current.g, current.b);
}

void SyncSequenceLight::set_rgb_(float r, float g, float b) {
  if (!r_out_ || !g_out_ || !b_out_) return;

  auto clamp01 = [](float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
  };

  r_out_->set_level(clamp01(r));
  g_out_->set_level(clamp01(g));
  b_out_->set_level(clamp01(b));
}

}  // namespace sync_sequence_light
}  // namespace esphome
