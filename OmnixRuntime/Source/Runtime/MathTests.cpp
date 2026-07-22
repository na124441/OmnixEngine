#include "Runtime/MathTests.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Logging/Logger.h"
#include <cassert>
#include <cmath>

namespace eng::runtime {

    constexpr float EPSILON = 0.0001f;

    inline bool NearEqual(float a, float b, float epsilon = EPSILON) noexcept {
        return std::abs(a - b) <= epsilon;
    }

    inline bool NearEqual(const Vector3& a, const Vector3& b, float epsilon = EPSILON) noexcept {
        return NearEqual(a.x, b.x, epsilon) && NearEqual(a.y, b.y, epsilon) && NearEqual(a.z, b.z, epsilon);
    }

    bool RunMathTests() noexcept {
        LOG_INFO("=== Running Kernel Math Library Tests ===");

        // ---------------------------------------------------------
        // 1. Vector3 Tests
        // ---------------------------------------------------------
        {
            LOG_INFO("[Test] Starting Vector3 Tests...");
            Vector3 v1(1.0f, 2.0f, 3.0f);
            Vector3 v2(4.0f, 5.0f, 6.0f);

            // Operators
            Vector3 add = v1 + v2;
            assert(NearEqual(add, Vector3(5.0f, 7.0f, 9.0f)));

            Vector3 sub = v1 - v2;
            assert(NearEqual(sub, Vector3(-3.0f, -3.0f, -3.0f)));

            Vector3 scale = v1 * 2.0f;
            assert(NearEqual(scale, Vector3(2.0f, 4.0f, 6.0f)));

            // Dot product
            float dot = v1.Dot(v2);
            assert(NearEqual(dot, 32.0f)); // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32

            // Cross product
            Vector3 cross = v1.Cross(v2);
            assert(NearEqual(cross, Vector3(-3.0f, 6.0f, -3.0f)));

            // Length and distance
            Vector3 v3(3.0f, 4.0f, 0.0f);
            assert(NearEqual(v3.Length(), 5.0f));
            assert(NearEqual(v3.LengthSquared(), 25.0f));

            Vector3 norm = v3.Normalized();
            assert(NearEqual(norm, Vector3(0.6f, 0.8f, 0.0f)));
            assert(NearEqual(norm.Length(), 1.0f));

            assert(NearEqual(v1.Distance(v2), std::sqrt(27.0f)));

            // Lerp
            Vector3 lerpVal = Vector3::Lerp(v1, v2, 0.5f);
            assert(NearEqual(lerpVal, Vector3(2.5f, 3.5f, 4.5f)));

            LOG_INFO("[Test] Vector3 Tests Passed.");
        }

        // ---------------------------------------------------------
        // 2. Quaternion Tests
        // ---------------------------------------------------------
        {
            LOG_INFO("[Test] Starting Quaternion Tests...");

            // Axis-angle rotation
            Vector3 axis(0.0f, 1.0f, 0.0f);
            float angle = 3.1415926535f * 0.5f; // 90 degrees yaw
            Quaternion q = Quaternion::FromAxisAngle(axis, angle);

            // Rotating a vector (1, 0, 0) should yield (0, 0, -1) in standard right-handed space
            Vector3 v(1.0f, 0.0f, 0.0f);
            Vector3 rotV = q * v;
            assert(NearEqual(rotV, Vector3(0.0f, 0.0f, -1.0f)));

            // Euler angle conversion
            Quaternion qEuler = Quaternion::FromEuler(0.0f, angle, 0.0f);
            assert(NearEqual(qEuler.x, q.x));
            assert(NearEqual(qEuler.y, q.y));
            assert(NearEqual(qEuler.z, q.z));
            assert(NearEqual(qEuler.w, q.w));

            Vector3 euler = qEuler.ToEuler();
            assert(NearEqual(euler.y, angle));

            // Slerp
            Quaternion q1 = Quaternion::FromAxisAngle(axis, 0.0f);
            Quaternion q2 = Quaternion::FromAxisAngle(axis, angle);
            Quaternion qSlerp = Quaternion::Slerp(q1, q2, 0.5f);
            Quaternion qHalf = Quaternion::FromAxisAngle(axis, angle * 0.5f);
            assert(NearEqual(qSlerp.w, qHalf.w));
            assert(NearEqual(qSlerp.y, qHalf.y));

            LOG_INFO("[Test] Quaternion Tests Passed.");
        }

        // ---------------------------------------------------------
        // 3. Matrix4x4 Tests
        // ---------------------------------------------------------
        {
            LOG_INFO("[Test] Starting Matrix4x4 Tests...");

            Matrix4x4 identity;
            // Verify identity values
            assert(NearEqual(identity.m[0], 1.0f));
            assert(NearEqual(identity.m[5], 1.0f));
            assert(NearEqual(identity.m[10], 1.0f));
            assert(NearEqual(identity.m[15], 1.0f));
            assert(NearEqual(identity.m[1], 0.0f));

            // Translation matrix
            Vector3 pos(10.0f, -5.0f, 3.0f);
            Matrix4x4 trans = Matrix4x4::Translation(pos);
            assert(NearEqual(trans.GetPosition(), pos));

            Vector3 pt(0.0f, 0.0f, 0.0f);
            Vector3 transPt = trans.TransformPoint(pt);
            assert(NearEqual(transPt, pos));

            // TRS combine & extract
            Vector3 scale(2.0f, 0.5f, 1.5f);
            Quaternion rot = Quaternion::FromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), 3.1415926535f * 0.25f);
            Matrix4x4 trs = Matrix4x4::TRS(pos, rot, scale);

            assert(NearEqual(trs.GetPosition(), pos));
            assert(NearEqual(trs.GetScale(), scale));

            // Matrix Inversion
            Matrix4x4 invTrs = trs.Inversed();
            Matrix4x4 product = trs * invTrs;
            // Product of a matrix and its inverse must equal the identity matrix
            for (int i = 0; i < 16; ++i) {
                float expected = (i % 5 == 0) ? 1.0f : 0.0f; // Diagonal indices are 0, 5, 10, 15
                assert(NearEqual(product.m[i], expected, 0.001f));
            }

            // Projections (Perspective FOV)
            Matrix4x4 proj = Matrix4x4::Perspective(3.1415926535f * 0.5f, 16.0f / 9.0f, 0.1f, 1000.0f);
            assert(proj.m[11] == -1.0f); // Perspective divide trigger

            LOG_INFO("[Test] Matrix4x4 Tests Passed.");
        }

        LOG_INFO("=== All Math Library Tests Passed Successfully ===");
        return true;
    }

} // namespace eng::runtime
