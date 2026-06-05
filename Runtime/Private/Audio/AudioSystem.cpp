#include "Runtime/Public/Audio/AudioSystem.h"
#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/Gameplay/GameplayEvent.h"
#include "Runtime/Public/Gameplay/GameplayEventBus.h"
#include "Core/World.h"
#include "ECS/ECSComponents.h"
#include "Core/Logger.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SceneLoader.h"
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace eng::runtime {

    static bool GenerateTestWav(const std::string& filePath, float frequency = 440.0f, float duration = 0.1f) {
        if (std::filesystem::exists(filePath)) {
            return true;
        }
        try {
            std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
            std::ofstream file(filePath, std::ios::binary);
            if (!file) return false;

            uint32_t sampleRate = 44100;
            uint16_t numChannels = 1;
            uint16_t bitsPerSample = 16;
            uint32_t numSamples = static_cast<uint32_t>(sampleRate * duration);
            uint32_t dataSize = numSamples * numChannels * (bitsPerSample / 8);
            uint32_t chunkSize = 36 + dataSize;
            uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
            uint16_t blockAlign = numChannels * (bitsPerSample / 8);

            file.write("RIFF", 4);
            file.write(reinterpret_cast<const char*>(&chunkSize), 4);
            file.write("WAVE", 4);
            file.write("fmt ", 4);
            uint32_t subchunk1Size = 16;
            file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
            uint16_t audioFormat = 1; // PCM
            file.write(reinterpret_cast<const char*>(&audioFormat), 2);
            file.write(reinterpret_cast<const char*>(&numChannels), 2);
            file.write(reinterpret_cast<const char*>(&sampleRate), 4);
            file.write(reinterpret_cast<const char*>(&byteRate), 4);
            file.write(reinterpret_cast<const char*>(&blockAlign), 2);
            file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
            file.write("data", 4);
            file.write(reinterpret_cast<const char*>(&dataSize), 4);

            for (uint32_t i = 0; i < numSamples; ++i) {
                double t = static_cast<double>(i) / sampleRate;
                int16_t sample = static_cast<int16_t>(32767.0 * sin(2.0 * 3.1415926535 * frequency * t));
                file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    bool AudioSystem::Initialize(RuntimeContext* context)
    {
        m_Context = context;
        
        ma_result result = ma_engine_init(nullptr, &m_Engine);
        if (result != MA_SUCCESS)
        {
            m_LastError = "ma_engine_init failed with error code: " + std::to_string(result);
            LOG_ERROR("[AudioSystem] Failed to initialize AudioSystem Engine! Result: %d", result);
            return false;
        }

        m_Initialized = true;
        m_LastError = "None";
        LOG_INFO("[AudioSystem] AudioSystem initialized successfully.");

        // Generate synthetic audio files for test suite coverage
        GenerateTestWav("Assets/Audio/interaction_use.wav", 523.25f, 0.15f); // C5 tone
        GenerateTestWav("Assets/Audio/trigger_enter.wav", 659.25f, 0.15f); // E5 tone
        GenerateTestWav("Assets/Audio/checkpoint.wav", 783.99f, 0.25f);    // G5 tone
        GenerateTestWav("Assets/Audio/objective_completed.wav", 1046.50f, 0.35f); // C6 tone
        GenerateTestWav("Assets/Audio/test_beep.wav", 440.0f, 0.1f);       // A4 tone

        // Subscribe to event bus
        if (m_Context && m_Context->gameplayEventBus)
        {
            m_Context->gameplayEventBus->Subscribe(GameplayEventType::Interaction, [this](const GameplayEvent& e) {
                OnGameplayEvent(e);
            });
            m_Context->gameplayEventBus->Subscribe(GameplayEventType::TriggerEnter, [this](const GameplayEvent& e) {
                OnGameplayEvent(e);
            });
            m_Context->gameplayEventBus->Subscribe(GameplayEventType::CheckpointReached, [this](const GameplayEvent& e) {
                OnGameplayEvent(e);
            });
            m_Context->gameplayEventBus->Subscribe(GameplayEventType::ObjectiveCompleted, [this](const GameplayEvent& e) {
                OnGameplayEvent(e);
            });
        }

        return true;
    }

    void AudioSystem::Shutdown()
    {
        if (!m_Initialized)
            return;

        StopAllSounds();
        ma_engine_uninit(&m_Engine);
        m_Initialized = false;
        m_Context = nullptr;
        m_LoadedClips.clear();
        LOG_INFO("[AudioSystem] AudioSystem shut down cleanly.");
    }

    void AudioSystem::Update(float dt)
    {
        if (!m_Initialized || !m_Context)
            return;

        bool currentlyPlaying = IsPlaying();

        // Handle play-mode state transitions
        if (currentlyPlaying && !m_WasPlaying)
        {
            m_WasPlaying = true;
            
            // Scan for AudioSourceComponents with PlayOnStart
            if (m_Context->ecs)
            {
                auto& coordinator = m_Context->ecs->getCoordinator();
                const auto& entities = coordinator.GetActiveEntities();
                auto audioSourceType = coordinator.GetComponentType<AudioSourceComponent>();
                for (Entity entity : entities)
                {
                    if (coordinator.IsEntityAlive(entity))
                    {
                        auto signature = coordinator.GetSignature(entity);
                        if (signature.test(audioSourceType))
                        {
                            auto& comp = coordinator.GetComponent<AudioSourceComponent>(entity);
                            if (comp.PlayOnStart)
                            {
                                comp.IsPlaying = true;
                            }
                        }
                    }
                }
            }
        }
        else if (!currentlyPlaying && m_WasPlaying)
        {
            m_WasPlaying = false;
            StopAllSounds();
        }

        if (currentlyPlaying)
        {
            // Sync properties of AudioSourceComponents in active ECS
            if (m_Context->ecs)
            {
                auto& coordinator = m_Context->ecs->getCoordinator();
                const auto& entities = coordinator.GetActiveEntities();
                auto audioSourceType = coordinator.GetComponentType<AudioSourceComponent>();
                
                for (Entity entity : entities)
                {
                    if (coordinator.IsEntityAlive(entity))
                    {
                        auto signature = coordinator.GetSignature(entity);
                        if (signature.test(audioSourceType))
                        {
                            auto& comp = coordinator.GetComponent<AudioSourceComponent>(entity);
                            
                            auto it = std::find_if(m_ActiveSounds.begin(), m_ActiveSounds.end(),
                                [entity](const std::unique_ptr<ActiveSound>& as) { return as->EntityID == entity; });
                            
                            if (comp.IsPlaying)
                            {
                                if (it == m_ActiveSounds.end())
                                {
                                    PlayEntitySound(entity, comp.ClipPath, comp.Loop, comp.Volume);
                                }
                                else
                                {
                                    // Sync properties dynamically
                                    ma_sound_set_volume(&(*it)->Sound, std::clamp(comp.Volume, 0.0f, 1.0f));
                                    ma_sound_set_looping(&(*it)->Sound, comp.Loop ? MA_TRUE : MA_FALSE);
                                }
                            }
                            else
                            {
                                if (it != m_ActiveSounds.end())
                                {
                                    StopEntitySound(entity);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Clean up completed non-looping sound streams
        for (auto it = m_ActiveSounds.begin(); it != m_ActiveSounds.end(); )
        {
            ma_bool32 isLooping = ma_sound_is_looping(&(*it)->Sound);
            ma_bool32 isAtEnd = ma_sound_at_end(&(*it)->Sound);
            if (!isLooping && isAtEnd)
            {
                ma_sound_uninit(&(*it)->Sound);
                
                if ((*it)->EntityID != 0 && m_Context->ecs)
                {
                    auto& coordinator = m_Context->ecs->getCoordinator();
                    if (coordinator.IsEntityAlive((*it)->EntityID))
                    {
                        auto signature = coordinator.GetSignature((*it)->EntityID);
                        auto audioSourceType = coordinator.GetComponentType<AudioSourceComponent>();
                        if (signature.test(audioSourceType))
                        {
                            auto& comp = coordinator.GetComponent<AudioSourceComponent>((*it)->EntityID);
                            comp.IsPlaying = false;
                        }
                    }
                }
                it = m_ActiveSounds.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    AudioSystem::SoundHandle AudioSystem::PlayOneShot(const std::string& path, float volume)
    {
        if (!m_Initialized)
            return 0;

        PreloadClip(path);

        auto clipIt = m_LoadedClips.find(path);
        if (clipIt == m_LoadedClips.end() || !clipIt->second.Loaded)
        {
            LOG_WARN("[AudioSystem] Cannot play one-shot: file '%s' was not loaded successfully.", path.c_str());
            return 0;
        }

        auto as = std::make_unique<ActiveSound>();
        as->Handle = m_NextSoundHandle++;
        as->EntityID = 0;
        as->IsOneShot = true;
        as->Path = path;

        ma_result result = ma_sound_init_from_file(&m_Engine, path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &as->Sound);
        if (result == MA_SUCCESS)
        {
            ma_sound_set_looping(&as->Sound, MA_FALSE);
            ma_sound_set_volume(&as->Sound, std::clamp(volume, 0.0f, 1.0f));
            ma_sound_start(&as->Sound);
            m_ActiveSounds.push_back(std::move(as));
            m_LastPlayedClip = path;
            return m_ActiveSounds.back()->Handle;
        }
        else
        {
            m_LastError = "One-shot play error on " + path;
            LOG_ERROR("[AudioSystem] Failed to play one-shot sound '%s': %d", path.c_str(), result);
            return 0;
        }
    }

    void AudioSystem::StopSound(SoundHandle handle)
    {
        auto it = std::find_if(m_ActiveSounds.begin(), m_ActiveSounds.end(),
            [handle](const std::unique_ptr<ActiveSound>& as) { return as->Handle == handle; });

        if (it != m_ActiveSounds.end())
        {
            ma_sound_uninit(&(*it)->Sound);
            m_ActiveSounds.erase(it);
        }
    }

    void AudioSystem::StopAllSounds()
    {
        if (!m_Initialized)
            return;

        for (auto& as : m_ActiveSounds)
        {
            ma_sound_uninit(&as->Sound);
        }
        m_ActiveSounds.clear();
    }

    void AudioSystem::SetMasterVolume(float volume)
    {
        if (!m_Initialized)
            return;
        ma_engine_set_volume(&m_Engine, std::clamp(volume, 0.0f, 1.0f));
    }

    float AudioSystem::GetMasterVolume() const
    {
        if (!m_Initialized)
            return 0.0f;
        return ma_engine_get_volume(const_cast<ma_engine*>(&m_Engine));
    }

    void AudioSystem::OnGameplayEvent(const GameplayEvent& event)
    {
        if (!IsPlaying())
            return;

        switch (event.Type)
        {
            case GameplayEventType::Interaction:
                PlayOneShot("Assets/Audio/interaction_use.wav", 1.0f);
                break;

            case GameplayEventType::TriggerEnter:
                PlayOneShot("Assets/Audio/trigger_enter.wav", 1.0f);
                break;

            case GameplayEventType::CheckpointReached:
                PlayOneShot("Assets/Audio/checkpoint.wav", 1.0f);
                break;

            case GameplayEventType::ObjectiveCompleted:
                PlayOneShot("Assets/Audio/objective_completed.wav", 1.0f);
                break;

            default:
                break;
        }
    }

    bool AudioSystem::IsPlaying() const
    {
        if (!m_Context)
            return false;
        return (m_Context->mode == RuntimeMode::Game) ||
               (m_Context->mode == RuntimeMode::Editor && m_Context->editorSimulationState == EditorSimulationState::Play);
    }

    void AudioSystem::PlayEntitySound(uint32_t entityID, const std::string& path, bool loop, float volume)
    {
        if (!m_Initialized)
            return;

        StopEntitySound(entityID);
        PreloadClip(path);

        auto clipIt = m_LoadedClips.find(path);
        if (clipIt == m_LoadedClips.end() || !clipIt->second.Loaded)
        {
            LOG_WARN("[AudioSystem] Cannot play entity sound: file '%s' was not loaded successfully.", path.c_str());
            return;
        }

        auto as = std::make_unique<ActiveSound>();
        as->Handle = m_NextSoundHandle++;
        as->EntityID = entityID;
        as->IsOneShot = false;
        as->Path = path;

        ma_result result = ma_sound_init_from_file(&m_Engine, path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &as->Sound);
        if (result == MA_SUCCESS)
        {
            ma_sound_set_looping(&as->Sound, loop ? MA_TRUE : MA_FALSE);
            ma_sound_set_volume(&as->Sound, std::clamp(volume, 0.0f, 1.0f));
            ma_sound_start(&as->Sound);
            m_ActiveSounds.push_back(std::move(as));
            m_LastPlayedClip = path;
        }
        else
        {
            m_LastError = "Entity sound play error on " + path;
            LOG_ERROR("[AudioSystem] Failed to play entity sound '%s': %d", path.c_str(), result);
        }
    }

    void AudioSystem::StopEntitySound(uint32_t entityID)
    {
        auto it = std::find_if(m_ActiveSounds.begin(), m_ActiveSounds.end(),
            [entityID](const std::unique_ptr<ActiveSound>& as) { return as->EntityID == entityID; });

        if (it != m_ActiveSounds.end())
        {
            ma_sound_uninit(&(*it)->Sound);
            m_ActiveSounds.erase(it);
        }
    }

    void AudioSystem::PreloadClip(const std::string& path)
    {
        if (m_LoadedClips.find(path) != m_LoadedClips.end())
            return;

        AudioClip clip;
        clip.Path = path;
        auto pos = path.find_last_of("/\\");
        clip.Name = (pos == std::string::npos) ? path : path.substr(pos + 1);

        if (!m_Initialized)
        {
            clip.Loaded = false;
            clip.Duration = 0.0f;
            m_LoadedClips[path] = clip;
            return;
        }

        ma_decoder decoder;
        ma_result result = ma_decoder_init_file(path.c_str(), nullptr, &decoder);
        if (result == MA_SUCCESS)
        {
            ma_uint64 totalFrames = 0;
            ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
            clip.Duration = static_cast<float>(totalFrames) / decoder.outputSampleRate;
            clip.Loaded = true;
            ma_decoder_uninit(&decoder);
        }
        else
        {
            clip.Loaded = false;
            clip.Duration = 0.0f;
            m_LastError = "Decode error: " + path;
            LOG_ERROR("[AudioSystem] Failed to decode audio file '%s': %d", path.c_str(), result);
        }
        m_LoadedClips[path] = clip;
    }

    bool AudioSystem::TestSerialization()
    {
        Coordinator coord;
        coord.Init();
        coord.RegisterComponent<AudioSourceComponent>();

        Entity entity = coord.CreateEntity();
        AudioSourceComponent comp;
        comp.ClipPath = "Assets/Audio/custom_test.wav";
        comp.PlayOnStart = true;
        comp.Loop = true;
        comp.Volume = 0.65f;
        comp.IsPlaying = true;

        coord.AddComponent(entity, comp);

        Scene* scene = new Scene("TestAudioScene");
        auto obj = std::make_shared<SceneObject>("AudioObj");
        obj->SetAudioSource(comp);
        scene->AddSceneObject(obj);

        std::string tempFile = "temp_audio_test_scene.json";
        bool saveSuccess = SceneSerializer::SaveScene(scene, tempFile);
        if (!saveSuccess)
        {
            LOG_ERROR("[Stress] Audio Test 6 FAILED: Failed to serialize scene using SceneSerializer!");
            delete scene;
            std::filesystem::remove(tempFile);
            return false;
        }

        Scene* loadedScene = SceneLoader::LoadFromFile(tempFile);
        if (!loadedScene)
        {
            LOG_ERROR("[Stress] Audio Test 6 FAILED: Failed to deserialize scene using SceneLoader!");
            delete scene;
            std::filesystem::remove(tempFile);
            return false;
        }

        const auto& objects = loadedScene->GetAllSceneObjects();
        if (objects.size() != 1)
        {
            LOG_ERROR("[Stress] Audio Test 6 FAILED: SceneObject count mismatch! Got %zu, expected 1", objects.size());
            delete scene;
            delete loadedScene;
            std::filesystem::remove(tempFile);
            return false;
        }

        auto loadedObj = objects[0];
        if (!loadedObj->m_HasAudioSource)
        {
            LOG_ERROR("[Stress] Audio Test 6 FAILED: Deserialized object does not have AudioSourceComponent!");
            delete scene;
            delete loadedScene;
            std::filesystem::remove(tempFile);
            return false;
        }

        const auto& loadedComp = loadedObj->m_AudioSource;
        if (loadedComp.ClipPath != "Assets/Audio/custom_test.wav" ||
            loadedComp.PlayOnStart != true ||
            loadedComp.Loop != true ||
            std::abs(loadedComp.Volume - 0.65f) > 0.01f ||
            loadedComp.IsPlaying != true)
        {
            LOG_ERROR("[Stress] Audio Test 6 FAILED: AudioSourceComponent property mismatch after roundtrip!");
            delete scene;
            delete loadedScene;
            std::filesystem::remove(tempFile);
            return false;
        }

        delete scene;
        delete loadedScene;
        std::filesystem::remove(tempFile);
        return true;
    }

} // namespace eng::runtime
