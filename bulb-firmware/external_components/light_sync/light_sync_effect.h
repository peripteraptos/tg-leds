#pragma once

#include "esphome/core/defines.h"

#ifdef USE_NETWORK
#ifdef USE_LIGHT

#include "light_sync.h"
#include "esphome/components/light/light_effect.h"

namespace esphome
{
    namespace light_sync
    {

        class LightSyncEffect : public light::LightEffect
        {
        public:
            explicit LightSyncEffect(const char *name);

            void set_light_sync(LightSyncComponent *comp) { this->light_sync_ = comp; }

            void start() override;
            void stop() override;
            void apply() override;

        protected:
            LightSyncComponent *light_sync_{nullptr};

            // optional: cache last color so we don't spam LightCalls
            bool have_last_{false};
            uint8_t last_r_{0};
            uint8_t last_g_{0};
            uint8_t last_b_{0};
        };

    } // namespace light_sync
} // namespace esphome

#endif // USE_NETWORK
#endif // USE_LIGHT