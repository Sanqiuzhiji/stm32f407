// Formula.h
#pragma once

#include <array>
#include <vector>
#include <stdexcept>

template <size_t Rows, size_t Cols>
using Matrix = std::array<std::array<float, Cols>, Rows>;

// 直接在头文件中实现模板函数
template <typename MatrixType>
void matrix_add(const MatrixType& A, const MatrixType& B, MatrixType& C) {
    if (A.empty() || B.empty()) {
        throw std::invalid_argument("Empty matrix");
    }

    const size_t rows = A.size();
    const size_t cols = A[0].size();
    
    if (B.size() != rows || B[0].size() != cols) {
        throw std::invalid_argument("Matrix size mismatch");
    }
    
    if constexpr (std::is_same_v<MatrixType, std::vector<std::vector<float>>>) {
        C.resize(rows);
        for (auto& row : C) row.resize(cols);
    }
    
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            C[i][j] = A[i][j] + B[i][j];
        }
    }
}

template <typename MatrixA, typename MatrixB, typename MatrixC>
void matrix_mult(const MatrixA& A, const MatrixB& B, MatrixC& C) {
    if (A.empty() || B.empty()) {
        throw std::invalid_argument("Empty matrix");
    }

    const size_t m = A.size();
    const size_t n = A[0].size();
    const size_t p = B[0].size();
    
    if (B.size() != n) {
        throw std::invalid_argument("Matrix dimensions incompatible for multiplication");
    }
    
    if constexpr (std::is_same_v<MatrixC, std::vector<std::vector<float>>>) {
        C.resize(m);
        for (auto& row : C) row.resize(p);
    }
    
    for (size_t i = 0; i < m; ++i) {
        for (size_t j = 0; j < p; ++j) {
            float sum = 0.0f;
            for (size_t k = 0; k < n; ++k) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}

template <size_t N>
bool matrix_inverse(const Matrix<N, N>& A, Matrix<N, N>& A_inv) {
    static_assert(N == 2, "Currently only supports 2x2 matrix inversion");
    
    const float det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
    if (det == 0) return false;

    const float inv_det = 1.0f / det;
    A_inv[0][0] =  A[1][1] * inv_det;
    A_inv[0][1] = -A[0][1] * inv_det;
    A_inv[1][0] = -A[1][0] * inv_det;
    A_inv[1][1] =  A[0][0] * inv_det;
    return true;
}