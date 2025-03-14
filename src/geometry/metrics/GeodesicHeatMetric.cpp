#include "geometry/metrics/GeodesicHeatMetric.hpp"

template <typename PT, std::size_t PD>
GeodesicHeatMetric<PT, PD>::GeodesicHeatMetric(Mesh &mesh, double percentage_threshold, std::vector<Point<PT, PD>> data)
    : GeodesicDijkstraMetric<PT, PD>(mesh, percentage_threshold, data)
{
    const std::vector<Point<double, 3>> vertices = mesh.getVertices();
    Eigen::MatrixXd V(vertices.size(), 3);
    #pragma omp parallel for
    for (int i = 0; i < vertices.size(); i++)
    {
        for (int j = 0; j < 3; j++)
        {
            V(i, j) = vertices[i].coordinates[j];
        }
    }

    Eigen::MatrixXi F(mesh.numFaces(), 3);
    #pragma omp parallel for
    for (FaceId faceId = 0; faceId < mesh.numFaces(); ++faceId)
    {
        const auto &face = mesh.getFace(faceId);
        for (int i = 0; i < 3; i++)
        {
            F(faceId, i) = face.vertices[i];
        }
    }
    
    Eigen::SparseMatrix<PT> L, M;
    VectorXS dblA;

    #pragma omp parallel
    {
        #pragma omp single nowait
        igl::cotmatrix(V, F, L);

        #pragma omp single nowait
        igl::massmatrix(V, F, igl::MASSMATRIX_TYPE_DEFAULT, M);

        #pragma omp single nowait
        igl::doublearea(V, F, dblA);

        #pragma omp single nowait
        igl::grad(V, F, data_heat.Grad);
    }

    const PT h = igl::avg_edge_length(V, F);
    const PT t = h * h;

    assert(F.cols() == 3 && "Only triangles are supported");
    data_heat.ng = data_heat.Grad.rows() / F.rows();
    assert(data_heat.ng == 3 || data_heat.ng == 2);
    data_heat.Div = -0.25 * data_heat.Grad.transpose() * dblA.colwise().replicate(data_heat.ng).asDiagonal();

    Eigen::SparseMatrix<PT> Q = M - t * L;
    Eigen::MatrixXi O;
    igl::boundary_facets(F, O);
    igl::unique(O, data_heat.b);
    
    Eigen::SparseMatrix<PT> _;
    bool success1 = false, success2 = false, success3 = false;
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            success1 = igl::min_quad_with_fixed_precompute(Q, Eigen::VectorXi(), _, true, data_heat.Neumann);
        }
        
        #pragma omp section
        {
            success2 = true;
            if (data_heat.b.size() > 0)
            {
                success2 = igl::min_quad_with_fixed_precompute(Q, data_heat.b, _, true, data_heat.Dirichlet);
            }
        }

        #pragma omp section
        {
            const Eigen::Matrix<PT, 1, Eigen::Dynamic> M_diag_tr = M.diagonal().transpose();
            const Eigen::SparseMatrix<PT> Aeq = M_diag_tr.sparseView();
            L *= -0.5;
            success3 = igl::min_quad_with_fixed_precompute(L, Eigen::VectorXi(), Aeq, true, data_heat.Poisson);
        }
    }

    if (!success1 || !success2 || !success3)
    {
        throw std::runtime_error("Error in heat_geodesics_precompute");
    }
}

template <typename PT, std::size_t PD>
std::vector<PT> GeodesicHeatMetric<PT, PD>::computeDistances(const FaceId startFace) const
{
    Eigen::VectorXi gamma(1); 
    gamma << this->mesh->getFace(startFace).vertices[0];
    Eigen::VectorXd dist;
    igl::heat_geodesics_solve(data_heat, gamma, dist);

    std::vector<PT> distFaces(this->mesh->numFaces());
    #pragma omp parallel for
    for (FaceId faceId = 0; faceId < this->mesh->numFaces(); ++faceId)
    {
        const auto &face = this->mesh->getFace(faceId);

        for (int i = 0; i < 3; i++)
        {
            distFaces[faceId] += dist[face.vertices[i]];
        }
        distFaces[faceId] /= 3;
    }

    return distFaces;
}

// Explicit template instantiations
template class GeodesicHeatMetric<double, 3>;
