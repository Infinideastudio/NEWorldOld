export module math:frustum;
import std;
import types;
import :vector;
import :matrix;
import :aabb;

// 3D convex hulls, mainly used for frustum culling.
export template <typename T>
class Frustum {
public:
    // Normals (equations) of the clip planes of the convex hull.
    std::vector<Vec4<T>> planes;

    // Constructs a frustum from a model-view-projection matrix.
    explicit Frustum(Mat4<T> const& mvp) {
        planes.emplace_back(mvp.row(3) - mvp.row(0)); // X+
        planes.emplace_back(mvp.row(3) + mvp.row(0)); // X-
        planes.emplace_back(mvp.row(3) - mvp.row(1)); // Y+
        planes.emplace_back(mvp.row(3) + mvp.row(1)); // Y-
        planes.emplace_back(mvp.row(3) - mvp.row(2)); // Z+
        planes.emplace_back(mvp.row(3) + mvp.row(2)); // Z-
    }

    // Returns false if the given AABB is completely outside the convex hull.
    // This test is conservative.
    auto test(AABB3<T> const& box) const -> bool {
        auto vertices = std::array{
            Vec4<T>(box.min.x(), box.min.y(), box.min.z(), 1.0f),
            Vec4<T>(box.max.x(), box.min.y(), box.min.z(), 1.0f),
            Vec4<T>(box.min.x(), box.max.y(), box.min.z(), 1.0f),
            Vec4<T>(box.max.x(), box.max.y(), box.min.z(), 1.0f),
            Vec4<T>(box.min.x(), box.min.y(), box.max.z(), 1.0f),
            Vec4<T>(box.max.x(), box.min.y(), box.max.z(), 1.0f),
            Vec4<T>(box.min.x(), box.max.y(), box.max.z(), 1.0f),
            Vec4<T>(box.max.x(), box.max.y(), box.max.z(), 1.0f),
        };
        for (auto plane: planes) {
            auto inside = false;
            for (auto vertex: vertices) {
                if (plane.dot(vertex) > 0.0f) {
                    inside = true;
                    break;
                }
            }
            if (!inside) {
                return false;
            }
        }
        return true;
    }
};

export using Frustumf = Frustum<float>;
export using Frustumd = Frustum<double>;
