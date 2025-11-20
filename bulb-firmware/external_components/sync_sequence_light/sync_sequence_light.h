#pragma once

#include "esphome.h"
#include <vector>

namespace esphome {
namespace sync_sequence_light {

using namespace esphome;

class SyncSequenceLight : public Component {
 public:
  void setup() override;
  void loop() override;

  void set_unique_id(const std::string &id) { unique_id_ = id; }
  void set_server_host(const std::string &host) { server_host_ = host; }
  void set_server_port(uint16_t port) { server_port_ = port; }
  void set_sync_port(uint16_t port) { sync_port_ = port; }

  void set_outputs(output::FloatOutput *r, output::FloatOutput *g, output::FloatOutput *b) {
    r_out_ = r;
    g_out_ = g;
    b_out_ = b;
  }

 protected:
  struct Frame {
    uint32_t t_ms;
    float r;
    float g;
    float b;
  };

  // Sequence data
  std::vector<Frame> sequence_;
  uint32_t total_length_ms_{0};
  bool sequence_loaded_{false};

  // Identity / server
  std::string unique_id_;
  std::string server_host_{"light-sequencer.local"};
  uint16_t server_port_{9000};
  uint16_t sync_port_{9001};
  IPAddress server_ip_;
  bool server_resolved_{false};
  bool server_connected_{false};

  WiFiClient client_;
  WiFiUDP sync_udp_;

  // Sync handling
  bool have_sync_{false};
  uint32_t sequence_start_ms_{0};  // millis() that corresponds to sequence time 0

  // Outputs
  output::FloatOutput *r_out_{nullptr};
  output::FloatOutput *g_out_{nullptr};
  output::FloatOutput *b_out_{nullptr};

  // Internal helpers
  void try_resolve_server_();
  void try_connect_server_();
  void handle_sequence_download_();
  void handle_sync_packets_();
  void update_playback_();
  void set_rgb_(float r, float g, float b);
};

}  // namespace sync_sequence_light
}  // namespace esphome
