/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 */

#ifndef __UTILITY_HPP_INCLUDED__
#define __UTILITY_HPP_INCLUDED__ 1

#include <vector>
namespace utility {
    
    struct StatisticsAccumulator {
    private:
        double N;
        double sum;
        double M2;
        double min;
        double max;
    public:
        StatisticsAccumulator() : N(0) {}
        explicit StatisticsAccumulator(double const value) : sum(value), min(value), max(value), M2(0.0), N(1) {}
        
        auto count() const -> decltype(N) { return N; }
        double average() const { return sum / N; }
        double variance() const { return M2 / N; }
        double minimum() const { return min; }
        double maximum() const { return max; }

        void add(double const value) {
            if (N == 0) {
                min = max = sum = value;
                M2 = 0.0;
            }
            else {
                auto const diff = average() - value;
                
                M2 += diff * diff * N / (N + 1);
                sum += value;
                min = std::min(min, value);
                max = std::max(max, value);
            }
            N += 1;
        }
        friend StatisticsAccumulator operator +(StatisticsAccumulator a, StatisticsAccumulator b) {
            StatisticsAccumulator result;

            result.N = a.N + b.N;
            result.sum = (a.N == 0.0 ? 0.0 : a.sum) + (b.N == 0.0 ? 0.0 : b.sum);
            result.min = std::min(a.N == 0.0 ? 0.0 : a.min, b.N == 0.0 ? 0.0 : b.min);
            result.max = std::max(a.N == 0.0 ? 0.0 : a.max, b.N == 0.0 ? 0.0 : b.max);
            result.M2 = (a.N == 0.0 ? 0.0 : a.M2) + (b.N == 0.0 ? 0.0 : b.M2);
            if (a.N != 0 && b.N != 0) {
                auto const diff = a.average() - b.average();
                auto const adjust = diff * diff * a.N * b.N / result.N;
                result.M2 += adjust;
            }
            return result;
        }
        StatisticsAccumulator operator +=(StatisticsAccumulator const &other) {
            return *this + other;
        }
    };

    static char const *programNameFromArgv0(char const *const argv0)
    {
        auto last = -1;
        for (auto i = 0; ; ++i) {
            auto const ch = argv0[i];
            if (ch == '\0') return argv0 + last + 1;
            if (ch == '/') last = i;
        }
    }
    
    struct CommandLine {
        std::vector<std::string> program, argument;
        
        CommandLine() {}
        CommandLine(int const argc, char const *const *const argv)
        : argument(argv + 1, argv + argc)
        , program(1, programNameFromArgv0(argv[0]))
        {}
        auto arguments() const -> decltype(argument.size()) { return argument.size(); }
        CommandLine dropFirst() const {
            auto rslt = CommandLine();
            rslt.program = program;
            rslt.argument = argument;
            if (rslt.argument.size() > 0) {
                rslt.program.push_back(rslt.argument.front());
                rslt.argument.erase(rslt.argument.begin());
            }
            return rslt;
        }
    };

    class strings_map {
        typedef std::vector<char> char_store_t;
        typedef unsigned index_t;
        typedef std::vector<index_t> reverse_lookup_t;
        typedef std::pair<index_t, index_t> ordered_list_elem_t;
        typedef std::vector<ordered_list_elem_t> ordered_list_t;
        
        char_store_t char_store;
        reverse_lookup_t reverse_lookup;
        ordered_list_t ordered_list;
        
        std::pair<bool, int> find(std::string const &name) const {
            auto f = ordered_list.begin();
            auto e = ordered_list.end();
            auto const base = char_store.data();
            
            while (f + 0 < e) {
                auto const m = f + ((e - f) >> 1);
                auto const cmp = name.compare(base + m->first);
                if (cmp == 0) return std::make_pair(true, m - ordered_list.begin());
                if (cmp < 0)
                    e = m;
                else
                    f = m + 1;
            }
            return std::make_pair(false, f - ordered_list.begin());
        }
    public:
        strings_map() {}
        strings_map(std::initializer_list<std::string> list) {
            for (auto && i : list)
                (void)operator[](i);
        }
        strings_map(std::initializer_list<char const *> list) {
            for (auto && i : list)
                (void)operator[](std::string(i));
        }
        index_t count() const {
            return (index_t)reverse_lookup.size();
        }
        bool contains(std::string const &name, index_t &id) const {
            auto const fnd = find(name);
            if (fnd.first) id = (ordered_list.begin() + fnd.second)->second;
            return fnd.first;
        }
        index_t operator[](std::string const &name) {
            auto const fnd = find(name);
            if (fnd.first) return (ordered_list.begin() + fnd.second)->second;
            
            auto const newPair = std::make_pair((index_t)char_store.size(), (index_t)ordered_list.size());
            
            char_store.insert(char_store.end(), name.begin(), name.end());
            char_store.push_back(0);
            
            ordered_list.insert((ordered_list.begin() + fnd.second), newPair);
            reverse_lookup.push_back(newPair.first);
            
            return (index_t)newPair.second;
        }
        std::string operator[](index_t id) const {
            if (id < reverse_lookup.size()) {
                char const *const rslt = char_store.data() + reverse_lookup[id];
                return std::string(rslt);
            }
            throw std::out_of_range("invalid id");
        }
    };
}

#endif //__VDB_HPP_INCLUDED__
