#ifndef METRICS_HPP
#define METRICS_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>

/**
 * Abstract class representing a generic metric for calculating distances.
 */
template <typename PT, std::size_t PD>
class Metric
{
public:
    // Pure virtual function to calculate distance
    virtual PT distanceTo(const Point<PT, PD>& a, const Point<PT, PD>& b) const = 0;

    // Virtual destructor to ensure proper cleanup in derived classes
    virtual ~Metric() = default;
};

/**
 * Euclidean metric: calculates straight-line distance.
 */
template <typename PT, std::size_t PD>
class EuclideanMetric : public Metric<PT, PD>
{
public:
    PT distanceTo(const Point<PT, PD>& a, const Point<PT, PD>& b) const override
    {
        if (a.coordinates.size() != a.coordinates.size())
        {
            throw std::invalid_argument("Points must have the same dimensionality");
        }
        PT sum = 0;
        for (size_t i = 0; i < a.coordinates.size(); ++i)
        {
            sum += std::pow(a.coordinates[i] - b.coordinates[i], 2);
        }
        return std::sqrt(sum);
    }
};

#endif