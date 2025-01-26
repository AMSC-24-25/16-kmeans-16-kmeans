#include "objload.h"
#include "geometry/mesh/Mesh.hpp"
#include <fstream> // For file output
#include <sstream> // For stringstream

Mesh::Mesh(const std::string path)
{
  try
  {
    std::ifstream in(path.c_str());
    auto model = obj::parseObjModel(in);
    for (int i = 0; i < model.vertex.size(); i = i + 3)
    {
      std::array<double, 3> coords = {model.vertex[i], model.vertex[i + 1], model.vertex[i + 2]};
      Point<double, 3> point(coords, i / 3);
      meshVertices.push_back(point);
    }

    const auto faces = model.faces.at("default");
    const auto &faceVertices = faces.first;

    for (int j = 0; j < faceVertices.size(); j = j + 3)
    {
      FaceId faceId(j / 3);
      Face face = Face({VertId(faceVertices[j].v), VertId(faceVertices[j + 1].v), VertId(faceVertices[j + 2].v)}, meshVertices, faceId);

      meshFaces.push_back(face);
    }
  }
  catch (const std::exception &e)
  {
    throw std::runtime_error("Failed to load file");
  }
}

std::ostream &operator<<(std::ostream &os, const Mesh &graph)
{
  os << "Vertices: " << std::endl;
  for (const auto &vertex : graph.meshVertices)
  {
    os << vertex << std::endl;
  }
  return os;
}

int Mesh::createSegmentationFromSegFile(const std::filesystem::path &path)
{
  // check if the file has .seg extension
  if (path.extension() != ".seg")
  {
    throw std::invalid_argument("File must have .seg extension");
  }

  std::ifstream file(path);
  if (!file.is_open())
  {
    throw std::runtime_error("Failed to open file");
  }

  std::string line;
  int faceId = 0;
  int maxCluster = 0;
  while (std::getline(file, line))
  {
    std::istringstream iss(line);
    int cluster;
    if (!(iss >> cluster))
    {
      break;
    } // error

    this->setFaceCluster(FaceId(faceId), cluster);

    if (cluster > maxCluster)
    {
      maxCluster = cluster;
    }
    faceId++;
  }

  return maxCluster + 1;
}

void Mesh::buildFaceAdjacency()
{
  faceAdjacency.clear();

  // Temporary map to store the faces connected to each vertex
  std::unordered_map<VertId, std::set<FaceId>> vertexToFaces;

  // Populates the vertexToFaces map
  for (const auto &face : meshFaces)
  {
    for (VertId vertex : face.vertices)
    {
      vertexToFaces[vertex].insert(face.baricenter.id);
    }
  }

  // Creates the adjacency list
  for (const auto &face : meshFaces)
  {
    std::set<FaceId> adjacentFacesSet;

    for (VertId vertex : face.vertices)
    {
      const auto &connectedFaces = vertexToFaces[vertex];
      adjacentFacesSet.insert(connectedFaces.begin(), connectedFaces.end());
    }

    // Remove the face itself from the set
    adjacentFacesSet.erase(face.baricenter.id);

    // Save in the final adjacency map
    faceAdjacency[face.baricenter.id] = std::vector<FaceId>(adjacentFacesSet.begin(), adjacentFacesSet.end());
  }
}
void Mesh::exportToObj(const std::string &filepath, int cluster)
{
  std::ofstream objFile(filepath);

  if (!objFile.is_open())
  {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    return;
  }

  // Write the vertices to the file (in .obj format)
  for (const auto &vertex : meshVertices)
  {
    objFile << "v " << vertex.coordinates[0] << " " << vertex.coordinates[1] << " " << vertex.coordinates[2] << std::endl;
  }

  for (FaceId faceId = 0; faceId < meshFaces.size(); ++faceId)
  {
    if (getFaceCluster(faceId) == cluster)
    { // Check if the cluster ID is 0
      const Face &face = meshFaces[faceId];

      // Write the face in OBJ format (note that OBJ uses 1-based indexing)
      objFile << "f";
      for (const auto &vertId : face.vertices)
      {
        objFile << " " << (vertId + 1); // OBJ indices are 1-based
      }
      objFile << std::endl;
    }
  }

  objFile.close();
  std::cout << "Exported mesh to " << filepath << std::endl;
}

void Mesh::exportToGroupedObj(const std::string &filepath) const
{
  std::ofstream objFile(filepath);

  if (!objFile.is_open())
  {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    return;
  }

  // Write the vertices to the file (in .obj format)
  objFile << "# Vertices\n";
  for (const auto &vertex : meshVertices)
  {
    objFile << "v " << vertex.coordinates[0] << " "
            << vertex.coordinates[1] << " "
            << vertex.coordinates[2] << std::endl;
  }

  // Initialize current cluster and group
  int currentCluster = -1;

  objFile << "# Faces grouped by clusters\n";
  for (FaceId faceId = 0; faceId < meshFaces.size(); ++faceId)
  {
    int cluster = getFaceCluster(faceId);

    // If cluster changes, write a new group
    if (cluster != currentCluster)
    {
      currentCluster = cluster;
      objFile << "\ng cluster_" << currentCluster << std::endl;
    }

    // Write the face in OBJ format (note that OBJ uses 1-based indexing)
    const Face &face = meshFaces[faceId];
    objFile << "f";
    for (const auto &vertId : face.vertices)
    {
      objFile << " " << (vertId + 1); // OBJ indices are 1-based
    }
    objFile << std::endl;
  }

  objFile.close();
  std::cout << "Exported grouped mesh to " << filepath << std::endl;
}
