#include "game/audio/AudioEventQueue.h"

#include <algorithm>
#include <utility>

namespace rf::game::audio {

std::uint32_t AudioEventQueue::mix(std::uint32_t value) noexcept {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float AudioEventQueue::signedUnit(std::uint32_t value) noexcept {
    return static_cast<float>(value & 0xffffu) / 32767.5f - 1.0f;
}

void AudioEventQueue::emit(AudioEvent event) noexcept {
    if (events_.size() >= maxQueuedEvents) return;
    event.gain = std::clamp(event.gain, 0.0f, 2.0f);
    event.pitch = std::clamp(event.pitch, 0.65f, 1.45f);
    if (event.variationSeed == 0) event.variationSeed = mix(sequence_++);
    events_.push_back(event);
}

void AudioEventQueue::emitBlock(AudioEventType type, world::blocks::SoundFamily family,
                                Vec3 position, float gain, bool spatial) noexcept {
    const std::uint32_t seed = mix(sequence_++ ^ (static_cast<std::uint32_t>(type) << 8u) ^
                                   (static_cast<std::uint32_t>(family) << 17u));
    AudioEvent event;
    event.type = type;
    event.family = family;
    event.position = position;
    event.gain = gain * (0.94f + signedUnit(seed >> 7u) * 0.055f);
    event.pitch = 1.0f + signedUnit(seed >> 13u) * 0.055f;
    event.spatial = spatial;
    event.variationSeed = seed;
    emit(event);
}

std::vector<AudioEvent> AudioEventQueue::drain() {
    std::vector<AudioEvent> result;
    result.swap(events_);
    return result;
}

} // namespace rf::game::audio
