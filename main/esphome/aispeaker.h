#pragma once

#include "esphome/components/speaker/speaker.h"
#include "audio_service.h"
#include <esp_log.h>

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace esphome {
namespace speaker {

class AISpeaker : public Speaker {
public:
    explicit AISpeaker(AudioService* audio_service) : audio_service_(audio_service) {
        // 同步 AudioService 的音频参数
        if (audio_service_ && audio_service_->IsIdle()) {
            auto codec = Board::GetInstance().GetAudioCodec();
            this->audio_stream_info_ = audio::AudioStreamInfo(
                16,                     // 16位 PCM
                codec->output_channels(),// 声道数
                codec->output_sample_rate() // 采样率
            );
        }
    }

    // ========== 实现 Speaker 纯虚函数 ==========
    size_t play(const uint8_t *data, size_t length) override {
        if (!audio_service_ || !this->is_running()) {
            return 0;
        }
        return audio_service_->PlayRemoteAudio(data, length);
    }

#ifdef USE_ESP32
    size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override  {
        if (!audio_service_ || !this->is_running()) {
            return 0;
        }
        return audio_service_->PlayRemoteAudio(data, length, ticks_to_wait);
    }
#endif

    void start() override {
        if (!audio_service_) {
            return;
        }
        this->state_ = STATE_STARTING;
        audio_service_->EnableRemotePlay(true);
        this->state_ = STATE_RUNNING;
        ESP_LOGI(TAG, "AI Speaker started (remote playback mode)");
    }

    void stop() override {
        if (!audio_service_) {
            return;
        }
        this->state_ = STATE_STOPPING;
        audio_service_->StopRemotePlay();
        this->state_ = STATE_STOPPED;
        ESP_LOGI(TAG, "AI Speaker stopped (remote playback mode)");
    }

    void finish() override {
        if (!audio_service_ || !this->is_running()) {
            return;
        }
        this->state_ = STATE_STOPPING;
        audio_service_->FinishRemotePlay();
        this->state_ = STATE_STOPPED;
        ESP_LOGI(TAG, "AI Speaker finished (remote playback mode)");
    }

    bool has_buffered_data() const override {
        return audio_service_ ? audio_service_->HasRemoteBufferedData() : false;
    }

    // ========== 可选：音量/静音控制（需 AudioCodec 支持） ==========
    void set_volume(float volume) override {
        Speaker::set_volume(volume); // 调用父类方法（更新 volume_）
        if (audio_service_) {
            auto codec = Board::GetInstance().GetAudioCodec();
            codec->SetOutputVolume((int)volume); // 需 AudioCodec 实现 SetVolume 接口
        }
    }

    inline static const char* TAG = "AISpeaker";

private:
    AudioService* audio_service_ = nullptr;
};

}  // namespace speaker
}  // namespace esphome