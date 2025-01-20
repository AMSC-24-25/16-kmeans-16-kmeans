#ifndef KDE3D_HPP
#define KDE3D_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include <Eigen/Dense>
#include <stdexcept>

#include "matplotlib-cpp/matplotlibcpp.h"
#include "geometry/point/Point.hpp"
#include "clustering/CentroidInitializationMethods/CentroidInitMethods.hpp"

#define BANDWIDTHMETHODS 0
#define RANGE_NUMBER_DIVISION 4
#define NUMBER_RAY_STEP 3
#define PDS 3

using Grid3D = std::vector<std::vector<std::vector<Point<double, 3>>>>;
using Densities3D = std::vector<std::vector<std::vector<double>>>;

namespace plt = matplotlibcpp;

using namespace std;
using namespace Eigen;

class KDE3D : public CentroidInitMethod<double, 3> {
public:

    KDE3D(std::vector<Point<double, 3>>& data, int k) : CentroidInitMethod<double, 3>(data, k) {
        this->m_h = bandwidth_RuleOfThumb();
    }

    KDE3D(std::vector<Point<double, 3>>& data) : CentroidInitMethod<double, 3>(data) {
        this->m_h = bandwidth_RuleOfThumb();
    }

    void findCentroid(std::vector<CentroidPoint<double, 3>>& centroids) override;

private:
    int m_bandwidthMethods;
    std::size_t m_totalPoints;
    double m_h_det_sqrt;
    Eigen::MatrixXd m_h;
    MatrixXd m_h_sqrt_inv;
    std::vector<Eigen::VectorXd> m_transformedPoints;
    std::array<double, 3> m_range;
    std::array<double, 3> m_step;
    std::array<size_t, 3> m_numPoints;

    // Gaussian kernel
    double gaussianKernel(const Eigen::VectorXd& u);

    // Mean and standard deviation
    std::pair<double, double> computeMeanAndStdDev(int dim);

    // Bandwidth matrix calculation
    Eigen::MatrixXd bandwidth_RuleOfThumb();

    // KDE3D value calculation
    double kdeValue(const Point<double, 3>& x);

    // Convert Point to VectorXd
    Eigen::VectorXd pointToVector(const Point<double, 3>& point);

    // Generate grid
    Grid3D generateGrid();

    // Find local maxima in the grid
    std::vector<Point<double, PDS>> findLocalMaxima(Grid3D& gridPoints);

    // Check if a point is a local maximum
    bool isLocalMaximum(Grid3D& gridPoints, Densities3D& densities, size_t x, size_t y, size_t z);

};


