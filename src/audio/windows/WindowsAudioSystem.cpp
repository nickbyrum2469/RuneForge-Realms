#ifdef _WIN32

#include "audio/windows/WindowsAudioSystem.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rf::audio::windows {
namespace {

float randomSigned(std::uint32_t& state) noexcept {
    state = state * 1664525u + 1013904223u;
    const float unit = static_cast<float>((state >> 8) & 0x00ffffffu) / 16777215.0f;
    return unit * 2.0f - 1.0f;
}

float eventDuration(game::audio::AudioEventType type) noexcept {
    using game::audio::AudioEventType;
    switch (type) {
        case AudioEventType::BlockBreak: return 0.16f;
        case AudioEventType::MiningHit: return 0.085f;
        case AudioEventType::BlockPlace: return 0.095f;
        case AudioEventType::ItemPickup: return 0.11f;
        case AudioEventType::Footstep: return 0.085f;
        case AudioEventType::Jump:
        case AudioEventType::Land: return 0.10f;
        case AudioEventType::WaterSplash: return 0.18f;
        case AudioEventType::InventoryMove:
        case AudioEventType::UiHover:
        case AudioEventType::UiClick: return 0.055f;
        case AudioEventType::Craft: return 0.14f;
        case AudioEventType::PlayerDamage: return 0.16f;
        case AudioEventType::Environment: return 0.20f;
    }
    return 0.10f;
}

struct Timbre {
    float lowHz{};
    float highHz{};
    float noiseMix{};
    float filteredNoiseMix{};
    float decayPower{2.0f};
};

Timbre timbreFor(const game::audio::AudioEvent& event) noexcept {
    using world::blocks::SoundFamily;
    switch (event.family) {
        case SoundFamily::Grass: return {150.0f, 2100.0f, 0.52f, 0.26f, 2.8f};
        case SoundFamily::Dirt: return {92.0f, 310.0f, 0.32f, 0.54f, 2.2f};
        case SoundFamily::Stone: return {390.0f, 1320.0f, 0.39f, 0.21f, 3.0f};
        case SoundFamily::Wood: return {205.0f, 690.0f, 0.25f, 0.34f, 2.6f};
        case SoundFamily::Leaves: return {260.0f, 2850.0f, 0.68f, 0.17f, 3.2f};
        case SoundFamily::Water: return {180.0f, 760.0f, 0.57f, 0.38f, 1.9f};
        case SoundFamily::None: break;
    }

    using game::audio::AudioEventType;
    if (event.type == AudioEventType::ItemPickup || event.type == AudioEventType::UiClick ||
        event.type == AudioEventType::UiHover || event.type == AudioEventType::InventoryMove) {
        return {720.0f, 1440.0f, 0.08f, 0.06f, 3.4f};
    }
    return {180.0f, 520.0f, 0.24f, 0.22f, 2.5f};
}

} // namespace

WindowsAudioSystem::~WindowsAudioSystem() { shutdown(); }

bool WindowsAudioSystem::initialize() {
    if (initialized()) return true;
    Microsoft::WRL::ComPtr<IXAudio2> engine;
    if (FAILED(XAudio2Create(engine.ReleaseAndGetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR))) return false;
    IXAudio2MasteringVoice* mastering = nullptr;
    if (FAILED(engine->CreateMasteringVoice(&mastering))) return false;
    engine_ = std::move(engine);
    masteringVoice_ = mastering;
    return true;
}

void WindowsAudioSystem::shutdown() noexcept {
    stopAll();
    if (masteringVoice_) masteringVoice_->DestroyVoice();
    masteringVoice_ = nullptr;
    engine_.Reset();
}

void WindowsAudioSystem::stopAll() noexcept {
    for (auto& instance : voices_) {
        if (instance && instance->voice) {
            instance->voice->Stop(0);
            instance->voice->DestroyVoice();
            instance->voice = nullptr;
        }
    }
    voices_.clear();
}

void WindowsAudioSystem::update() {
    for (auto it = voices_.begin(); it != voices_.end();) {
        if (!*it || !(*it)->voice) {
            it = voices_.erase(it);
            continue;
        }
        XAUDIO2_VOICE_STATE state{};
        (*it)->voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued != 0) {
            ++it;
            continue;
        }
        (*it)->voice->DestroyVoice();
        (*it)->voice = nullptr;
        it = voices_.erase(it);
    }
}

