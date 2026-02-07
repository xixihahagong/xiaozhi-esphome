#include "audio_codec.h"

#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/FreeRTOS.h>

#include "esphome/components/audio/audio.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/ring_buffer.h"

namespace esphome {
namespace aispeaker {

class AISpeaker : public speaker::Speaker, public Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::PROCESSOR; }

  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }
  void set_timeout(uint32_t ms) { this->timeout_ = ms; }

  void start() override;
  void stop() override;
  void finish() override;

  void set_pause_state(bool pause_state) override { this->pause_state_ = pause_state; }
  bool get_pause_state() const override { return this->pause_state_; }

  /// @brief Plays the provided audio data.
  /// Starts the speaker task, if necessary. Writes the audio data to the ring buffer.
  /// @param data Audio data in the format set by the parent speaker classes ``set_audio_stream_info`` method.
  /// @param length The length of the audio data in bytes.
  /// @param ticks_to_wait The FreeRTOS ticks to wait before writing as much data as possible to the ring buffer.
  /// @return The number of bytes that were actually written to the ring buffer.
  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  size_t play(const uint8_t *data, size_t length) override { return play(data, length, 0); }

  bool has_buffered_data() const override;

  /// @brief Sets the volume of the speaker. Uses the speaker's configured audio dac component. If unavailble, it is
  /// implemented as a software volume control. Overrides the default setter to convert the floating point volume to a
  /// Q15 fixed-point factor.
  /// @param volume between 0.0 and 1.0
  void set_volume(float volume) override;

  /// @brief Mutes or unmute the speaker. Uses the speaker's configured audio dac component. If unavailble, it is
  /// implemented as a software volume control. Overrides the default setter to convert the floating point volume to a
  /// Q15 fixed-point factor.
  /// @param mute_state true for muting, false for unmuting
  void set_mute_state(bool mute_state) override;

  /// @brief Set audio_codec.
  /// @param codec AudioCodec.
  void set_codec_(AudioCodec* codec){this->codec_ = codec;};

 protected:
  /// @brief Function for the FreeRTOS task handling audio output.
  /// After receiving the COMMAND_START signal, allocates space for the buffers, starts the I2S driver, and reads
  /// audio from the ring buffer and writes audio to the I2S port. Stops immmiately after receiving the COMMAND_STOP
  /// signal and stops only after the ring buffer is empty after receiving the COMMAND_STOP_GRACEFULLY signal. Stops if
  /// the ring buffer hasn't read data for more than timeout_ milliseconds. When stopping, it deallocates the buffers,
  /// stops the I2S driver, unlocks the I2S port, and deletes the task. It communicates the state and any errors via
  /// event_group_.
  /// @param params I2SAudioSpeaker component
  static void speaker_task(void *params);

  /// @brief Sends a stop command to the speaker task via event_group_.
  /// @param wait_on_empty If false, sends the COMMAND_STOP signal. If true, sends the COMMAND_STOP_GRACEFULLY signal.
  void stop_(bool wait_on_empty);

  /// @brief Sets the corresponding ERR_ESP event group bits.
  /// @param err esp_err_t error code.
  /// @return True if an ERR_ESP bit is set and false if err == ESP_OK
  bool send_esp_err_to_event_group_(esp_err_t err);

  /// @brief Allocates the data buffer and ring buffer
  /// @param data_buffer_size Number of bytes to allocate for the data buffer.
  /// @param ring_buffer_size Number of bytes to allocate for the ring buffer.
  /// @return ESP_ERR_NO_MEM if either buffer fails to allocate
  ///         ESP_OK if successful
  esp_err_t allocate_buffers_(size_t data_buffer_size, size_t ring_buffer_size);

  /// @brief Deletes the speaker's task.
  /// Deallocates the data_buffer_ and audio_ring_buffer_, if necessary, and deletes the task. Should only be called by
  /// the speaker_task itself.
  /// @param buffer_size The allocated size of the data_buffer_.
  void delete_task_(size_t buffer_size);

  TaskHandle_t speaker_task_handle_{nullptr};
  EventGroupHandle_t event_group_{nullptr};

  uint8_t *data_buffer_;
  std::shared_ptr<RingBuffer> audio_ring_buffer_;

  uint32_t buffer_duration_ms_;

  optional<uint32_t> timeout_;

  bool task_created_{false};
  bool pause_state_{false};

  int16_t q15_volume_factor_{INT16_MAX};

  size_t bytes_written_{0};

  uint32_t accumulated_frames_written_{0};
  AudioCodec *codec_{nullptr};
};

}  // namespace aispeaker
}  // namespace esphome
