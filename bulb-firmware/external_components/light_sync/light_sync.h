#pragma once
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_NETWORK

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/util.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/output/float_output.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace light_sync {

struct SequenceSample {
  float time_s;
  float r;
  float g;
  float b;
};

class LightSync : public Component {
 public:
  void set_server_host(const std::string &host) { server_host_ = host; }
  void set_server_port(uint16_t port) { server_port_ = port; }
  void set_client_id(const std::string &id) { client_id_ = id; }
  void set_light_id(const std::string &id) { light_id_ = id; }
  void set_outputs(output::FloatOutput *r_out, output::FloatOutput *g_out, output::FloatOutput *b_out);

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  // Connection + protocol helpers
  void ensure_connected_();
  bool connect_to_server_();
  void send_hello_();
  void process_tcp_();
  void process_line_(const std::string &line);
  void on_sync_time_(float time_s);
  void update_playback_();

  static std::string trim_(const std::string &s);

  std::string server_host_;
  uint16_t server_port_{4242};
  std::string client_id_;
  std::string light_id_;

  std::unique_ptr<socket::Socket> tcp_;

  bool connected_{false};
  bool sequence_loaded_{false};
  bool have_sync_{false};

  std::vector<SequenceSample> sequence_;
  float total_length_s_{0.0f};

  // playback timer (sequence time = base_sequence_time_ + (now - base_clock_ms_)/1000)
  float base_sequence_time_{0.0f};
  uint32_t base_clock_ms_{0};

  // Outputs
  output::FloatOutput *r_out_{nullptr};
  output::FloatOutput *g_out_{nullptr};
  output::FloatOutput *b_out_{nullptr};
  void set_rgb_(float r, float g, float b);

  // TCP receive buffer for line-based protocol
  std::string recv_buffer_;
};

}  // namespace light_sync
}  // namespace esphome

#endif  // USE_NETWORK
