#ifndef CENTROID_INIT_METHOD_HPP
#define CENTROID_INIT_METHOD_HPP

#include <vector>
#include <fstream>
#include <cstddef>
#include "geometry/point/Point.hpp"
#include "geometry/point/CentroidPoint.hpp"

/**
 * @class CentroidInitMethod
 * @brief A class for managing the initialization of centroids and computing cluster centroids.
 */
template <typename PT, std::size_t PD>
class CentroidInitMethod {
public:
    // Virtual destructor
    virtual ~CentroidInitMethod() = default;

    /**
     * @brief Constructor: Initialize centroids using the dataset.
     * @param data The dataset from which to initialize centroids.
     */
    explicit CentroidInitMethod(const std::vector<Point<PT, PD>>& data);

    /**
     * @brief Constructor: Initialize centroids using the dataset and the number of clusters.
     * @param data The dataset from which to initialize centroids.
     * @param k The number of centroids to initialize.
     */
    explicit CentroidInitMethod(const std::vector<Point<PT, PD>>& data, int k);

    /**
     * @brief Abstract method to find a centroid given a set of centroids.
     * @param centroids The centroids to process.
     */
    virtual void findCentroid(std::vector<CentroidPoint<PT, PD>>& centroids) = 0;

    /**
     * @brief a mesh to a CSV file without densities
     * @param points vector of points to export
     * @param name_csv name of the file csv
     */
    static void exportedMesh(const std::vector<Point<PT, PD>>& points,const std::string& name_csv);

    /**
     * @brief a mesh to a CSV file without densities
     * @param points vector of points to export
     * @param name_csv name of the file csv
     */
    static void exportedMesh(const std::vector<CentroidPoint<PT, PD>>& points,const std::string& name_csv);


    /**
     * @brief It truncates double value for small cv
     * @param value value to be truncated
     */
    static double truncateToThreeDecimals(double value);
    
protected:
    std::vector<Point<PT, PD>> m_data; ///< Dataset
    std::size_t m_k = 0;                       ///< Number of clusters

    // Setter for `m_k`
    void set_k(std::size_t k);
};


#endif // CENTROID_INIT_METHOD_HPP
