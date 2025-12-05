// brightsign_sync/brightsign_sync.h
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/udp/udp_component.h"
#include <ArduinoJson.h>
#include <vector>
#include <string>

namespace esphome
{
    namespace brightsign_sync
    {
        struct BrightsignSyncTime
        {
            uint32_t t;
            uint32_t x;
        };

        class BrightsignSyncComponent : public Component, public Parented<udp::UDPComponent>
        {
        public:
            BrightsignSyncComponent() = default;

            void addListener(
                std::function<void(BrightsignSyncTime)> &&callback)
            {
                this->on_msg_t_callback_.add(std::move(callback));
            }
            void setup() override;

            static uint32_t ParseISO8601(const std::string &input);
            static int parse_int_range(const std::string &s, size_t pos, size_t len);
            float get_setup_priority() const override;

        protected:
            void handle_packet_(std::vector<uint8_t> &buf);
            CallbackManager<void(BrightsignSyncTime)> on_msg_t_callback_;
        };

    } // namespace brightsign_sync
} // namespace esphome
