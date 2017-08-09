#include <vector>
#include <boost/geometry.hpp>

#include "kdtree.h"

using namespace spatial_index;

typedef boost::geometry::model::point <double, 3, boost::geometry::cs::cartesian> Point;
std::vector<double> *natural_neighbor(std::vector<Point>& known_coordinates,
                                      std::vector<double>& known_values,
                                      std::vector<Point>& interpolation_points,
                                      int coord_max) {
    /*
     * Assumptions:
     *  - known_coordinates and known_values are in parallel
     *  - interpolation_points are regularly spaced and ordered
     *    such that X is the outermost loop, then Y, then Z.
     */

    printf("Building KD-Tree\n");

    kdtree<double> *tree = new kdtree<double>();
    for (int i = 0; i < known_coordinates.size(); i ++) {
        tree->add(&known_coordinates[i], &known_values[i]);
    }
    tree->build();

    std::vector<double> accumulator(interpolation_points.size());
    std::vector<double> contribution_counter(interpolation_points.size());

    for (int i = 0; i < interpolation_points.size(); i++) {
        accumulator[i] = 0;
        contribution_counter[i] = 0;
    }

    printf("Calculating scattered contributions\n");
    int xscale = coord_max*coord_max;
    int yscale = coord_max;

    // Scatter method discrete Sibson
    // For each interpolation point p, search neighboring interpolation points
    // within a sphere of radius r, where r = distance to nearest known point.
    for (int i = 0; i < interpolation_points.size(); i++) {
        const QueryResult *q = tree->nearest_iterative(interpolation_points[i]);
        double comparison_distance = q->distance; // Actually distance squared
        int r = floor(sqrt(comparison_distance));
        int px = interpolation_points[i].get<0>();
        int py = interpolation_points[i].get<1>();
        int pz = interpolation_points[i].get<2>();
        if (i%10000 == 0){
            printf("\tPoint %d of %d: (%d, %d, %d). Radius %d and nearest value %f\n", i, (int) interpolation_points.size(), px, py, pz, r, q->value);
        }
        // Search neighboring interpolation points within a bounding box
        // of r indices. From this subset of points, calculate their distance
        // and tally the ones that fall within the sphere of radius r surrounding
        // interpolation_points[i].
        for (int x = px - r; x < px + r; x++) {
            if (x < 0 || x >= coord_max) { continue; }
            for (int y = py - r; y < py + r; y++) {
                if (y < 0 || y >= coord_max) { continue; }
                for (int z = pz - r; z < pz + r; z++) {
                    if (z < 0 || z >= coord_max) { continue; }
                    int idx = x*xscale + y*yscale + z;
                    double distance_x = interpolation_points[idx].get<0>() - px;
                    double distance_y = interpolation_points[idx].get<1>() - py;
                    double distance_z = interpolation_points[idx].get<2>() - pz;
                    if (distance_x*distance_x + distance_y*distance_y
                            + distance_z*distance_z > comparison_distance){
                        continue;
                    }
                    accumulator[idx] += q->value;
                    contribution_counter[idx] += 1;
                }
            }
        }
    }

    std::vector<double> *interpolation_values = new std::vector<double>(interpolation_points.size());
    for (int i = 0; i < interpolation_values->size(); i++) {
        if (contribution_counter[i] != 0) {
            (*interpolation_values)[i] = accumulator[i] / contribution_counter[i];
        } else {
            (*interpolation_values)[i] = 0; //TODO: this is just 0, better way to mark NAN?
        }
    }
    delete tree;
    return interpolation_values;
}
