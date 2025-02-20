#ifndef ELBOW
#define ELBOW

#include <cstddef> 
#include "clustering/CentroidInitializationMethods/kInitMethods.hpp"

#define MAX_CLUSTER 10

// Forward declaration of KMeans to avoid cyclic dependencies
template <typename PT, std::size_t PD, class M>
class KMeans;

// Forward declaration of Kinit to ensure it is available as a base class
template<typename PT, std::size_t PD, class M>
class Kinit;

/**
 * \class ElbowMethod
 * \brief Implements the "elbow method" to determine the optimal number of clusters.
 *
 * This class inherits from Kinit and is responsible for computing the optimal number of clusters
 * using the elbow method, which analyzes the Within-Cluster Sum of Squares (WCSS).
 *
 * \tparam PT Data type of the points.
 * \tparam PD Dimensionality of the data.
 * \tparam M Metric or distance measure used.
 */
template<typename PT, std::size_t PD, class M>
class ElbowMethod : public Kinit<PT, PD, M> {
public:
    /**
     * \brief Constructor initializes ElbowMethod with a reference to KMeans.
     *
     * \param kMeans A reference to the KMeans instance used for clustering.
     */
    ElbowMethod(const KMeans<PT, PD, M>& kMeans) : Kinit<PT, PD, M>(kMeans) {}

    /**
     * \brief Determines the optimal number of clusters using the elbow method.
     *
     * This method analyzes the changes in the Within-Cluster Sum of Squares (WCSS) to
     * determine the best value for k.
     *
     * \return The optimal number of clusters.
     */
    int findK();
    
private:
    std::vector<PT> wcss; ///< Stores the Within-Cluster Sum of Squares (WCSS) for each k.
};

// Implementation of the findK method
template<typename PT, std::size_t PD, class M>
int ElbowMethod<PT, PD, M>::findK() {

    int k = 1; // Start with k = 1 cluster
    double epsilon = 1e-2; // Convergence threshold for WCSS change
    int optimalK = 0; // Variable to store the optimal number of clusters

    // Retrieve the points from KMeans
    std::vector<Point<PT, PD>> points = (this->m_kMeans).getPoints();
    std::cout << "Start searching k...\n";

    for(int i = 0; i < MAX_CLUSTER ; i++) { // Infinite loop to incrementally search for the optimal k

        MostDistanceClass mdc(points, k); 
        std::vector<CentroidPoint<PT, PD>>& pointerCentroids = (this->m_kMeans).getCentroids();
        mdc.findCentroid(pointerCentroids);
        (this->m_kMeans).setNumClusters(static_cast<std::size_t>(k));
        (this->m_kMeans).fit();
        points = (this->m_kMeans).getPoints();
        double sum = 0; 

        #pragma omp parallel for reduction(+:sum)
        for (size_t i = 0; i < points.size(); ++i) {
            Point<PT, PD> centroidTmp = *(points[i].centroid);
            sum += std::pow(EuclideanMetric<PT, PD>::distanceTo(points[i], centroidTmp), 2);
        }

        std::cout << "K: " << k << ", WCSS: " << sum << std::endl;
        (this->wcss).push_back(sum); 

        if(i > 1 && wcss[i-1] > wcss[i-2])
            break;

        (this->m_kMeans).resetCentroids();
        k++; 
    }

    double maxCurvature = -std::numeric_limits<double>::max();

    for (size_t k = 1; k < wcss.size() - 1; ++k) {
        double secondDerivative = wcss[k - 1] - 2 * wcss[k] + wcss[k + 1];

        if (std::abs(secondDerivative) > maxCurvature) {
            maxCurvature = std::abs(secondDerivative);
            optimalK = k + 1; 
        }
    }

    return optimalK; // Return the optimal number of clusters
}

#endif