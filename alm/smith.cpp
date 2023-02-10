//
// Created by Terumasa Tadano on 2023/02/09.
//
#include "smith.h"
#include <algorithm>
#include <limits>
#include <Eigen/Core>
#include <iostream>
#include <iomanip>

int gcd(const int &a, const int &b)
{
    // Compute GCD of integer a and b
    if (b == 0) return a;
    return gcd(b, a % b);
}

int exgcd(const int &a, const int &b, int &x, int &y)
{
    // Perform Extended Euclidean Algorithm to find
    // integers x and y satisfying ax + by = gcd(a,b).
    if (b == 0) {
        x = 1;
        y = 0;
        return a;
    }
    int xx, yy, gcd;
    gcd = exgcd(b, a % b, xx, yy);
    x = yy;
    y = xx - (a / b) * yy;
    return gcd;
}

bool is_lone(const Eigen::MatrixXi &A, const int s)
{
    if (s < 0) return true;

    const auto m = A.rows();
    const auto n = A.cols();
    const auto nmin = std::min(A.rows(), A.cols());
    if (s >= nmin) return true;

    for (auto i = s + 1; i < m; ++i) {
        if (abs(A(i, s)) != 0) return false;
    }
    for (auto j = s + 1; j < n; ++j) {
        if (abs(A(s, j)) != 0) return false;
    }

    return true;
}

int locate_minval_lower_right(const Eigen::MatrixXi &A, const int s, int &irow, int &icol)
{
    // Find the location of the minimum non-zero absolute value in
    // the right bottom submatrix of A(s:m, s:n)

    irow = s;
    icol = s;

    if (s < 0) return 0;

    const auto m = A.rows();
    const auto n = A.cols();
    const auto nmin = std::min(m, n);
    if (s >= nmin) return 0;

    int minval = std::numeric_limits<int>::max(); // Very big integer as an initial value

    for (auto i = s; i < m; ++i) {
        for (auto j = s; j < n; ++j) {
            if (A(i, j) != 0 && abs(A(i, j)) < minval) {
                minval = abs(A(i, j));
                irow = i;
                icol = j;
            }
        }
    }

    return minval;
}

bool check_divide_subelements(const Eigen::MatrixXi &A, const int s, int &irow, int &icol)
{

    if (s < 0) return false;

    const auto m = A.rows();
    const auto n = A.cols();

    if (s >= std::min(m, n) - 1) return true;

    for (auto i = s + 1; i < m; ++i) {
        for (auto j = s + 1; j < n; ++j) {
            if (A(i, j) % A(s, s) != 0) {
                irow = i;
                icol = j;
                return false;
            }
        }
    }
    return true;
}

void swap_rows(Eigen::MatrixXi &A, const int irow, const int jrow)
{
    const size_t m = A.rows();
    if (irow >= 0 && jrow >= 0 && irow < m && jrow < m) {
        A.row(irow).swap(A.row(jrow));
    }
}

void swap_cols(Eigen::MatrixXi &A, const int icol, const int jcol)
{
    const size_t n = A.cols();
    if (icol >= 0 && jcol >= 0 && icol < n && jcol < n) {
        A.col(icol).swap(A.col(jcol));
    }
}

void add_row_wise(Eigen::MatrixXi &A, const int irow, const int jrow, const int factor)
{
    const size_t m = A.rows();
    if (irow >= 0 && jrow >= 0 && irow < m && jrow < m) {
        A.row(irow) += factor * A.row(jrow);
    }
}

void add_col_wise(Eigen::MatrixXi &A, const int icol, const int jcol, const int factor)
{
    const size_t n = A.cols();
    if (icol >= 0 && jcol >= 0 && icol < n && jcol < n) {
        A.col(icol) += factor * A.col(jcol);
    }
}

