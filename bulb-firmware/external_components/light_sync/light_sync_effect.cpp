#include "light_sync_effect.h"

#ifdef USE_NETWORK

#include "esphome/core/log.h"
#include "esphome/components/light/light_state.h"

namespace esphome
{
    namespace light_sync
    {

        static const char *const TAG = "light_sync_effect";

        LightSyncEffect::LightSyncEffect(const char *name)
            : light::LightEffect(name) {}

        void LightSyncEffect::start()
        {
            ESP_LOGD(TAG, "LightSyncEffect '%s' start", this->get_name());
            this->have_last_ = false;
        }

        void LightSyncEffect::stop()
        {
            ESP_LOGD(TAG, "LightSyncEffect '%s' stop", this->get_name());
        }

        void LightSyncEffect::apply()
        {
            if (this->state_ == nullptr || this->light_sync_ == nullptr)
                return;

            uint8_t r, g, b;
            if (!this->light_sync_->get_current_rgb(r, g, b))
            {
                // no valid color yet
                return;
            }

            this->last_r_ = r;
            this->last_g_ = g;
            this->last_b_ = b;
            this->have_last_ = true;

            auto call = this->state_->make_call();
            call.set_state(true);
            call.set_transition_length(0);
            call.set_rgb(r / 255.0, g / 255.0, b / 255.0);
            call.perform();
        }

    } // namespace light_sync
} // namespace esphome

#endif // USE_NETWORK
