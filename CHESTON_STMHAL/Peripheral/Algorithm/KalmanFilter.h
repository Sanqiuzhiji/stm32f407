#pragma once

#include "../../main_link.h"
#include <array>

// 使用编译期常量替代宏
constexpr size_t STATE_DIM = 2; // 状态变量维度（角度和偏置）
constexpr size_t MEASURE_DIM = 1; // 观测变量维度（角度）

// 使用模板定义固定大小矩阵
template <size_t Rows, size_t Cols>
using Matrix = std::array<std::array<float, Cols>, Rows>;

class KalmanFilter
{
public:
    KalmanFilter() = default;
    // 初始化卡尔曼滤波器
    void Init(float Q_angle, float Q_bias, float R_measure);
    float Update(float z, float gyroRate, float dt);

    // Kalman滤波器结构体
    typedef struct
    {
        // 状态变量 [角度, 偏置]
        std::array<float, STATE_DIM> x;
        // 误差协方差矩阵
        Matrix<STATE_DIM, STATE_DIM> P;
        // 过程噪声
        Matrix<STATE_DIM, STATE_DIM> Q;
        // 观测噪声
        Matrix<MEASURE_DIM, MEASURE_DIM> R;
    } kalman_t;

    kalman_t kalman_ = {0};
};