void smith_decomposition(const Eigen::MatrixXi &A,
                         Eigen::MatrixXi &D,
                         Eigen::MatrixXi &L,
                         Eigen::MatrixXi &R)
{
    // Perform a Smith decomposition for an integer matrix A as
    // L*A*R = D where D is a diagonal matrix, and L and R are
    // unimodular matrices (determinant is +1 or -1).
    // The algorithm is decribed at https://www.dlfer.xyz/post/2016-10-27-smith-normal-form/

    const auto m = A.rows();
    const auto n = A.cols();
    const auto nmin = std::min(m, n);

    L = Eigen::MatrixXi::Identity(m, m);
    R = Eigen::MatrixXi::Identity(n, n);
    D = Eigen::MatrixXi::Zero(m, n);

    for (auto i = 0; i < m; ++i) {
        for (auto j = 0; j < n; ++j) {
            D(i, j) = A(i, j);
        }
    }

    int irow, jrow;
    int icol, jcol;

    for (auto i = 0; i < nmin; ++i) {

#ifdef _DEBUG
        std::cout << "step " << i + 1 << std::endl;
        std::cout << "Dmat:\n" << D << std::endl << std::flush;
        std::cout << "L\n" << L << std::endl;
        std::cout << "R\n" << R << std::endl;
#endif
        while (!is_lone(D, i)) {
            // Find the location (irow, icol) where the abs(D(irow, icol)) has
            // the smallest nonzero value.
            auto minval = locate_minval_lower_right(D, i, irow, icol);

#ifdef _DEBUG
            std::cout << "irow = " << std::setw(4) << irow + 1 << " icol = " << std::setw(4) << icol + 1 << std::endl;
            std::cout << "minval = " << std::setw(5) << minval << std::endl;
#endif
            // Move the smallest nonzero element to (i,i)
            swap_rows(D, i, irow);
            swap_rows(L, i, irow);
            swap_cols(D, i, icol);
            swap_cols(R, i, icol);

#ifdef _DEBUG
            std::cout << "Dmat After swap\n" << D << std::endl;
            std::cout << "L * A * R\n" << L * A * R << std::endl;
#endif
            // Subtract D(i,:) from D(j,:) (j>i)
            for (auto j = i + 1; j < m; ++j) {
                if (D(j, i) != 0) {
                    auto k = D(j, i) / D(i, i);
                    add_row_wise(D, j, i, -k);
                    add_row_wise(L, j, i, -k);
                }
            }

            // Subtract D(:,i) from D(:,j) (j>i)
            for (auto j = i + 1; j < n; ++j) {
                if (D(i, j) != 0) {
                    auto k = D(i, j) / D(i, i);
                    add_col_wise(D, j, i, -k);
                    add_col_wise(R, j, i, -k);
                }
            }

#ifdef _DEBUG
            std::cout << "Dmat after reduction\n" << D << std::endl;
            std::cout << "L * A * R\n" << L * A * R << std::endl;
            std::cout << "is_lone = " << is_lone(D, i) << std::endl;
#endif

            if (is_lone(D, i)) {
                auto divide_all = check_divide_subelements(D, i, jrow, jcol);
                if (divide_all) {
                    if (D(i, i) < 0) {
                        D.row(i) *= -1;
                        L.row(i) *= -1;
                    }
                } else {
                    add_row_wise(D, i, jrow, 1);
                    add_row_wise(L, i, jrow, 1);
                }
            }
#ifdef _DEBUG
            std::cout << "Dmat after sign-change\n" << D << std::endl;
            std::cout << "L * A * R\n" << L * A * R << std::endl;
#endif
        } // close while loop

        // if is_lone(D, i) is true from the beginning,
        // need to check if the coefficient is positive; if not make it positive.
        if (D(i, i) < 0) {
            D.row(i) *= -1;
            L.row(i) *= -1;
        }

#ifdef _DEBUG
        std::cout << "After step " << i + 1 << std::endl;
        std::cout << "Dmat:\n" << D << std::endl << std::flush;
        std::cout << "L\n" << L << std::endl;
        std::cout << "R\n" << R << std::endl;
        std::cout << "L * A * R\n" << L * A * R << std::endl;
#endif
    }

#ifdef _DEBUG
    std::cout << "D\n";
    std::cout << D << std::endl << std::flush;

    std::cout << "A\n";
    std::cout << A << std::endl;

    std::cout << "L\n";
    std::cout << L << std::endl;

    std::cout << "R\n";
    std::cout << R << std::endl;

    std::cout << "L * A * R\n";
    std::cout << L * (A * R) << std::endl;
#endif

    assert((L * A * R - D).isZero());
}