void KDE3D::findCentroid(std::vector<CentroidPoint<double, 3>>& centroids) {

        // Generate the grid points based on the calculated ranges and steps
        Grid3D gridPoints = generateGrid();

        // Find the peaks (local maxima) in the grid
        std::vector<Point<double, PDS>> peaks = findLocalMaxima(gridPoints);

        int i = 0;
        for (auto& p : peaks)
        {
            centroids.push_back(CentroidPoint<double, PDS>(p));
            centroids[i].setID(i);
            i++;
        }
        return;
}

    /*This function allows the creation of a grid in the multidimensional space where the points to be classified reside. 
    Once the maximum and minimum values are found for each dimension, 
    each range is divided into steps. Each step in every dimension represents a point, 
    and this point will be used to calculate the density */
    Grid3D KDE3D::generateGrid() {

        std::array<double, 3> minValues = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
        std::array<double, 3> maxValues = {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};

        for (const auto& point : this->m_data) {
            for (size_t dim = 0; dim < PDS; ++dim) {
                minValues[dim] = std::min(minValues[dim], point.coordinates[dim]);
                maxValues[dim] = std::max(maxValues[dim], point.coordinates[dim]);
            }
        }

        
        for (size_t dim = 0; dim < PDS; ++dim) {
            m_range[dim] = maxValues[dim] - minValues[dim];
            m_step[dim] = m_range[dim] / RANGE_NUMBER_DIVISION;
            m_numPoints[dim] = static_cast<size_t>(m_range[dim] / m_step[dim]) + 1;
        }

        Grid3D grid(
            m_numPoints[0],
            std::vector<std::vector<Point<double, PDS>>>(
                m_numPoints[1],
                std::vector<Point<double, PDS>>(m_numPoints[2])
            )
        );

        for (size_t x = 0; x < m_numPoints[0]; ++x) {
            for (size_t y = 0; y < m_numPoints[1]; ++y) {
                for (size_t z = 0; z < m_numPoints[2]; ++z) {
                    Point<double, PDS> point;

                    point.coordinates[0] = minValues[0] + x * m_step[0];
                    point.coordinates[1] = minValues[1] + y * m_step[1];
                    point.coordinates[2] = minValues[2] + z * m_step[2];

                    grid[x][y][z] = point;
                }
            }
        }

        return grid; // Ritorna la griglia tridimensionale
    }



    /* Multivariate Gaussian Kernel Function.
    This function evaluates the Gaussian kernel at a given vector `u`.
    The Gaussian kernel is used to compute the contribution of a data point
    to the kernel density estimate (KDE3D). */
    double KDE3D::gaussianKernel(const VectorXd& u) {
        // Compute the squared norm (||u||^2)
        double norm = u.squaredNorm();
        
        // Compute the normalization coefficient: 1 / (2 * PI)^(d/2)
        double coeff = 1.0 / pow(2 * M_PI, PDS / 2.0);
        
        // Return the Gaussian kernel value: coeff * exp(-||u||^2 / 2)
        return coeff * exp(-0.5 * norm);
    }


    /* Calculation of Mean and Standard Deviation.
    This function computes the mean and standard deviation of the points in the dataset
    along a specific dimension `dim`.*/
    std::pair<double, double> KDE3D::computeMeanAndStdDev(int dim) {
        double sum = 0.0, sumSquares = 0.0; // Initialize sum and sum of squares
        int n = (this->m_data).size(); // Number of points in the dataset

        // Iterate over all points and compute the sum and sum of squares for the given dimension
        // A possible alternative cloud be use a reduce pattern
        for (const auto& point : this->m_data) {
            double value = point.coordinates[dim]; // Extract the value in the specified dimension
            sum += value;
            sumSquares += value * value;
        }

        // Compute the mean
        double mean = sum / n;

        // Compute the variance
        double variance = (sumSquares / n) - (mean * mean);

        // Compute the standard deviation (square root of variance)
        double stdDev = sqrt(variance);

        return {mean, stdDev}; // Return the results as a pair
    }

    /* Calculation of the bandwidth matrix using the Rule of Thumb method.
    This method estimates the bandwidth for each dimension based on the 
    standard deviation of the data and the number of points. */
    Eigen::MatrixXd KDE3D::bandwidth_RuleOfThumb() {
        int n = (this->m_data).size(); // Number of points in the dataset

        VectorXd bandwidths(PDS); // Vector to store the bandwidths for each dimension
        for (int i = 0; i < PDS; ++i) {
            // Compute the mean and standard deviation for the current dimension
            auto [mean, stdDev] = computeMeanAndStdDev(i);
            // Rule of Thumb formula: h_ii = stdDev * n^(-1/(d+4)) * (4 / d + 2)
            bandwidths[i] = stdDev * pow(n, -1.0 / (PDS + 4)) * pow(4.0 / (PDS + 2), 1.0 / (PDS + 4));
        }

        // Create a diagonal matrix from the squared bandwidth values
        MatrixXd bandwidthMatrix = bandwidths.array().square().matrix().asDiagonal();

        // Compute necessary components for KDE3D
        SelfAdjointEigenSolver<MatrixXd> solver(bandwidthMatrix);
        this->m_h_sqrt_inv = solver.operatorInverseSqrt();                  // Inverse square root of the bandwidth matrix
        this->m_h_det_sqrt = sqrt(bandwidthMatrix.determinant());           // Square root of the determinant of the bandwidth matrix
        m_transformedPoints.clear();
        for (const auto& xi : this->m_data) {
            Eigen::VectorXd transformed = m_h_sqrt_inv * pointToVector(xi);
            m_transformedPoints.push_back(transformed);
        }
        return bandwidthMatrix; // Return the bandwidth matrix
    }



    /* This defines the actual function. "x" is the independent variable, and "data" 
       represents the set of points required for the calculation.
       It simply computes the kernel density estimate (KDE3D) for the given input. 
        * f(x) = (1 / (n * h)) * Σ K((x - x_i) / h) for i = 1 to n
        * 
        * Where:
        * - f(x): The estimated density at point x.
        * - n: The total number of data points.
        * - h: The bandwidth (or smoothing parameter) controlling the kernel's width.
        * - x_i: The i-th data point in the dataset.
        * - K(u): The kernel function, typically a symmetric and normalized function.
     */
    double KDE3D::kdeValue(const Point<double, PDS>& x) {
        if (m_h.rows() == 0 || m_h.cols() == 0) {
            throw std::runtime_error("Bandwidth matrix is not initialized.");
        }
       
        Eigen::VectorXd transformedQuery = m_h_sqrt_inv * pointToVector(x);

        double density = 0.0;
        for (const auto& transformedPoint : m_transformedPoints) {
            Eigen::VectorXd diff = transformedQuery - transformedPoint;
            density += gaussianKernel(diff);
        }
        density /= (m_transformedPoints.size() * m_h_det_sqrt);
        return density;
    }

    /* Converte un Point in un VectorXd */
    Eigen::VectorXd KDE3D::pointToVector(const Point<double, PDS>& point) {
        VectorXd vec(PDS);
        for (std::size_t i = 0; i < PDS; ++i) {
            vec[i] = point.coordinates[i];
        }
        return vec;
    }

    // Find local maxima in the grid
    std::vector<Point<double, PDS>> KDE3D::findLocalMaxima(Grid3D& gridPoints) {

        std::vector<std::pair<Point<double, PDS>, double>> maximaPD;
        std::vector<Point<double, PDS>> returnVec;

        Densities3D densities(
            m_numPoints[0],
            std::vector<std::vector<double>>(
                m_numPoints[1],
                std::vector<double>(m_numPoints[2])
            )
        );

        int countCicle = 0;

        while (true) {
            std::cout<<"Counter: "<<countCicle<<std::endl;

            for (size_t x = 0; x < densities.size(); ++x) {
                for (size_t y = 0; y < densities[x].size(); ++y) {
                    for (size_t z = 0; z < densities[x][y].size(); ++z) {
                        densities[x][y][z] = kdeValue(gridPoints[x][y][z]); 
                    }
                }
            }

            for (size_t x = 0; x < gridPoints.size(); ++x) {
                for (size_t y = 0; y < gridPoints[x].size(); ++y) {
                    for (size_t z = 0; z < gridPoints[x][y].size(); ++z) {
                        if (isLocalMaximum(gridPoints, densities, x, y, z)) {
                            maximaPD.emplace_back(gridPoints[x][y][z], densities[x][y][z]);
                        }
                    }
                }
            }
        

            // Check if we need to adjust the bandwidth matrix
            if (this->m_k != 0 && maximaPD.size() < this->m_k) {

                densities.clear();
                maximaPD.clear();

                // Reduce the bandwidth
                m_h.diagonal() *= 0.85;

                // Recompute derived parameters
                SelfAdjointEigenSolver<MatrixXd> solver(m_h);
                this->m_h_sqrt_inv = solver.operatorInverseSqrt();
                this->m_h_det_sqrt = sqrt(m_h.determinant());

                m_transformedPoints.clear();
                for (const auto& xi : this->m_data) {
                    Eigen::VectorXd transformed = m_h_sqrt_inv * pointToVector(xi);
                    m_transformedPoints.push_back(transformed);
                }

            } else {
                // Too many maxima or just enough
                if (this->m_k != 0 && maximaPD.size() > this->m_k) {

                    std::sort(maximaPD.begin(), maximaPD.end(),
                            [](const auto& a, const auto& b) {
                                return a.second > b.second;
                            });

                    maximaPD.resize(this->m_k);
                }

                // Copy the maxima to the return vector
                for (const auto& pair : maximaPD) {
                    returnVec.push_back(pair.first);
                }
                break;
            }
            countCicle++;
        }

        return returnVec; // Return the list of local maxima
    }


    // Check if a point is a local maximum
    bool KDE3D::isLocalMaximum(Grid3D& gridPoints, Densities3D& densities, size_t x,size_t y,size_t z) {
        double currentDensity = densities[x][y][z];
        for (int dx = -NUMBER_RAY_STEP; dx <= NUMBER_RAY_STEP; ++dx) {
            for (int dy = -NUMBER_RAY_STEP; dy <= NUMBER_RAY_STEP; ++dy) {
                for (int dz = -NUMBER_RAY_STEP; dz <= NUMBER_RAY_STEP; ++dz) {

                    if (dx == 0 && dy == 0 && dz == 0) {
                        continue;
                    }

                    size_t nx = x + dx;
                    size_t ny = y + dy;
                    size_t nz = z + dz;

                    if (nx < gridPoints.size() && ny < gridPoints[nx].size() && nz < gridPoints[nx][ny].size()) {
                        if (densities[nx][ny][nz] > currentDensity) {
                            return false; 
                        }
                    }
                }
            }
        }
        return true;
    }

#endif
