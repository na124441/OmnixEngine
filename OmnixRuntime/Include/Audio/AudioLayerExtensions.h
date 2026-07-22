#pragma once

#include "Runtime/Audio/AudioSystem.h"
#include "ECS/ECSComponents.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <cmath>

namespace eng::audio {

    // -------------------------------------------------------------------------
    // 1. Audio Device Subsystem
    // -------------------------------------------------------------------------
    struct AudioDeviceCapabilities {
        std::string deviceName = "Default Output Device";
        uint32_t sampleRate = 48000;
        uint32_t channels = 2;
        bool isInitialized = true;
    };

    // -------------------------------------------------------------------------
    // 2. Audio Listener Subsystem
    // -------------------------------------------------------------------------
    struct AudioListenerComponent {
        bool active = true;
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 forward{ 0.0f, 0.0f, -1.0f };
        glm::vec3 up{ 0.0f, 1.0f, 0.0f };
        glm::vec3 velocity{ 0.0f, 0.0f, 0.0f };
    };

    class AudioListenerSystem {
    public:
        void SetActiveListener(const AudioListenerComponent& listener) {
            m_ActiveListener = listener;
        }

        const AudioListenerComponent& GetActiveListener() const {
            return m_ActiveListener;
        }

    private:
        AudioListenerComponent m_ActiveListener;
    };

    // -------------------------------------------------------------------------
    // 3. Audio Mixer Subsystem
    // -------------------------------------------------------------------------
    enum class MixerChannelType : uint8_t {
        Master = 0,
        Music,
        SFX,
        Voice,
        UI,
        Count
    };

    struct MixerChannel {
        std::string name;
        float volume = 1.0f;
        float pitch = 1.0f;
        bool muted = false;
    };

    class AudioMixer {
    public:
        AudioMixer() {
            m_Channels[MixerChannelType::Master] = { "Master", 1.0f, 1.0f, false };
            m_Channels[MixerChannelType::Music]  = { "Music",  0.8f, 1.0f, false };
            m_Channels[MixerChannelType::SFX]    = { "SFX",    1.0f, 1.0f, false };
            m_Channels[MixerChannelType::Voice]  = { "Voice",  1.0f, 1.0f, false };
            m_Channels[MixerChannelType::UI]     = { "UI",     1.0f, 1.0f, false };
        }

        void SetChannelVolume(MixerChannelType type, float volume) {
            m_Channels[type].volume = glm::clamp(volume, 0.0f, 2.0f);
        }

        float GetChannelVolume(MixerChannelType type) const {
            auto it = m_Channels.find(type);
            return (it != m_Channels.end()) ? it->second.volume : 1.0f;
        }

        float GetEffectiveVolume(MixerChannelType type) const {
            float masterVol = GetChannelVolume(MixerChannelType::Master);
            if (m_Channels.at(MixerChannelType::Master).muted) return 0.0f;
            auto it = m_Channels.find(type);
            if (it != m_Channels.end() && !it->second.muted) {
                return masterVol * it->second.volume;
            }
            return 0.0f;
        }

        void SetMuted(MixerChannelType type, bool mute) {
            m_Channels[type].muted = mute;
        }

        bool IsMuted(MixerChannelType type) const {
            auto it = m_Channels.find(type);
            return (it != m_Channels.end()) ? it->second.muted : false;
        }

    private:
        std::unordered_map<MixerChannelType, MixerChannel> m_Channels;
    };

    // -------------------------------------------------------------------------
    // 4. Streaming Audio Subsystem
    // -------------------------------------------------------------------------
    struct AudioStreamBuffer {
        std::string streamPath;
        size_t chunkSize = 65536; // 64 KB chunks
        size_t totalBytes = 0;
        size_t readBytes = 0;
        bool isStreaming = false;

        bool HasMoreChunks() const {
            return isStreaming && (readBytes < totalBytes);
        }

        float GetProgress() const {
            return (totalBytes > 0) ? static_cast<float>(readBytes) / static_cast<float>(totalBytes) : 0.0f;
        }
    };

    class AudioStreamer {
    public:
        AudioStreamBuffer OpenStream(const std::string& path, size_t fileSizeBytes) {
            AudioStreamBuffer buf;
            buf.streamPath = path;
            buf.chunkSize = 65536;
            buf.totalBytes = fileSizeBytes;
            buf.readBytes = 0;
            buf.isStreaming = true;
            return buf;
        }

