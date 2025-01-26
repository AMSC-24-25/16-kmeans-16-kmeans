#include <vector>

#include <iostream>

#include "utils/CSVUtils.hpp"
#include "clustering/KMeans.hpp"
#include "geometry/metrics/EuclideanMetric.hpp"

#define DIMENSION 2

using namespace std;

int main()
{

    try
    {
        string file_name;
        cout << "Enter the name of the csv file: ";
        cin >> file_name;

        // Append file_name to /app/resources/
        std::string full_path = "/app/resources/" + file_name;

        int num_clusters;
        cout << "Enter the number of clusters (parameter k): ";
        cin >> num_clusters;

        int num_initialization_method;
        std::cout << "Enter the number of initialization method for centroids \n (0: random, 1: kernel density estimator, 2: most distant)" <<std::endl;
        cin >> num_initialization_method;

        // Reading from the file
        std::vector<Point<double, DIMENSION>> points;

        try
        {
            points = CSVUtils::readCSV<double, DIMENSION>(full_path);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to read CSV: " << e.what() << '\n';
            std::cerr << "Ensure the file exists at: " << full_path << '\n';
            return 1;
        }

        EuclideanMetric<double, DIMENSION> metric(points, 1e-4);
        KMeans<double, DIMENSION, EuclideanMetric<double, DIMENSION>> kmeans(num_clusters, points, 1e-4, &metric, num_initialization_method);

        kmeans.fit();
        kmeans.print();

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
