export module frustum_tests;
import std;
import types;
import mat4;

export class FrustumTest {
public:
    // AABB with 32-bit float coords
    struct AABBf {
        float xmin, ymin, zmin;
        float xmax, ymax, zmax;
    };

    explicit FrustumTest(Mat4f const& mvp) {
        // The following code assumes column-major matrices
        auto m = mvp.transpose();

        auto normalize = [&](int side) {
            float magnitude = std::sqrt(
                frus[side + 0] * frus[side + 0] + frus[side + 1] * frus[side + 1] + frus[side + 2] * frus[side + 2]
            );
            frus[side + 0] /= magnitude;
            frus[side + 1] /= magnitude;
            frus[side + 2] /= magnitude;
            frus[side + 3] /= magnitude;
        };

        frus[0] = m.data[3] - m.data[0];
        frus[1] = m.data[7] - m.data[4];
        frus[2] = m.data[11] - m.data[8];
        frus[3] = m.data[15] - m.data[12];
        normalize(0);

        frus[4] = m.data[3] + m.data[0];
        frus[5] = m.data[7] + m.data[4];
        frus[6] = m.data[11] + m.data[8];
        frus[7] = m.data[15] + m.data[12];
        normalize(4);

        frus[8] = m.data[3] + m.data[1];
        frus[9] = m.data[7] + m.data[5];
        frus[10] = m.data[11] + m.data[9];
        frus[11] = m.data[15] + m.data[13];
        normalize(8);

        frus[12] = m.data[3] - m.data[1];
        frus[13] = m.data[7] - m.data[5];
        frus[14] = m.data[11] - m.data[9];
        frus[15] = m.data[15] - m.data[13];
        normalize(12);

        frus[16] = m.data[3] - m.data[2];
        frus[17] = m.data[7] - m.data[6];
        frus[18] = m.data[11] - m.data[10];
        frus[19] = m.data[15] - m.data[14];
        normalize(16);

        frus[20] = m.data[3] + m.data[2];
        frus[21] = m.data[7] + m.data[6];
        frus[22] = m.data[11] + m.data[10];
        frus[23] = m.data[15] + m.data[14];
        normalize(20);
    }

    auto test(AABBf const& aabb) const -> bool {
        for (int i = 0; i < 24; i += 4) {
            if (frus[i] * aabb.xmin + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0f
                && frus[i] * aabb.xmax + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0f
                && frus[i] * aabb.xmin + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0f
                && frus[i] * aabb.xmax + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0f
                && frus[i] * aabb.xmin + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0f
                && frus[i] * aabb.xmax + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0f
                && frus[i] * aabb.xmin + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0f
                && frus[i] * aabb.xmax + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0f)
                return false;
        }
        return true;
    }

private:
    std::array<float, 24> frus = {};
};
