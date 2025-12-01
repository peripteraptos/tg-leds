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

        class BrightsignSyncComponent : public udp::UDPComponent
        {
        public:
            BrightsignSyncComponent() = default;

            void set_t(std::string t) { t_ = t; }
            void set_x(std::string x) { x_ = x; }

            void setup() override;

        protected:
            void handle_packet_(std::vector<uint8_t> &buf);

            std::string t_;
            std::string x_;
        };

    } // namespace brightsign_sync
} // namespace esphome