float WindowsAudioSystem::spatialGain(const game::audio::AudioEvent& event, game::Vec3 listener) noexcept {
    if (!event.spatial) return std::clamp(event.gain, 0.0f, 1.5f);
    const game::Vec3 delta = event.position - listener;
    const float distanceSq = game::lengthSquared(delta);
    const float attenuation = 1.0f / (1.0f + distanceSq * 0.20f);
    return std::clamp(event.gain * attenuation, 0.0f, 1.25f);
}

std::vector<std::int16_t> WindowsAudioSystem::synthesize(const game::audio::AudioEvent& event) {
    const float pitch = std::clamp(event.pitch, 0.72f, 1.35f);
    const float duration = eventDuration(event.type) / pitch;
    const std::size_t sampleCount = std::max<std::size_t>(32, static_cast<std::size_t>(duration * sampleRate));
    std::vector<std::int16_t> result(sampleCount);

    const Timbre timbre = timbreFor(event);
    std::uint32_t randomState = event.variationSeed ^ 0xa511e9b3u;
    float filteredNoise = 0.0f;
    const float phaseOffset = static_cast<float>(event.variationSeed & 1023u) / 1024.0f *
                              2.0f * std::numbers::pi_v<float>;

    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float normalized = static_cast<float>(i) / static_cast<float>(std::max<std::size_t>(sampleCount - 1, 1));
        const float attack = std::min(1.0f, t / 0.004f);
        const float envelope = attack * std::pow(std::max(0.0f, 1.0f - normalized), timbre.decayPower);

        const float noise = randomSigned(randomState);
        filteredNoise += (noise - filteredNoise) * 0.16f;
        const float low = std::sin(2.0f * std::numbers::pi_v<float> * timbre.lowHz * pitch * t + phaseOffset);
        const float high = std::sin(2.0f * std::numbers::pi_v<float> * timbre.highHz * pitch * t + phaseOffset * 0.41f);

        float sample = low * 0.34f + high * 0.18f + noise * timbre.noiseMix + filteredNoise * timbre.filteredNoiseMix;
        if (event.type == game::audio::AudioEventType::BlockBreak) {
            const float crumble = std::sin(2.0f * std::numbers::pi_v<float> * timbre.lowHz * 0.47f * t);
            sample += crumble * 0.22f;
        } else if (event.type == game::audio::AudioEventType::ItemPickup) {
            const float chime = std::sin(2.0f * std::numbers::pi_v<float> * 1720.0f * pitch * t);
            sample = sample * 0.35f + chime * 0.55f;
        }

        sample = std::clamp(sample * envelope * 0.52f, -1.0f, 1.0f);
        result[i] = static_cast<std::int16_t>(sample * 32767.0f);
    }
    return result;
}

void WindowsAudioSystem::play(const game::audio::AudioEvent& event, game::Vec3 listener) {
    if (!initialized()) return;
    update();
    if (voices_.size() >= maxVoices) return;

    auto instance = std::make_unique<VoiceInstance>();
    instance->samples = synthesize(event);
    if (instance->samples.empty()) return;

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    IXAudio2SourceVoice* voice = nullptr;
    if (FAILED(engine_->CreateSourceVoice(&voice, &format))) return;
    instance->voice = voice;
    instance->buffer.Flags = XAUDIO2_END_OF_STREAM;
    instance->buffer.AudioBytes = static_cast<UINT32>(instance->samples.size() * sizeof(std::int16_t));
    instance->buffer.pAudioData = reinterpret_cast<const BYTE*>(instance->samples.data());

    const float gain = spatialGain(event, listener);
    voice->SetVolume(gain);
    if (FAILED(voice->SubmitSourceBuffer(&instance->buffer)) || FAILED(voice->Start(0))) {
        voice->DestroyVoice();
        instance->voice = nullptr;
        return;
    }
    voices_.push_back(std::move(instance));
}

void WindowsAudioSystem::consume(const std::vector<game::audio::AudioEvent>& events, game::Vec3 listener) {
    if (events.empty()) {
        update();
        return;
    }
    if (!initialized() && !initialize()) return;

    update();
    std::size_t emitted = 0;
    for (const auto& event : events) {
        if (emitted >= 8 || voices_.size() >= maxVoices) break;
        play(event, listener);
        ++emitted;
    }
}

} // namespace rf::audio::windows

#endif