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

#pragma once

#if _WIN32
#error TODO
#elif __linux__
#define USE_PROCFS 1
#else
// assume it's some kind of BSD like system (ala macOS)
#include <sys/sysctl.h>
#define USE_SYSCTL 1
#endif

#include <algorithm>
#include <vector>
#include <map>
#include <cmath>
#include <cstdlib>
#include <atomic>
#include <iostream>
#include <fstream>

namespace utility {
    template <typename T>
    struct Array
    {
        using Element = T;
    private:
        bool isOwner;
        T *const pointer;
        size_t const count;

    public:
        explicit Array(T *const p = nullptr, size_t const n = 0, bool owned = true) noexcept
        : pointer(p)
        , count(n)
        , isOwner(owned)
        {}
        
        explicit Array(size_t const n, bool owned = true) noexcept
        : pointer((T *)malloc(n * sizeof(T)))
        , count(n)
        , isOwner(owned)
        {}
        
        ~Array() noexcept {
            if (pointer && isOwner)
                free(pointer);
        }
        
        bool contains(T const *const p) const { return p >= pointer && p < pointer + count; }
        bool contains(T *const p) const { return contains(const_cast<T const *>(p)); }
        T *base() const { return pointer; }
        size_t elements() const noexcept { return count; }
        operator bool() const noexcept { return pointer != nullptr; }
        bool operator !() const noexcept { return pointer == nullptr; }

        template <typename U>
        Array<U> cast() const noexcept { ///< NB. ownership is not transfered
            return Array<U>(reinterpret_cast<U *>(pointer), (count * sizeof(T))/sizeof(U), false);
        }
        template <typename U>
        Array<U> reshape() noexcept { ///< NB. ownership is transfered
            auto const wasOwner = isOwner;
            if (wasOwner) isOwner = false;
            return Array<U>(reinterpret_cast<U *>(pointer), (count * sizeof(T))/sizeof(U), wasOwner);
        }
        Array<T> resize(size_t new_count) noexcept { ///< NB. if successful, ownership is transfered to the new object and this object is invalidated
            void *const tmp = isOwner ? realloc(pointer, new_count * sizeof(T)) : nullptr;
            if (tmp != nullptr) isOwner = false;
            return Array<T>((T *)tmp, tmp != nullptr ? new_count : 0, true);
        }
        void zero() const {
            memset(pointer, 0, count * sizeof(T));
        }
        T *begin() const noexcept { return pointer; }
        T *end() const noexcept { return pointer + count; }
        T &front() noexcept { return *pointer; }
        T const &front() const noexcept { return *pointer; }
        T &operator [](size_t i) noexcept { return pointer[i]; }
        T const &operator [](size_t i) const noexcept { return pointer[i]; }
        
        template <typename Comp> void sort(Comp &&comp) { std::sort(pointer, pointer + count, comp); }

        template <typename U, typename F> U reduce(U const initial, F &&func) const
        {
            auto v = initial;
            for (auto && i : *this) {
                v = func(v, i);
            }
            return v;
        }
    };
    
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

    template <typename T, typename F>
    static T advanceWhile(T const &iter, T const &end, F &&comp) {
        T rslt = iter;
        while (rslt != end && comp(*rslt))
            ++rslt;
        return rslt;
    }

    template <typename T, typename F>
    static T advanceUntil(T const &iter, T const &end, F &&comp) {
        T rslt = iter;
        while (rslt != end && !comp(*rslt))
            ++rslt;
        return rslt;
    }

