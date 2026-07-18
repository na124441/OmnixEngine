#pragma once

#include "miniaudio/miniaudio.h"
#include "Runtime/Audio/AudioClip.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace eng::runtime {

    struct RuntimeContext;
    struct GameplayEvent;

    class AudioSystem
    {
    public:
        using SoundHandle = uint32_t;

        struct ActiveSound
        {
            SoundHandle Handle = 0;
            ma_sound Sound;
            uint32_t EntityID = 0;
            bool IsOneShot = false;
            std::string Path;
        };

        AudioSystem() = default;
        ~AudioSystem() { Shutdown(); }

        bool Initialize(RuntimeContext* context);
        void Shutdown();

        void Update(float dt);

        // Playback API
        SoundHandle PlayOneShot(const std::string& path, float volume = 1.0f);
        void StopSound(SoundHandle handle);
        void StopAllSounds();

        void SetMasterVolume(float volume);
        float GetMasterVolume() const;

        // Telemetry
        bool IsInitialized() const { return m_Initialized; }
        size_t GetLoadedClipsCount() const { return m_LoadedClips.size(); }
        size_t GetActiveSoundsCount() const { return m_ActiveSounds.size(); }
        std::string GetLastPlayedClip() const { return m_LastPlayedClip; }
        std::string GetLastError() const { return m_LastError; }

        // Event handler
        void OnGameplayEvent(const GameplayEvent& event);

        // Serialization Test
        static bool TestSerialization();

    private:
        bool IsPlaying() const;
        void PlayEntitySound(uint32_t entityID, const std::string& path, bool loop, float volume);
        void StopEntitySound(uint32_t entityID);
        void PreloadClip(const std::string& path);

        RuntimeContext* m_Context = nullptr;
        bool m_Initialized = false;
        ma_engine m_Engine;

        std::unordered_map<std::string, AudioClip> m_LoadedClips;
        std::vector<std::unique_ptr<ActiveSound>> m_ActiveSounds;
        SoundHandle m_NextSoundHandle = 1;

        std::string m_LastPlayedClip = "None";
        std::string m_LastError = "None";
        bool m_WasPlaying = false;
    };

} // namespace eng::runtime
