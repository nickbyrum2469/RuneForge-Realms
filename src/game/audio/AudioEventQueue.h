#pragma once

#include "game/audio/AudioEvent.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rf::game::audio {

class AudioEventQueue {
public:
    static constexpr std::size_t maxQueuedEvents = 256;

    void emit(AudioEvent event) noexcept;
    void emitBlock(AudioEventType type, world::blocks::SoundFamily family, Vec3 position,
                   float gain = 1.0f, bool spatial = true) noexcept;

    [[nodiscard]] std::vector<AudioEvent> drain();
    [[nodiscard]] const std::vector<AudioEvent>& events() const noexcept { return events_; }
    [[nodiscard]] std::size_t size() const noexcept { return events_.size(); }
    void clear() noexcept { events_.clear(); }

private:
    [[nodiscard]] static std::uint32_t mix(std::uint32_t value) noexcept;
    [[nodiscard]] static float signedUnit(std::uint32_t value) noexcept;

    std::vector<AudioEvent> events_;
    std::uint32_t sequence_{1};
};

} // namespace rf::game::audio
