#include <map>

class MateDistanceStats {
public:
    typedef unsigned long distance_t;
    typedef unsigned long long count_t;
private:
    typedef std::map<distance_t, count_t> map_t;
    map_t map;
    static bool compare_count_descending(map_t::const_iterator a, map_t::const_iterator b) {
        return b->second < a->second;
    }
    static bool compare_distance(map_t::const_iterator a, map_t::const_iterator b) {
        return a->first < b->first;
    }
public:
    MateDistanceStats() {}
    void Count(distance_t const &d) { ++map[d]; }
    unsigned NthMostFrequent(unsigned N, distance_t result[N]) const;
};
