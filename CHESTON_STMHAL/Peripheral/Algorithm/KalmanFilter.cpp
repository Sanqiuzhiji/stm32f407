#include "KalmanFilter.h"
#include "Formula.h"

// 初始化卡尔曼滤波器
void KalmanFilter::Init(float Q_angle, float Q_bias, float R_measure)
{
    // 初始化状态变量
    kalman_.x.fill(0.0f);

    // 初始化误差协方差矩阵
    for (auto& row : kalman_.P) row.fill(0.0f);

    // 初始化过程噪声
    kalman_.Q = {
        {
            {Q_angle, 0.0f},
            {0.0f, Q_bias}
        }
    };

    // 初始化观测噪声
    kalman_.R = {{{{R_measure}}}};
}

// 卡尔曼滤波更新
float KalmanFilter::Update(float z, float gyroRate, float dt)
{
    // 1. 预测状态估计: 角度预测 (θ = θ + dt * (w - bias))
    kalman_.x[0] += dt * (gyroRate - kalman_.x[1]);

    /* 2. 预测误差协方差 P = F * P * F^T + Q */
    const Matrix<STATE_DIM, STATE_DIM> F = {
        {
            {1.0f, -dt},
            {0.0f, 1.0f}
        }
    };
    // F 的转置
    Matrix<STATE_DIM, STATE_DIM> FT = {
        {
            {1.0f, 0.0f},
            {-dt, 1.0f}
        }
    };
    Matrix<STATE_DIM, STATE_DIM> tempP;

    matrix_mult(F, kalman_.P, tempP);
    matrix_mult(tempP, FT, kalman_.P);

    // 添加过程噪声
    Matrix<STATE_DIM, STATE_DIM> tempQ = {
        {
            {dt * kalman_.Q[0][0], 0.0f},
            {0.0f, dt * kalman_.Q[1][1]}
        }
    };
    matrix_add(kalman_.P, tempQ, kalman_.P);

    //这是上面代码的数学推导式
    // kalman_.P[0][0] += dt * (dt*kalman_.P[1][1] - kalman_.P[0][1] - kalman_.P[1][0] + kalman_.Q[0][0]);
    // kalman_.P[0][1] -= dt * kalman_.P[1][1];
    // kalman_.P[1][0] -= dt * kalman_.P[1][1];
    // kalman_.P[1][1] += kalman_.Q[1][1] * dt;

    // 3. 计算 Kalman 增益 K = P * H^T * (H * P * H^T + R)^-1
    const Matrix<MEASURE_DIM, STATE_DIM> H = {
        {
            {1.0f, 0.0f}
        }
    }; // 观测矩阵
    Matrix<STATE_DIM, MEASURE_DIM> HT = {
        {
            {1.0f},
            {0.0f}
        }
    }; // H 的转置

    Matrix<MEASURE_DIM, STATE_DIM> tempS;
    matrix_mult(H, kalman_.P, tempS);

    Matrix<MEASURE_DIM, MEASURE_DIM> S;
    matrix_mult(tempS, HT, S);
    matrix_add(S, kalman_.R, S);

    Matrix<STATE_DIM, MEASURE_DIM> K;
    matrix_mult(kalman_.P, HT, K);

    // 计算 Kalman 增益
    const float inv_S = 1.0f / S[0][0];
    K[0][0] *= inv_S;
    K[1][0] *= inv_S;

    // 4. 计算状态更新 x = x + K * (z - H * x)
    const float y = z - kalman_.x[0];
    kalman_.x[0] += K[0][0] * y;
    kalman_.x[1] += K[1][0] * y;

    // 5. 计算误差协方差更新 P = (I - K * H) * P
    Matrix<STATE_DIM, STATE_DIM> KH;
    matrix_mult(K, H, KH);

    Matrix<STATE_DIM, STATE_DIM> I_KH;
    for (size_t i = 0; i < STATE_DIM; ++i)
    {
        for (size_t j = 0; j < STATE_DIM; ++j)
        {
            I_KH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];
        }
    }

    matrix_mult(I_KH, kalman_.P, kalman_.P);

    return kalman_.x[0];
}