    static std::string stringWithContentsOfFile(std::string const &path)
    {
        std::string s;
        auto ifs = std::ifstream(path, std::ios::in);
        if (!ifs) { throw std::runtime_error("can't open file " + path); }
        
        ifs.seekg(0, std::ios::end);
        s.reserve(ifs.tellg());
        ifs.seekg(0, std::ios::beg);
        
        s.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
        return s;
    }
    
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
            if (arguments() > 0) {
                rslt.program.push_back(argument.front());
                if (arguments() > 1)
                    rslt.argument = decltype(rslt.argument)(argument.begin() + 1, argument.end());
            }
            return rslt;
        }
    };
    
    std::string trim_leading_whitespace(std::string const &str) {
        std::string::size_type first = 0, last = str.length();
        while (first < last && ::isspace(str[first]))
            ++first;
        return str.substr(first, last - first);
    }
    std::string trim_trailing_whitespace(std::string const &str) {
        std::string::size_type first = 0, last = str.length();
        while (first < last && ::isspace(str[last - 1]))
            --last;
        return str.substr(first, last - first);
    }
    std::string trim_whitespace(std::string const &str) {
        return trim_leading_whitespace(trim_trailing_whitespace(str));
    }
    
    struct sys_hardware_info {
        using name_value_pair_t = std::map<std::string, std::string>;

        enum { RAM, L1_CACHE, L2_CACHE, L3_CACHE, L_MAX = 8 };
        size_t cache_size[L_MAX];
        unsigned cache_sharing[L_MAX];
        unsigned cache_levels;
        unsigned available_cpus;
#if USE_PROCFS
        std::vector<name_value_pair_t> proc_cpuinfo;
#endif

        auto logical_cpu_count() const -> decltype(cache_sharing[0]) {
            return cache_sharing[0];
        }
        sys_hardware_info() {
            memset(this, 0, sizeof(*this));
#if USE_PROCFS
            proc_cpuinfo = load_proc_cpuinfo();
#endif
            available_cpus = get_avail_count();
            cache_levels = get_cache_sizes(cache_size);
            (void)get_cache_sharing(cache_sharing);
        }
#if 0
        static void test_parse_cpuinfo() {
            auto const content = std::string(
"processor     : 0\n"
"vendor_id     : GenuineIntel\n"
"cpu family    : 15\n"
"model         : 2\n"
"model name    : Intel(R) Xeon(TM) CPU 2.40GHz\n"
"stepping      : 7 cpu\n"
"MHz           : 2392.371\n"
"cache size    : 512 KB\n"
"physical id   : 0\n"
"siblings      : 2\n"
"runqueue      : 0\n"
"fdiv_bug      : no\n"
"hlt_bug       : no\n"
"f00f_bug      : no\n"
"coma_bug      : no\n"
"fpu           : yes\n"
"fpu_exception : yes\n"
"cpuid level   : 2\n"
"wp            : yes\n"
"flags         : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca  cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm\n"
"bogomips      : 4771.02\n");
            auto result = parse_cpuinfo(content.begin(), content.end());
            assert(result.size() == 1);
            assert(result[0]["processor"] == "0");
        }
#endif
    private:
        template <typename Iter, typename = std::enable_if<std::is_same<typename Iter::value_type, char>::value>>
        static std::vector<name_value_pair_t> parse_nvp(Iter beg, Iter const end) {
            using result_t = decltype(parse_nvp(beg, end));
            using char_t = decltype('\n');
            
            auto result = result_t();
            bool ws = true;
            bool iskey = true;
            bool eol = false;
            std::string::size_type lnw = 0;
            std::string key, value;
            int line = 0;
            
            while (beg != end) {
                auto const ch = char_t(*beg++);
                
                if (ch == '\n') {
                    ++line;
                    if (!eol) {
                        if (iskey) {
                            if (!key.empty()) break; ///< error
                            result.emplace_back(name_value_pair_t());
                        }
                        else {
                            if (result.empty())
                                result.emplace_back(name_value_pair_t());
                            value.erase(value.begin() + lnw, value.end());
                            result.back().insert(name_value_pair_t::value_type(key, value));
                        }
                    }
                    key.erase();
                    value.erase();
                    iskey = true;
                    ws = true;
                    eol = false;
                    lnw = 0;
                    continue;
                }
                else if (eol || (ws && ::isspace(ch)))
                    continue;
                
                ws = false;
                if (iskey && ch == ':') {
                    key.erase(key.begin() + lnw, key.end());
                    iskey = false;
                    ws = true;
                }
                else {
                    auto &accum = iskey ? key : value;
                    accum.append(1, ch);
                    if (!::isspace(ch))
                        lnw = accum.size();
                }
            }
            if (iskey && key.empty() && ws && !eol)
                return result;
            throw std::range_error("parse_nvp: invalid at line " + std::to_string(line));
        }
        static std::vector<name_value_pair_t> load_proc_cpuinfo() {
            auto cpuinfo = std::ifstream("/proc/cpuinfo");
            return parse_nvp(std::istream_iterator<char>(cpuinfo), std::istream_iterator<char>());
        }
        static bool sysctl(char const *const name, int &result) {
            size_t len = sizeof(int);
            return ::sysctlbyname(name, &result, &len, 0, 0) == 0 && len == sizeof(int);
        }
        static bool sysctl(char const *const name, size_t &result) {
            union u { uint64_t s; uint32_t i; } tmp;
            size_t len = sizeof(tmp);
            if (::sysctlbyname(name, &tmp, &len, 0, 0) == 0 && (len == sizeof(uint64_t) || len == sizeof(uint32_t))) {
                result = len == sizeof(uint32_t) ? size_t(tmp.i) : size_t(tmp.s);
                return true;
            }
            return false;
        }
        static unsigned get_cpu_count() {
#if USE_SYSCTL
            int value = 0;
            if (sysctl("hw.logicalcpu", value) && value > 0)
                return value;
            if (sysctl("hw.ncpu", value) && value > 0)
                return value;
#elif __linux__
#endif
            return 1; ///< there is always at least one cpu
        }
        static unsigned get_avail_count() {
#if USE_SYSCTL
            int value = 0;
            if (sysctl("hw.availcpu", value) && value > 0)
                return value;
#elif __linux__
#endif
            return get_cpu_count();
        }
        /** \brief Size of each cache
         */
        static unsigned get_cache_sizes(size_t sizes[]) {
#if USE_SYSCTL
            size_t len = sizeof(sizes[0] * L_MAX);
            if (sysctlbyname("hw.cachesize", sizes, &len, 0, 0) == 0) {
                return unsigned(len / sizeof(sizes[0]));
            }
            
            if (!sysctl("hw.memsize", sizes[RAM]) && !sysctl("hw.physmem", sizes[RAM]))
                return 0;
            if (!sysctl("hw.l1dcachesize", sizes[L1_CACHE]))
                return L1_CACHE;
            if (!sysctl("hw.l2cachesize", sizes[L2_CACHE]))
                return L2_CACHE;
            if (!sysctl("hw.l3cachesize", sizes[L3_CACHE]))
                return L3_CACHE;
            return L3_CACHE+1;
#elif __linux__
#endif
        }
        /** \brief Number of logical cpus per cache
         */
        static unsigned get_cache_sharing(unsigned sharing[]) {
#if USE_SYSCTL
            uint64_t value[L_MAX];
            size_t len = sizeof(value);
            if (sysctlbyname("hw.cacheconfig", value, &len, 0, 0) == 0) {
                auto const n = int(len / sizeof(value[0]));
                for (int i = 0; i < n; ++i) {
                    sharing[i] = unsigned(value[i]);
                }
                return n;
            }
            return 0;
#elif __linux__
#endif
        }
    };
    
    template <int DIVISIONS>
    struct ProgressTracker {
    private:
        double const frequency;
        int next = 0;
    public:
        template <typename T>
        ProgressTracker(T max) : frequency(max / double(DIVISIONS)) {}

        template <typename T, typename FUNC>
        void current(T const cur, FUNC &&f) {
            if (next * frequency <= cur) {
                auto const pct = int(ceil(next * (100.0 / DIVISIONS)));
                ++next;
                f(pct);
            }
        }
    };

    class strings_map {
    public:
        using index_t = unsigned;
    private:
        using char_store_t = std::vector<char>;
        using reverse_lookup_t = std::vector<index_t>;
        using ordered_list_elem_t = std::pair<index_t, index_t>;
        using ordered_list_t = std::vector<ordered_list_elem_t>;
        
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
        bool contains(std::string const &name, index_t &id) const { ///< deprecated
            auto const fnd = find(name);
            if (fnd.first) id = (ordered_list.begin() + fnd.second)->second;
            return fnd.first;
        }
        bool contains(std::string const &name, index_t *id) const {
            auto const fnd = find(name);
            if (fnd.first) *id = (ordered_list.begin() + fnd.second)->second;
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
        index_t operator[](std::string const &name) const {
            auto const fnd = find(name);
            if (fnd.first) return (ordered_list.begin() + fnd.second)->second;
            throw std::range_error("stringmap const");
        }
    };
    static inline unsigned uniform_random(unsigned const lower_bound, unsigned const upper_bound) {
        if (lower_bound > upper_bound) return uniform_random(upper_bound, lower_bound);
        if (lower_bound == upper_bound) return lower_bound;
        unsigned const n = upper_bound - lower_bound;
        unsigned r;
#if __APPLE__
        r = arc4random_uniform(n);
#else
        struct Seed {
            enum { smallest = 8, small = 32, normal = 64, big = 128, biggest = 256 };
            char state[normal];
            Seed() {
                initstate(unsigned(time(0)), state, sizeof(state));
            }
            unsigned random(unsigned const max) const {
                for ( ; ; ) {
                    auto const r = ::random();
                    if (r < max)
                        return unsigned(r);
                }
            }
            static unsigned max() { return unsigned(RAND_MAX); }
        };
        static auto seed = Seed();
        r = seed.random(unsigned(Seed::max() / n) * n) % n;
#endif
        return r + lower_bound;
    }
}
