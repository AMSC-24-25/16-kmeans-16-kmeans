#include <vector>

#include <iostream>
#include "../include/metrics.hpp"
#include "../include/KMeans.hpp"
#include "../include/CSVUtils.hpp"
#include "../include/csv-parser/single_include/csv.hpp"
#include "../include/matplotlib-cpp/matplotlibcpp.h"

namespace plt = matplotlibcpp;

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

        DistanceMetric euclMetric = euclideanMetric;
        KMeans<double, DIMENSION> kmeans(num_clusters, points, euclMetric, 1e-4);

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
