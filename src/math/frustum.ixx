export module math:frustum;
import std;
import types;
import :matrix;
import :aabb;

export class FrustumTest {
public:
    explicit FrustumTest(Mat4f const& mvp) {
        // The following code assumes column-major matrices
        auto m = mvp.transpose();

        auto normalize = [&](size_t side) {
            float magnitude = std::sqrt(
                frus[side + 0] * frus[side + 0] + frus[side + 1] * frus[side + 1] + frus[side + 2] * frus[side + 2]
            );
            frus[side + 0] /= magnitude;
            frus[side + 1] /= magnitude;
            frus[side + 2] /= magnitude;
            frus[side + 3] /= magnitude;
        };

        frus[0] = m[0][3] - m[0][0];
        frus[1] = m[1][3] - m[1][0];
        frus[2] = m[2][3] - m[2][0];
        frus[3] = m[3][3] - m[3][0];
        normalize(0);

        frus[4] = m[0][3] + m[0][0];
        frus[5] = m[1][3] + m[1][0];
        frus[6] = m[2][3] + m[2][0];
        frus[7] = m[3][3] + m[3][0];
        normalize(4);

        frus[8] = m[0][3] + m[0][1];
        frus[9] = m[1][3] + m[1][1];
        frus[10] = m[2][3] + m[2][1];
        frus[11] = m[3][3] + m[3][1];
        normalize(8);

        frus[12] = m[0][3] - m[0][1];
        frus[13] = m[1][3] - m[1][1];
        frus[14] = m[2][3] - m[2][1];
        frus[15] = m[3][3] - m[3][1];
        normalize(12);

        frus[16] = m[0][3] - m[0][2];
        frus[17] = m[1][3] - m[1][2];
        frus[18] = m[2][3] - m[2][2];
        frus[19] = m[3][3] - m[3][2];
        normalize(16);

        frus[20] = m[0][3] + m[0][2];
        frus[21] = m[1][3] + m[1][2];
        frus[22] = m[2][3] + m[2][2];
        frus[23] = m[3][3] + m[3][2];
        normalize(20);
    }

    auto test(AABB3f const& box) const -> bool {
        for (auto i = 0uz; i < 24; i += 4) {
            if (frus[i] * box.min.x() + frus[i + 1] * box.min.y() + frus[i + 2] * box.min.z() + frus[i + 3] <= 0.0f
                && frus[i] * box.max.x() + frus[i + 1] * box.min.y() + frus[i + 2] * box.min.z() + frus[i + 3] <= 0.0f
                && frus[i] * box.min.x() + frus[i + 1] * box.max.y() + frus[i + 2] * box.min.z() + frus[i + 3] <= 0.0f
                && frus[i] * box.max.x() + frus[i + 1] * box.max.y() + frus[i + 2] * box.min.z() + frus[i + 3] <= 0.0f
                && frus[i] * box.min.x() + frus[i + 1] * box.min.y() + frus[i + 2] * box.max.z() + frus[i + 3] <= 0.0f
                && frus[i] * box.max.x() + frus[i + 1] * box.min.y() + frus[i + 2] * box.max.z() + frus[i + 3] <= 0.0f
                && frus[i] * box.min.x() + frus[i + 1] * box.max.y() + frus[i + 2] * box.max.z() + frus[i + 3] <= 0.0f
                && frus[i] * box.max.x() + frus[i + 1] * box.max.y() + frus[i + 2] * box.max.z() + frus[i + 3] <= 0.0f)
                return false;
        }
        return true;
    }

private:
    std::array<float, 24> frus = {};
};
