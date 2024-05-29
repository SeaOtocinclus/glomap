#ifndef GLOMAP_SCENE_RIG3D_H_
#define GLOMAP_SCENE_RIG3D_H_
#include <colmap/geometry/rigid3.h>

namespace glomap {
struct Rigid3d : public colmap::Rigid3d {
    Rigid3d() : colmap::Rigid3d() {};
    Rigid3d(const colmap::Rigid3d& rigid3d) : colmap::Rigid3d(rigid3d) {};

    Rigid3d& operator=(const colmap::Rigid3d& rigid3d) {
        *this = Rigid3d(rigid3d);
        return *this;
    }

    inline Eigen::Vector3d Rotate(const Eigen::Vector3d& point) const {
        return rotation * point;
    }

    inline Eigen::Vector3d Derotate(const Eigen::Vector3d& point) const {
        return rotation.inverse() * point;
    }
};

inline std::ostream& operator<<(std::ostream& output, Rigid3d& pose) {
    output << "q: " << pose.rotation.coeffs().transpose() << ", t: " << pose.translation.transpose();
    return output;
}

}; // glomap

#endif // GLOMAP_SCENE_RIG3D_H_
