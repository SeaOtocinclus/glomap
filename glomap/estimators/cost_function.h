
#pragma once
#pragma once

#include <Eigen/Core>
#include <ceres/ceres.h>
#include <ceres/rotation.h>

namespace glomap {

// ----------------------------------------
// BATAPairwiseDirectionError
// ----------------------------------------
// Computes the error between a translation direction and the direction formed
// from two positions such that t_ij - scale * (c_j - c_i) is minimized.
struct BATAPairwiseDirectionError {
  BATAPairwiseDirectionError(const Eigen::Vector3d& translation_obs)
      : translation_obs_(translation_obs){};

  // The error is given by the position error described above.
  template <typename T>
  bool operator()(const T* position1,
                  const T* position2,
                  const T* scale,
                  T* residuals) const {
    Eigen::Matrix<T, 3, 1> translation;
    translation[0] = position2[0] - position1[0];
    translation[1] = position2[1] - position1[1];
    translation[2] = position2[2] - position1[2];

    Eigen::Matrix<T, 3, 1> residual_vec;

    residual_vec = translation_obs_.cast<T>() - scale[0] * translation;

    residuals[0] = residual_vec(0);
    residuals[1] = residual_vec(1);
    residuals[2] = residual_vec(2);

    return true;
  }

  static ceres::CostFunction* Create(const Eigen::Vector3d& translation_obs) {
    return (
        new ceres::AutoDiffCostFunction<BATAPairwiseDirectionError, 3, 3, 3, 1>(
            new BATAPairwiseDirectionError(translation_obs)));
  }