        void ReadNextChunk(AudioStreamBuffer& buf) {
            if (!buf.isStreaming) return;
            buf.readBytes = std::min(buf.readBytes + buf.chunkSize, buf.totalBytes);
            if (buf.readBytes >= buf.totalBytes) {
                buf.isStreaming = false;
            }
        }
    };

    // -------------------------------------------------------------------------
    // 5. 3D Spatial Audio Subsystem
    // -------------------------------------------------------------------------
    enum class AttenuationModel : uint8_t {
        Linear = 0,
        Inverse,
        Exponential
    };

    struct SpatialAudioSettings {
        AttenuationModel attenuation = AttenuationModel::Linear;
        float minDistance = 1.0f;
        float maxDistance = 50.0f;
        float rolloffFactor = 1.0f;
    };

    class SpatialAudioSystem {
    public:
        static float CalculateAttenuation(
            const glm::vec3& sourcePos,
            const glm::vec3& listenerPos,
            const SpatialAudioSettings& settings
        ) {
            float dist = glm::distance(sourcePos, listenerPos);
            if (dist <= settings.minDistance) return 1.0f;
            if (dist >= settings.maxDistance) return 0.0f;

            float range = settings.maxDistance - settings.minDistance;
            float normDist = (dist - settings.minDistance) / range;

            switch (settings.attenuation) {
            case AttenuationModel::Linear:
                return glm::clamp(1.0f - normDist, 0.0f, 1.0f);
            case AttenuationModel::Inverse:
                return glm::clamp(settings.minDistance / (settings.minDistance + settings.rolloffFactor * (dist - settings.minDistance)), 0.0f, 1.0f);
            case AttenuationModel::Exponential:
                return std::pow(1.0f - normDist, settings.rolloffFactor * 2.0f);
            default:
                return 1.0f;
            }
        }
    };

    // -------------------------------------------------------------------------
    // 6. Reverb Zones / Occlusion Subsystem
    // -------------------------------------------------------------------------
    struct ReverbZone {
        std::string name = "Cave";
        glm::vec3 center{ 0.0f, 0.0f, 0.0f };
        glm::vec3 extents{ 10.0f, 10.0f, 10.0f };
        float roomSize = 0.8f;
        float decayTimeSeconds = 2.5f;
        float wetMix = 0.5f;
        float dryMix = 0.8f;

        bool Contains(const glm::vec3& pos) const {
            glm::vec3 minP = center - extents * 0.5f;
            glm::vec3 maxP = center + extents * 0.5f;
            return (pos.x >= minP.x && pos.x <= maxP.x &&
                    pos.y >= minP.y && pos.y <= maxP.y &&
                    pos.z >= minP.z && pos.z <= maxP.z);
        }
    };

    class AudioOcclusionSystem {
    public:
        static float CalculateOcclusionFactor(
            const glm::vec3& listenerPos,
            const glm::vec3& sourcePos,
            bool isObstructed
        ) {
            if (!isObstructed) return 1.0f; // No occlusion dampening
            float dist = glm::distance(listenerPos, sourcePos);
            // Dampen volume based on distance behind obstacle
            return glm::clamp(1.0f - (dist * 0.02f), 0.1f, 0.5f);
        }
    };

    // -------------------------------------------------------------------------
    // 7 & 8. DSP Effects & Audio Debug Subsystem
    // -------------------------------------------------------------------------
    enum class FilterType : uint8_t {
        LowPass = 0,
        HighPass,
        BandPass
    };

    struct DSPEffect {
        FilterType type = FilterType::LowPass;
        float cutoffFrequencyHz = 1000.0f;
        float resonance = 1.0f;
        bool enabled = true;
    };

    struct AudioSpectrumData {
        float peakVolumeDb = -6.0f;
        float rmsVolumeDb = -12.0f;
        float frequencyBands[8] = { 0.1f, 0.3f, 0.5f, 0.8f, 0.6f, 0.4f, 0.2f, 0.1f };
    };

    class AudioDSPDebugger {
    public:
        static AudioSpectrumData AnalyzeSpectrum(float masterVolume) {
            AudioSpectrumData spec;
            spec.peakVolumeDb = 20.0f * std::log10(std::max(masterVolume, 1e-4f));
            spec.rmsVolumeDb = spec.peakVolumeDb - 6.0f;
            for (int i = 0; i < 8; ++i) {
                spec.frequencyBands[i] *= masterVolume;
            }
            return spec;
        }
    };

} // namespace eng::audio
