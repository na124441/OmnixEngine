#pragma once

#include <string>

namespace eng::runtime {

    class ISceneManager {
    public:
        virtual ~ISceneManager() = default;

        virtual void LoadScene(const std::string& sceneName) = 0;
        virtual void SwitchScene() = 0;
        virtual void ReloadCurrentScene() = 0;
        virtual void Update(float dt) = 0;
    };

} // namespace eng::runtime
