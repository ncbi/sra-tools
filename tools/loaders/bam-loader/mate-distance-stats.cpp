#include "mate-distance-stats.hpp"
#include <vector>
#include <algorithm>

unsigned MateDistanceStats::NthMostFrequent(unsigned N, distance_t result[N]) const
{
    typedef std::vector<map_t::const_iterator> vector_t;
    unsigned const n = map.size() < N ? map.size() : N;
    vector_t v;
    
    v.reserve(map.size());
    for (map_t::const_iterator i = map.cbegin(); i != map.cend(); ++i) {
        v.push_back(i);
    }
    std::stable_sort(v.begin(), v.end(), compare_count_descending);
    std::sort(v.begin(), v.begin() + n, compare_distance); // unique key therefore regular sort is stable
    
    vector_t::const_iterator j = v.cbegin();
    for (unsigned i = 0; i != n; ++i, ++j) {
        result[i] = (*j)->first;
    }
    return n;
}
