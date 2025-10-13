#pragma once

#include "../../main_link.h"

class QuaternionFilter
{
public:
    QuaternionFilter();
    void update(float ax, float ay, float az, float gx, float gy, float gz);
    void getQuaternion(float& q0, float& q1, float& q2, float& q3) const;
    void getEulerAngles(float& roll, float& pitch, float& yaw) const;

private:
    float q0_, q1_, q2_, q3_; // 四元数
    float beta_; // 算法增益系数
    float sampleFreq_; // 采样频率 (Hz)

    void madgwickUpdate(float ax, float ay, float az, float gx, float gy, float gz);
};
