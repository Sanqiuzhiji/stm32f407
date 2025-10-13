#include "QuaternionFilter.h"

QuaternionFilter::QuaternionFilter()
    : q0_(1.0f), q1_(0.0f), q2_(0.0f), q3_(0.0f),
      beta_(0.1f), sampleFreq_(1000.0f)
{
} // 默认1kHz采样率

void QuaternionFilter::update(float ax, float ay, float az,
                              float gx, float gy, float gz)
{
    madgwickUpdate(ax, ay, az, gx, gy, gz);
}

void QuaternionFilter::madgwickUpdate(float ax, float ay, float az,
                                      float gx, float gy, float gz)
{
    // 规范化加速度计数据
    float norm = sqrt(ax * ax + ay * ay + az * az);
    if (norm == 0.0f) return;
    ax /= norm;
    ay /= norm;
    az /= norm;

    // 计算目标方向
    float _2q0 = 2.0f * q0_;
    float _2q1 = 2.0f * q1_;
    float _2q2 = 2.0f * q2_;
    float _2q3 = 2.0f * q3_;
    float _4q0 = 4.0f * q0_;
    float _4q1 = 4.0f * q1_;
    float _4q2 = 4.0f * q2_;
    float _8q1 = 8.0f * q1_;
    float _8q2 = 8.0f * q2_;
    float q0q0 = q0_ * q0_;
    float q1q1 = q1_ * q1_;
    float q2q2 = q2_ * q2_;
    float q3q3 = q3_ * q3_;

    // 梯度下降算法
    float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
    float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1_ - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
    float s2 = 4.0f * q0q0 * q2_ + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
    float s3 = 4.0f * q1q1 * q3_ - _2q1 * ax + 4.0f * q2q2 * q3_ - _2q2 * ay;

    // 规范化步进
    norm = std::sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    if (norm == 0.0f) return;
    s0 /= norm;
    s1 /= norm;
    s2 /= norm;
    s3 /= norm;

    // 计算陀螺仪方向误差
    float qDot1 = 0.5f * (-q1_ * gx - q2_ * gy - q3_ * gz);
    float qDot2 = 0.5f * (q0_ * gx + q2_ * gz - q3_ * gy);
    float qDot3 = 0.5f * (q0_ * gy - q1_ * gz + q3_ * gx);
    float qDot4 = 0.5f * (q0_ * gz + q1_ * gy - q2_ * gx);

    // 应用反馈
    qDot1 -= beta_ * s0;
    qDot2 -= beta_ * s1;
    qDot3 -= beta_ * s2;
    qDot4 -= beta_ * s3;

    // 积分四元数
    q0_ += qDot1 * (1.0f / sampleFreq_);
    q1_ += qDot2 * (1.0f / sampleFreq_);
    q2_ += qDot3 * (1.0f / sampleFreq_);
    q3_ += qDot4 * (1.0f / sampleFreq_);

    // 规范化四元数
    norm = std::sqrt(q0_ * q0_ + q1_ * q1_ + q2_ * q2_ + q3_ * q3_);
    if (norm == 0.0f) return;
    q0_ /= norm;
    q1_ /= norm;
    q2_ /= norm;
    q3_ /= norm;
}

void QuaternionFilter::getQuaternion(float& q0, float& q1, float& q2, float& q3) const
{
    q0 = q0_;
    q1 = q1_;
    q2 = q2_;
    q3 = q3_;
}

void QuaternionFilter::getEulerAngles(float& roll, float& pitch, float& yaw) const
{
    // 转换为欧拉角 (弧度)
    roll = std::atan2(2.0f * (q0_ * q1_ + q2_ * q3_), 1.0f - 2.0f * (q1_ * q1_ + q2_ * q2_));
    pitch = std::asin(2.0f * (q0_ * q2_ - q3_ * q1_));
    yaw = std::atan2(2.0f * (q0_ * q3_ + q1_ * q2_), 1.0f - 2.0f * (q2_ * q2_ + q3_ * q3_));

    // 转换为角度
    constexpr float rad2deg = 57.295779513f;
    roll *= rad2deg;
    pitch *= rad2deg;
    yaw *= rad2deg;
}
