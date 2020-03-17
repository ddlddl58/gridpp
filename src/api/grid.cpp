#include "gridpp.h"

gridpp::Grid::Grid(vec2 lats, vec2 lons, vec2 elevs, vec2 lafs) {
    mLats = lats;
    mLons = lons;
    mElevs = elevs;
    mLafs = lafs;
    mX = lats[0].size();
    int N = lats.size() * lats[0].size();
    vec lats0(N);
    vec lons0(N);
    for(int i = 0; i < lats.size(); i++) {
        for(int j = 0; j < lats[0].size(); j++) {
            lats0.push_back(lats[i][j]);
            lons0.push_back(lons[i][j]);
        }
    }
    KDTree test = KDTree(lats0, lons0);
    mTree = test;
}

int gridpp::Grid::get_num_neighbours(float lat, float lon, float radius) {
    ivec indices = mTree.get_neighbours(lat, lon, radius);
    return indices.size();
}

ivec2 gridpp::Grid::get_neighbours_with_distance(float lat, float lon, float radius, vec& distances) {
    ivec indices = mTree.get_neighbours_with_distance(lat, lon, radius, distances);
    return get_indices(indices);
}

ivec2 gridpp::Grid::get_neighbours(float lat, float lon, float radius) {
    ivec indices = mTree.get_neighbours(lat, lon, radius);
    return get_indices(indices);
}

ivec2 gridpp::Grid::get_closest_neighbours(float lat, float lon, int num) {
    ivec indices = mTree.get_closest_neighbours(lat, lon, num);
    return get_indices(indices);
}
ivec gridpp::Grid::get_nearest_neighbour(float lat, float lon) {
    return get_closest_neighbours(lat, lon, 1)[0];
}
vec2 gridpp::Grid::get_lats() const {
    return mLats;
}
vec2 gridpp::Grid::get_lons() const {
    return mLons;
}
vec2 gridpp::Grid::get_elevs() const {
    return mElevs;
}
vec2 gridpp::Grid::get_lafs() const {
    return mLafs;
}
vec2 gridpp::Grid::get_2d(vec input) const {
    int Y = input.size() / mX;
    vec2 output(Y);
    int count = 0;
    for(int i = 0; i < Y; i++) {
        output[i].resize(mX, 0);
        for(int j = 0; j < mX; j++) {
            output[i][j] = input[count];
            count++;
        }
    }
    return output;
}
ivec gridpp::Grid::get_indices(int index) const {
    ivec results(2, 0);
    results[0] = index / mX;
    results[1] = index % mX;
    return results;
}
ivec2 gridpp::Grid::get_indices(ivec indices) const {
    ivec2 results(indices.size());
    for(int i = 0; i < indices.size(); i++) {
        results[i] = get_indices(indices[i]);
    }
    return results;
}
