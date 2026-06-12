#include <iostream>
#include <chrono>
#include <string>
#include <unordered_map>

enum class ObjectiveState { Inactive, Active, Completed };

struct Objective {
    std::string ID;
    std::string Title;
    std::string Description;
    bool Repeatable;
    ObjectiveState State;
};

void run_baseline(int iterations) {
    std::unordered_map<std::string, Objective> m_Objectives;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string objectiveID = "OBJ_" + std::to_string(i);

        auto it = m_Objectives.find(objectiveID);
        if (it == m_Objectives.end())
        {
            Objective newObj;
            newObj.ID = objectiveID;
            newObj.Title = objectiveID;
            newObj.Description = "";
            newObj.Repeatable = false;
            newObj.State = ObjectiveState::Inactive;
            m_Objectives[objectiveID] = newObj;
            it = m_Objectives.find(objectiveID);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> diff = end - start;
    std::cout << "Baseline: " << diff.count() << " ms\n";
}

void run_optimized(int iterations) {
    std::unordered_map<std::string, Objective> m_Objectives;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string objectiveID = "OBJ_" + std::to_string(i);

        auto it = m_Objectives.find(objectiveID);
        if (it == m_Objectives.end())
        {
            Objective newObj;
            newObj.ID = objectiveID;
            newObj.Title = objectiveID;
            newObj.Description = "";
            newObj.Repeatable = false;
            newObj.State = ObjectiveState::Inactive;
            it = m_Objectives.emplace(objectiveID, std::move(newObj)).first;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> diff = end - start;
    std::cout << "Optimized: " << diff.count() << " ms\n";
}

int main() {
    int iterations = 1000000;
    run_baseline(iterations);
    run_optimized(iterations);
    return 0;
}
