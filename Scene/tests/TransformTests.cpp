#include "../Transform.h"
#include <iostream>
#include <cmath>

#define EXPECT_TRUE(cond) \
    if (!(cond)) { \
        std::cerr << "Test failed at " << __FILE__ << ":" << __LINE__ << " - " << #cond << std::endl; \
        return false; \
    }

#define EXPECT_FLOAT_EQ(a, b) \
    if (std::abs((a) - (b)) > 1e-5f) { \
        std::cerr << "Test failed at " << __FILE__ << ":" << __LINE__ << " - Expected " << (b) << " but got " << (a) << std::endl; \
        return false; \
    }

bool TestInitialization() {
    std::cout << "Running TestInitialization...\n";
    Transform t;

    // Check initial position
    const Vector3& pos = t.GetPosition();
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
    EXPECT_FLOAT_EQ(pos.z, 0.0f);

    // Check initial rotation
    const Quaternion& rot = t.GetRotation();
    EXPECT_FLOAT_EQ(rot.x, 0.0f);
    EXPECT_FLOAT_EQ(rot.y, 0.0f);
    EXPECT_FLOAT_EQ(rot.z, 0.0f);
    EXPECT_FLOAT_EQ(rot.w, 1.0f);

    // Check initial scale
    const Vector3& scale = t.GetScale();
    EXPECT_FLOAT_EQ(scale.x, 1.0f);
    EXPECT_FLOAT_EQ(scale.y, 1.0f);
    EXPECT_FLOAT_EQ(scale.z, 1.0f);

    return true;
}

bool TestLocalSettersAndGetters() {
    std::cout << "Running TestLocalSettersAndGetters...\n";
    Transform t;

    t.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    const Vector3& pos = t.GetPosition();
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
    EXPECT_FLOAT_EQ(pos.z, 3.0f);

    t.Translate(Vector3(2.0f, -1.0f, 0.0f));
    const Vector3& pos2 = t.GetPosition();
    EXPECT_FLOAT_EQ(pos2.x, 3.0f);
    EXPECT_FLOAT_EQ(pos2.y, 1.0f);
    EXPECT_FLOAT_EQ(pos2.z, 3.0f);

    t.SetScale(Vector3(2.0f, 2.0f, 2.0f));
    const Vector3& scale = t.GetScale();
    EXPECT_FLOAT_EQ(scale.x, 2.0f);
    EXPECT_FLOAT_EQ(scale.y, 2.0f);
    EXPECT_FLOAT_EQ(scale.z, 2.0f);

    t.ScaleBy(Vector3(1.5f, 0.5f, 2.0f));
    const Vector3& scale2 = t.GetScale();
    EXPECT_FLOAT_EQ(scale2.x, 3.0f);
    EXPECT_FLOAT_EQ(scale2.y, 1.0f);
    EXPECT_FLOAT_EQ(scale2.z, 4.0f);

    return true;
}

bool TestComputeWorldMatrixNoParent() {
    std::cout << "Running TestComputeWorldMatrixNoParent...\n";
    Transform t;
    t.SetPosition(Vector3(10.0f, 20.0f, 30.0f));
    t.SetScale(Vector3(2.0f, 2.0f, 2.0f));

    t.ComputeWorldMatrix(nullptr);

    const Vector3& wPos = t.GetWorldPosition();
    EXPECT_FLOAT_EQ(wPos.x, 10.0f);
    EXPECT_FLOAT_EQ(wPos.y, 20.0f);
    EXPECT_FLOAT_EQ(wPos.z, 30.0f);

    const Vector3& wScale = t.GetWorldScale();
    EXPECT_FLOAT_EQ(wScale.x, 2.0f);
    EXPECT_FLOAT_EQ(wScale.y, 2.0f);
    EXPECT_FLOAT_EQ(wScale.z, 2.0f);

    return true;
}

bool TestComputeWorldMatrixWithParent() {
    std::cout << "Running TestComputeWorldMatrixWithParent...\n";
    Transform parent;
    parent.SetPosition(Vector3(10.0f, 0.0f, 0.0f));
    // Scale parent by 2
    parent.SetScale(Vector3(2.0f, 2.0f, 2.0f));
    parent.ComputeWorldMatrix(nullptr);

    Transform child;
    child.SetPosition(Vector3(5.0f, 0.0f, 0.0f));
    child.SetScale(Vector3(0.5f, 0.5f, 0.5f));
    child.ComputeWorldMatrix(&parent);

    const Vector3& wPos = child.GetWorldPosition();
    // Child local pos (5,0,0) scaled by parent (2,2,2) = (10,0,0), then translated by parent (10,0,0) -> (20,0,0)
    EXPECT_FLOAT_EQ(wPos.x, 20.0f);
    EXPECT_FLOAT_EQ(wPos.y, 0.0f);
    EXPECT_FLOAT_EQ(wPos.z, 0.0f);

    const Vector3& wScale = child.GetWorldScale();
    // Child scale (0.5) * Parent scale (2) = 1.0
    EXPECT_FLOAT_EQ(wScale.x, 1.0f);
    EXPECT_FLOAT_EQ(wScale.y, 1.0f);
    EXPECT_FLOAT_EQ(wScale.z, 1.0f);

    return true;
}

bool TestComputeWorldMatrixDeepHierarchy() {
    std::cout << "Running TestComputeWorldMatrixDeepHierarchy...\n";

    Transform root;
    root.SetPosition(Vector3(0.0f, 10.0f, 0.0f));
    root.ComputeWorldMatrix(nullptr);

    Transform child1;
    child1.SetPosition(Vector3(0.0f, 10.0f, 0.0f));
    child1.ComputeWorldMatrix(&root);

    Transform child2;
    child2.SetPosition(Vector3(0.0f, 10.0f, 0.0f));
    child2.ComputeWorldMatrix(&child1);

    const Vector3& wPos = child2.GetWorldPosition();
    EXPECT_FLOAT_EQ(wPos.x, 0.0f);
    EXPECT_FLOAT_EQ(wPos.y, 30.0f);
    EXPECT_FLOAT_EQ(wPos.z, 0.0f);

    return true;
}

int main() {
    std::cout << "================================================================================\n";
    std::cout << "                            RUNNING TRANSFORM TESTS                             \n";
    std::cout << "================================================================================\n";

    int passed = 0;
    int total = 5;

    if (TestInitialization()) passed++;
    if (TestLocalSettersAndGetters()) passed++;
    if (TestComputeWorldMatrixNoParent()) passed++;
    if (TestComputeWorldMatrixWithParent()) passed++;
    if (TestComputeWorldMatrixDeepHierarchy()) passed++;

    std::cout << "\nTest Results: " << passed << "/" << total << " passed.\n";

    if (passed == total) {
        std::cout << "ALL TESTS PASSED.\n";
        return 0;
    } else {
        std::cout << "SOME TESTS FAILED.\n";
        return 1;
    }
}