  // TODO: add covariance
  const Eigen::Vector3d translation_obs_;
};

// ----------------------------------------
// FetzerFocalLengthCost
// ----------------------------------------
// Below are assets for DMAP by Philipp Lindenberger
inline Eigen::Vector4d fetzer_d(const Eigen::Vector3d& ai,
                                const Eigen::Vector3d& bi,
                                const Eigen::Vector3d& aj,
                                const Eigen::Vector3d& bj,
                                const int u,
                                const int v) {
  Eigen::Vector4d d;
  d.setZero();
  d(0) = ai(u) * aj(v) - ai(v) * aj(u);
  d(1) = ai(u) * bj(v) - ai(v) * bj(u);
  d(2) = bi(u) * aj(v) - bi(v) * aj(u);
  d(3) = bi(u) * bj(v) - bi(v) * bj(u);
  return d;
}

inline std::array<Eigen::Vector4d, 3> fetzer_ds(Eigen::Matrix3d& i1_G_i0) {
  Eigen::JacobiSVD<Eigen::Matrix3d> svd(
      i1_G_i0, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Vector3d s = svd.singularValues();

  Eigen::Vector3d v_0 = svd.matrixV().col(0);
  Eigen::Vector3d v_1 = svd.matrixV().col(1);

  Eigen::Vector3d u_0 = svd.matrixU().col(0);
  Eigen::Vector3d u_1 = svd.matrixU().col(1);

  Eigen::Vector3d ai =
      Eigen::Vector3d(s(0) * s(0) * (v_0(0) * v_0(0) + v_0(1) * v_0(1)),
                      s(0) * s(1) * (v_0(0) * v_1(0) + v_0(1) * v_1(1)),
                      s(1) * s(1) * (v_1(0) * v_1(0) + v_1(1) * v_1(1)));

  Eigen::Vector3d aj = Eigen::Vector3d(u_1(0) * u_1(0) + u_1(1) * u_1(1),
                                       -(u_0(0) * u_1(0) + u_0(1) * u_1(1)),
                                       u_0(0) * u_0(0) + u_0(1) * u_0(1));

  Eigen::Vector3d bi = Eigen::Vector3d(s(0) * s(0) * v_0(2) * v_0(2),
                                       s(0) * s(1) * v_0(2) * v_1(2),
                                       s(1) * s(1) * v_1(2) * v_1(2));

  Eigen::Vector3d bj =
      Eigen::Vector3d(u_1(2) * u_1(2), -(u_0(2) * u_1(2)), u_0(2) * u_0(2));

  Eigen::Vector4d d_01 = fetzer_d(ai, bi, aj, bj, 1, 0);
  Eigen::Vector4d d_02 = fetzer_d(ai, bi, aj, bj, 0, 2);
  Eigen::Vector4d d_12 = fetzer_d(ai, bi, aj, bj, 2, 1);

  std::array<Eigen::Vector4d, 3> ds;
  ds[0] = d_01;
  ds[1] = d_02;
  ds[2] = d_12;

  return ds;
}

class FetzerFocalLengthCost {
 public:
  FetzerFocalLengthCost(const Eigen::Matrix3d& i1_F_i0,
                        const Eigen::Vector2d& principal_point0,
                        const Eigen::Vector2d& principal_point1) {
    Eigen::Matrix3d K0 = Eigen::Matrix3d::Identity(3, 3);
    K0(0, 2) = principal_point0(0);
    K0(1, 2) = principal_point0(1);

    Eigen::Matrix3d K1 = Eigen::Matrix3d::Identity(3, 3);
    K1(0, 2) = principal_point1(0);
    K1(1, 2) = principal_point1(1);

    Eigen::Matrix3d i1_G_i0 = K1.transpose() * i1_F_i0 * K0;

    std::array<Eigen::Vector4d, 3> ds = fetzer_ds(i1_G_i0);

    d_01 = ds[0];
    d_02 = ds[1];
    d_12 = ds[2];
  }

  static ceres::CostFunction* Create(const Eigen::Matrix3d i1_F_i0,
                                     const Eigen::Vector2d& principal_point0,
                                     const Eigen::Vector2d& principal_point1) {
    return (new ceres::AutoDiffCostFunction<FetzerFocalLengthCost, 2, 1, 1>(
        new FetzerFocalLengthCost(
            i1_F_i0, principal_point0, principal_point1)));
  }

  template <typename T>
  bool operator()(const T* const fi_, const T* const fj_, T* residuals) const {
    Eigen::Vector<T, 4> d_01_ = d_01.cast<T>();
    Eigen::Vector<T, 4> d_12_ = d_12.cast<T>();

    T fi = fi_[0];
    T fj = fj_[0];

    T K0_01 =
        -(fj * fj * d_01_(2) + d_01_(3)) / (fj * fj * d_01_(0) + d_01_(1));
    T K1_12 =
        -(fi * fi * d_12_(1) + d_12_(3)) / (fi * fi * d_12_(0) + d_12_(2));

    residuals[0] = (fi * fi - K0_01) / (fi * fi);
    residuals[1] = (fj * fj - K1_12) / (fj * fj);

    return true;
  }

 private:
  Eigen::Vector4d d_01;
  Eigen::Vector4d d_02;
  Eigen::Vector4d d_12;
};

// Calibration error for the image pairs sharing the camera
class FetzerFocalLengthSameCameraCost {
 public:
  FetzerFocalLengthSameCameraCost(const Eigen::Matrix3d& i1_F_i0,
                                  const Eigen::Vector2d& principal_point) {
    Eigen::Matrix3d K0 = Eigen::Matrix3d::Identity(3, 3);
    K0(0, 2) = principal_point(0);
    K0(1, 2) = principal_point(1);

    Eigen::Matrix3d K1 = Eigen::Matrix3d::Identity(3, 3);
    K1(0, 2) = principal_point(0);
    K1(1, 2) = principal_point(1);

    Eigen::Matrix3d i1_G_i0 = K1.transpose() * i1_F_i0 * K0;

    std::array<Eigen::Vector4d, 3> ds = fetzer_ds(i1_G_i0);

    d_01 = ds[0];
    d_02 = ds[1];
    d_12 = ds[2];
  }

  static ceres::CostFunction* Create(const Eigen::Matrix3d i1_F_i0,
                                     const Eigen::Vector2d& principal_point) {
    return (
        new ceres::AutoDiffCostFunction<FetzerFocalLengthSameCameraCost, 2, 1>(
            new FetzerFocalLengthSameCameraCost(i1_F_i0, principal_point)));
  }

  template <typename T>
  bool operator()(const T* const fi_, T* residuals) const {
    Eigen::Vector<T, 4> d_01_ = d_01.cast<T>();
    Eigen::Vector<T, 4> d_12_ = d_12.cast<T>();

    T fi = fi_[0];
    T fj = fi_[0];

    T K0_01 =
        -(fj * fj * d_01_(2) + d_01_(3)) / (fj * fj * d_01_(0) + d_01_(1));
    T K1_12 =
        -(fi * fi * d_12_(1) + d_12_(3)) / (fi * fi * d_12_(0) + d_12_(2));

    residuals[0] = (fi * fi - K0_01) / (fi * fi);
    residuals[1] = (fj * fj - K1_12) / (fj * fj);

    return true;
  }

 private:
  Eigen::Vector4d d_01;
  Eigen::Vector4d d_02;
  Eigen::Vector4d d_12;
};

}  // namespace glomap
