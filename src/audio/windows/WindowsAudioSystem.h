#pragma once

#ifdef _WIN32

#include "game/Math.h"
#include "game/audio/AudioEvent.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include <xaudio2.h>

namespace rf::audio::windows {

// Platform playback backend. Gameplay emits semantic events; this class decides how those events
// sound. The first 0.5 bank is procedurally synthesized so the engine has original audible feedback
// without baking sample filenames into mining/inventory code. Authored sample banks can replace the
// synthesizer later without changing gameplay systems.
class WindowsAudioSystem {
public:
    WindowsAudioSystem() = default;
    ~WindowsAudioSystem();

    WindowsAudioSystem(const WindowsAudioSystem&) = delete;
    WindowsAudioSystem& operator=(const WindowsAudioSystem&) = delete;

    bool initialize();
    void shutdown() noexcept;
    void update();
    void consume(const std::vector<game::audio::AudioEvent>& events, game::Vec3 listener);
    void stopAll() noexcept;

    [[nodiscard]] bool initialized() const noexcept { return engine_ != nullptr && masteringVoice_ != nullptr; }
    [[nodiscard]] std::size_t activeVoiceCount() const noexcept { return voices_.size(); }

private:
    struct VoiceInstance {
        IXAudio2SourceVoice* voice{};
        std::vector<std::int16_t> samples;
        XAUDIO2_BUFFER buffer{};
    };

    static constexpr std::uint32_t sampleRate = 24000;
    static constexpr std::size_t maxVoices = 24;

    [[nodiscard]] static std::vector<std::int16_t> synthesize(const game::audio::AudioEvent& event);
    [[nodiscard]] static float spatialGain(const game::audio::AudioEvent& event, game::Vec3 listener) noexcept;
    void play(const game::audio::AudioEvent& event, game::Vec3 listener);

    Microsoft::WRL::ComPtr<IXAudio2> engine_;
    IXAudio2MasteringVoice* masteringVoice_{};
    std::vector<std::unique_ptr<VoiceInstance>> voices_;
};

} // namespace rf::audio::windows

#endif
