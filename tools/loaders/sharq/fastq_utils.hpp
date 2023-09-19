#ifndef __CFASTQ_UTILS_HPP__
#define __CFASTQ_UTILS_HPP__

/**
 * @file fastq_utils.hpp
 * @brief Utilities
 * 
 */

#include <sstream>
#include <string>

using namespace std;

namespace sharq {

template<typename T>
string join(const T& b, const T& e, const string& delimiter = ", ")
{
    stringstream ss;
    auto it = b;
    ss << *it++;
    while (it != e) {
        ss << delimiter << *it++;
    }
    return ss.str();
}

template<typename T_IN, typename T_OUT>
void split(const T_IN& str, T_OUT& out, const char c = ',')
{
    out.clear();
    if (str.empty())
        return;
    auto begin = str.begin();
    auto end  = str.end();
    while (begin != end) {
        auto it = begin;
        while (it != end && *it != c) ++it;
        out.emplace_back(&(*begin), distance(begin, it));
        if (it == end)
            break;
        begin = ++it;
    }
}


}  // sharq namespace

#endif

