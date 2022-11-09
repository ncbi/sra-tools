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
 *
 */

#pragma once

#if __cplusplus >= 202002L || ((__cplusplus >= 201703L || defined(__has_include)) && __has_include("version"))
# include <version>
#endif

#include "util.win32.hpp"
#include "util.posix.hpp"

#include "opt_string.hpp"

#include <stdexcept>
#include <system_error>
#include <iterator>
#include <algorithm>
#include <type_traits>
#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <cstdlib>
#include <climits>
#include <vector>

using Dictionary = std::map<std::string, std::string>;

#if __cpp_lib_string_udls && !MS_Visual_C
#define STD_STRING_LITERAL(STR) STR ## s
#else
#define STD_STRING_LITERAL(STR) std::string(STR)
#endif

#if __cpp_lib_starts_ends_with

static inline bool starts_with(std::string const &prefix, std::string const &in_string)
{
    return in_string.starts_with(prefix);
}

static inline bool ends_with(std::string const &suffix, std::string const &in_string)
{
    return in_string.ends_with(suffix);
}

#else

static inline bool starts_with(std::string const &prefix, std::string const &in_string)
{
    return (in_string.size() < prefix.size()) ? false : (in_string.compare(0, prefix.size(), prefix) == 0);
}

static inline bool ends_with(std::string const &suffix, std::string const &in_string)
{
    return (in_string.size() < suffix.size()) ? false : in_string.compare(in_string.size() - suffix.size(), suffix.size(), suffix) == 0;
}
#endif

static inline std::string without_suffix(std::string const &suffix, std::string const &string)
{
    if (ends_with(suffix, string))
        return string.substr(0, string.size() - suffix.size());
    else
        return string;
}

#if __cpp_lib_string_view
# include <string_view>
using StringView = std::string_view;
#else
#include <cstdint>
struct StringView {
    using size_type = std::string::size_type;
private:
    char const *begin_;
    char const *end_;

    size_type copy(char *dst) const {
        auto cur = dst;
        for (auto ch : *this)
            *cur++ = ch;
        return (size_type)(dst - cur);
    }
    int comp(char const *rhs) const {
        for (auto ch : *this) {
            auto const diff = int(ch) - int(*rhs++);
            if (diff != 0)
                return diff;
        }
        return 0;
    }
public:
    static const size_type npos = ~(size_type(0));

    StringView()
    : begin_(nullptr)
    , end_(nullptr)
    {}
    StringView(std::string const &str)
    : begin_(str.data())
    , end_(begin_ + str.size())
    {}
    StringView(char const *start, char const *endp)
    : begin_(start)
    , end_(endp)
    {
        assert(start != nullptr && endp != nullptr && start <= endp);
    }
    explicit StringView(char const *start)
    : begin_(start)
    , end_(start)
    {
        assert(start != nullptr);
        while (*end_ != '\0')
            ++end_;
    }
    StringView(char const *start, size_type count)
    : begin_(start)
    , end_(start)
    {
        assert(start != nullptr);
        end_ += count;
    }
    bool empty() const { return begin_ == end_; }
    size_type size() const { return (size_type)(end_ - begin_); }
    size_type length() const { return size(); }
    constexpr size_type max_size() const { return SIZE_MAX; }
    char const *begin() const { return begin_; }
    char const *end() const { return end_; }
    char const *cbegin() const { return begin_; }
    char const *cend() const { return end_; }

    char const &operator [](size_type i) const {
        auto const ptr = begin_ + i;
        assert(ptr < end_);
        return *ptr;
    }
    char const &at(size_type i) const {
        auto const ptr = begin_ + i;
        if (ptr < end_)
            return *ptr;
        throw std::out_of_range("StringView::at");
    }
    char const &front() const {
        return (*this)[0];
    }
    char const &back() const {
        return (*this)[size() - 1];
    }
    void remove_prefix(size_type n) {
        begin_ += n;
        assert(begin_ <= end_);
    }
    void remove_suffix(size_type n) {
        end_ -= n;
        assert(begin_ <= end_);
    }
    void swap(StringView &other) {
        std::swap(begin_, other.begin_);
        std::swap(end_, other.end_);
    }
    StringView substr() const {
        return *this;
    }
    StringView substr(size_type pos) const {
        if (pos == 0) return substr();
        if (pos < size()) {
            auto const ptr = begin_ + pos;
            return StringView(ptr, end_);
        }
        throw std::out_of_range("StringView::substr: pos beyond end");
    }
    StringView substr(size_type pos, size_type count) const {
        if (count == npos)
            return substr(pos);

        if (pos < size()) {
            auto const ptr = begin_ + pos;
            return StringView(ptr, std::min(ptr + count, end_));
        }
        throw std::out_of_range("StringView::substr: pos beyond end");
    }
    size_type copy(char *dst, size_type count, size_type pos = 0) {
        if (pos < size())
            return substr(pos, count).copy(dst);

        throw std::out_of_range("StringView::copy: pos beyond end");
    }
    int compare(StringView const &rhs) const {
        auto const lsize = size();
        auto const rsize = rhs.size();

        if (rsize < lsize)
            return -rhs.compare(*this);

        auto ri = rhs.begin();
        for (auto ch : *this) {
            auto const diff = int(ch) - int(*ri++);
            if (diff != 0) return diff < 0 ? -1 : 1;
        }
        return lsize < rsize ? -1 : 0;
    }
    int compare(char const *rhs) const {
        for (auto ch : *this) {
            auto const theirs = *rhs++;
            if (theirs == '\0')
                return 1;
            auto const diff = int(ch) - int(theirs);
            if (diff != 0) return diff < 0 ? -1 : 1;
        }
        return *rhs == '\0' ? 0 : -1;
    }
    bool operator ==(StringView const &rhs) const {
        return compare(rhs) == 0;
    }
    bool operator ==(char const *rhs) const {
        return compare(rhs) == 0;
    }
    bool starts_with(char const ch) const {
        if (size() < 1) return false;
        return *begin_ == ch;
    }
    bool starts_with(StringView const &rhs) const {
        if (size() < rhs.size()) return false;
        return substr(rhs.size()) == rhs;
    }
    bool starts_with(char const *rhs) const {
        for (auto ch : *this) {
            auto const theirs = *rhs++;
            if (theirs == '\0')
                return true;
            if (ch != theirs)
                return false;
        }
        return *rhs == '\0';
    }
    bool ends_with(StringView const &other) const {
        if (size() < other.size()) return false;
        return substr(size() - other.size()) == other;
    }
    bool ends_with(char const *other) const {
        return ends_with(StringView(other));
    }

    operator std::string() const {
        return std::string(begin(), end());
    }
};
static inline std::ostream &operator <<(std::ostream &os, StringView const &sv) {
    return os << std::string(sv);
}
#endif

static inline std::error_code error_code_from_errno();

static inline void throw_system_error [[noreturn]] (char const *const what)
{
    throw std::system_error(error_code_from_errno(), what);
}

static inline void throw_system_error [[noreturn]] (std::string const &what)
{
    throw std::system_error(error_code_from_errno(), what);
}

/// @brief Array 'optimized' for small sizes
template <typename T, int N>
class SmallArray {
public:
    using Value = typename std::remove_reference<T>::type;
    using size_type = typename std::vector<Value>::size_type;
    using difference_type = typename std::vector<Value>::difference_type;
private:
    T fixed[N];
    std::vector<T> overflow;
    size_type size = 0;
public:
    SmallArray() = default;
    SmallArray(std::initializer_list<T> const &list)
    {
        for (auto &value : list)
            push_back(value);
    }
    bool empty() const { return size == 0; }
    bool validIndex(size_type i) const {
        return i >= 0 && (i < N || (i - N) < overflow.size());
    }
    T const &at(size_type i) const {
        if (i < N) {
            if (i < size)
                return fixed[i];
            throw std::out_of_range("SmallArray::at");
        }
        return overflow.at(i - N);
    }
    T &at(size_type i) {
        if (i < N) {
            if (i < size)
                return fixed[i];
            throw std::out_of_range("SmallArray::at");
        }
        return overflow.at(i - N);
    }
    T const &operator[](size_type i) const {
        return at(i);
    }
    T &operator[](size_type i) {
        return at(i);
    }
    T const &back() const { return at(size - 1); }
    T &back() { return at(size - 1); }
    void push_back(T const &value) {
        if (size < N)
            fixed[size++] = value;
        else
            overflow.push_back(value);
    }
    void pop_back() {
        assert(!empty());
        if (size <= N)
            (fixed + (--size))->~T();
        else
            overflow.pop_back();
    }
    struct Iterator {
    private:
        SmallArray *const parent;
        size_type current;

        friend SmallArray;

        Iterator(SmallArray *const par, size_type const cur = 0)
        : parent(par)
        , current(cur)
        {}
    public:
        Iterator(Iterator const &other)
        : parent(other.parent)
        , current(other.current)
        {}
        Iterator &operator= (Iterator const &other)
        {
            parent = other.parent;
            current = other.current;
        }
        Value &operator* () {
            return parent->at(current);
        }
        Value const &operator* () const {
            return parent->at(current);
        }
        Value &operator-> () {
            return parent->at(current);
        }
        Value const &operator-> () const {
            return parent->at(current);
        }
        Iterator &operator++ () {
            if (parent->validIndex(current))
                ++current;
            return *this;
        }
        Iterator operator++ (int) {
            Iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        Iterator &operator-= (difference_type i) {
            if (i < 0)
                return this += -i;

            if (i <= current && parent->validIndex(current - i))
                current -= i;
            else
                *this = parent->end();

            return *this;
        }
        Iterator &operator+= (difference_type i) {
            if (i < 0)
                return this -= -i;

            if (parent->validIndex(current + i))
                current += i;
            else
                *this = parent->end();

            return *this;
        }
        Iterator operator- (difference_type i) const {
            Iterator result(*this);
            result -= i;
            return result;
        }
        Iterator operator+ (difference_type i) const {
            Iterator result(*this);
            result += i;
            return result;
        }
        difference_type operator- (Iterator const &other) const {
            if (other.parent == parent)
                return ((difference_type)current) - ((difference_type)other.current);
            throw std::logic_error("SmallArray::operator-");
        }
        bool operator== (Iterator const &other) const {
            return other.parent == parent && other.current == current;
        }
    };
    struct ConstIterator {
    private:
        Iterator base_;
        friend SmallArray;

        ConstIterator(Iterator const &other)
        : base_(other)
        {}
    public:
        ConstIterator(ConstIterator const &other)
        : base_(other.base_)
        {}
        ConstIterator &operator= (ConstIterator const &other)
        {
            base_ = other.base_;
        }
        Value const &operator* () const {
            return *base_;
        }
        Value const &operator-> () const {
            return *base_;
        }
        ConstIterator &operator++ () {
            ++base_;
            return *this;
        }
        ConstIterator operator++ (int) {
            Iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        ConstIterator &operator-= (difference_type i) {
            base_ -= i;
            return *this;
        }
        ConstIterator &operator+= (difference_type i) {
            base_ += i;
            return *this;
        }
        ConstIterator operator- (difference_type i) const {
            ConstIterator result(*this);
            result -= i;
            return result;
        }
        ConstIterator operator+ (difference_type i) const {
            ConstIterator result(*this);
            result += i;
            return result;
        }
        difference_type operator- (ConstIterator const &other) const {
            return base_ - other.base_;
        }
        difference_type operator- (Iterator const &other) const {
            return base_ - other;
        }
        bool operator== (Iterator const &other) const {
            return base_ == other;
        }
        bool operator== (ConstIterator const &other) const {
            return base_ == other.base_;
        }
    };
    using iterator = Iterator;
    using const_iterator = ConstIterator;

    iterator begin() { return Iterator(this); }
    const_iterator cbegin() const { return ConstIterator(begin()); }
    const_iterator begin() const { return cbegin(); }

    iterator end() { return Iterator(this, N + overflow.size()); }
    const_iterator cend() const { return ConstIterator(end()); }
    const_iterator end() const { return cend(); }
};

/// @brief helper class for doing range-for on iterate-able things that don't have begin/end, like c arrays
///
/// @tparam ITER the type of iterator
template <typename ITER>
struct Sequence {
private:
    ITER beg_;
    ITER end_;

public:
    Sequence(ITER const &beg, ITER const &end) : beg_(beg), end_(end) {}

    ITER begin() const { return beg_; }
    ITER end() const { return end_; }
};

/// @brief helper function to allow type inference of iterator type
template <typename ITER>
static inline Sequence<ITER> make_sequence(ITER const &beg, ITER const &end) {
    return Sequence<ITER>(beg, end);
}

/// @brief helper function to allow type inference of iterator type
template <typename ITER>
static inline Sequence<ITER> make_sequence(ITER const &beg, size_t const count) {
    return Sequence<ITER>(beg, beg + count);
}

template < typename ITER >
size_t iterDistance(ITER const &f, ITER const &e)
{
    auto const result = std::distance(f, e);
    assert(result >= 0);
    return result;
}

template < typename ITER >
ITER middle(ITER const &f, ITER const &e)
{
    auto m = f;
    std::advance(m, iterDistance(f, e) >> 1);
    return m;
}

template < typename ITER, typename VALUE, typename COMP >
ITER lowerBound(ITER const &beg, ITER const &end, VALUE const &value, COMP comp)
{
    auto f = beg;
    auto e = end;

    while (f < e) {
        auto const m = middle(f, e);
        if (comp(*m, value))
            f = std::next(m);
        else
            e = m;
    }
    return f;
}

template < typename ITER, typename VALUE >
ITER lowerBound(ITER const &beg, ITER const &end, VALUE const &value)
{
    auto f = beg;
    auto e = end;

    while (f < e) {
        auto const m = middle(f, e);
        if (*m < value)
            f = std::next(m);
        else
            e = m;
    }
    return f;
}

template < typename ITER, typename VALUE, typename COMP >
ITER upperBound(ITER const &beg, ITER const &end, VALUE const &value, COMP comp)
{
    auto f = beg;
    auto e = end;

    while (f < e) {
        auto const m = middle(f, e);
        if (!comp(value, *m))
            f = std::next(m);
        else
            e = m;
    }
    return f;
}

template < typename ITER, typename VALUE >
ITER upperBound(ITER const &beg, ITER const &end, VALUE const &value)
{
    auto f = beg;
    auto e = end;

    while (f < e) {
        auto const m = middle(f, e);
        if (!(value < *m))
            f = std::next(m);
        else
            e = m;
    }
    return f;
}

template < typename ITER, typename VALUE, typename LESS, typename SSEL >
std::pair<ITER, ITER> equalRange(ITER const &beg, ITER const &end, VALUE const &value, LESS less, SSEL ssel)
{
    auto f = beg;
    auto e = end;

    while (f < e) {
        auto const m = middle(f, e);
        if (less(*m, value))
            f = std::next(m);
        else if (ssel(value, *m))
            e = m;
        else
            return std::make_pair(lowerBound(f, m, value, less), upperBound(std::next(m), e, value, ssel));
    }
    return std::make_pair(f, f);
}

template < typename ITER, typename VALUE >
std::pair<ITER, ITER> equalRange(ITER const &beg, ITER const &end, VALUE const &value)
{
    auto f = beg;
    auto e = end;

    while (f < e) {
        auto const m = middle(f, e);
        if (*m < value)
            f = std::next(m);
        else if (value < *m)
            e = m;
        else
            return std::make_pair(lowerBound(f, m, value), upperBound(std::next(m), e, value));
    }
    return std::make_pair(f, f);
}

template < typename CONTAINER, typename VALUE, typename COMP, typename ITER = typename CONTAINER::const_iterator >
ITER lowerBound(CONTAINER const &container, VALUE const &value, COMP comp)
{
    return lowerBound(container.begin(), container.end(), value, comp);
}

template < typename CONTAINER, typename VALUE, typename COMP, typename ITER = typename CONTAINER::const_iterator >
ITER upperBound(CONTAINER const &container, VALUE const &value, COMP comp)
{
    return upperBound(container.begin(), container.end(), value, comp);
}

template < typename CONTAINER, typename VALUE, typename LESS, typename SSEL, typename ITER = typename CONTAINER::const_iterator >
std::pair<ITER, ITER> equalRange(CONTAINER const &container, VALUE const &value, LESS less, SSEL ssel)
{
    return equalRange(container.begin(), container.end(), value, less, ssel);
}

template < typename CONTAINER, typename VALUE, typename ITER = typename CONTAINER::const_iterator >
ITER lowerBound(CONTAINER const &container, VALUE const &value)
{
    return lowerBound(container.begin(), container.end(), value);
}

template < typename CONTAINER, typename VALUE, typename ITER = typename CONTAINER::const_iterator >
ITER upperBound(CONTAINER const &container, VALUE const &value)
{
    return upperBound(container.begin(), container.end(), value);
}

template < typename CONTAINER, typename VALUE, typename ITER = typename CONTAINER::const_iterator >
std::pair<ITER, ITER> equalRange(CONTAINER const &container, VALUE const &value)
{
    return equalRange(container.begin(), container.end(), value);
}

template < typename T, bool UNIQUE = false >
class OrderedList {
public:
    using Value = typename std::remove_reference<T>::type;
    using Container = std::vector<Value>;
    using Index = unsigned int;
    using Iterator = typename Container::iterator;
    using ConstIterator = typename Container::const_iterator;
private:
    Container container;

public:
    OrderedList(OrderedList &&other)
    : container(std::move(other.container))
    {}
    OrderedList &operator =(OrderedList &&other) {
        container.swap(other.container);
        return *this;
    }
    OrderedList(OrderedList const &other)
    : container(other.container)
    {}
    OrderedList &operator =(OrderedList const &other) {
        container = other.container;
        return *this;
    }

    decltype(container.size()) size() const { return container.size(); }
    Iterator insert(T const &value)
    {
        if (container.empty()) {
    AT_BACK:
            container.emplace_back(value);
            auto result = (container.rbegin() + 1).base();
            assert(&*result == &container.back());
            return result;
        }

        auto const before = std::upper_bound(container.begin(), container.end(), value);
        if (before == container.end()) {
            if (UNIQUE && container.back() == value) {
    NOT_UNIQUE:
                return container.end();
            }
            goto AT_BACK;
        }
        auto const at = std::distance(container.begin(), before);
        if (UNIQUE && at > 0 && container[at - 1] == value)
            goto NOT_UNIQUE;

        container.insert(before, value);
        {
            auto result = container.begin();
            std::advance(result, at);
            return result;
        }
    }
    template <typename U>
    std::pair<ConstIterator, ConstIterator> find(U const &value) const
    {
        return equalRange(container, value);
    }
    template <typename U>
    bool contains(U const &value) const {
        auto const fnd = find(value);
        return fnd.first != fnd.second;
    }
    OrderedList() = default;
    explicit OrderedList(size_t capacity)
    {
        container.reserve(capacity);
    }
    template <typename U>
    explicit OrderedList(std::initializer_list<U> const &u)
    {
        container.reserve(u.size());
        for (auto && i : u) {
            auto const &t = Value(u);
            if (container.empty() || container.back() < t || (!UNIQUE && container.back() == t))
                container.emplace_back(t);
            else
                insert(t);
        }
    }
    template <typename ITER>
    explicit OrderedList(ITER const &beg, ITER const &end)
    {
        container.reserve(iterDistance(beg, end));
        for (auto i = beg; i != end; ++i) {
            auto const &t = Value(*i);
            if (container.empty() || container.back() < t || (!UNIQUE && container.back() == t))
                container.emplace_back(t);
            else
                insert(t);
        }
    }
    Value const &operator[] (Index i) const { return container[i]; }
    Iterator begin() { return container.begin(); }
    ConstIterator cbegin() const { return container.cbegin(); }
    ConstIterator begin() const { return cbegin(); }
    Iterator end() { return container.end(); }
    ConstIterator cend() const { return container.cend(); }
    ConstIterator end() const { return cend(); }
};

template <typename T>
using UniqueOrderedList = OrderedList<T, true>;

template <typename IMPL>
class EnvironmentVariables_ : protected IMPL {
public:
    using Value = opt_string;
    using Set = std::map<std::string, Value>;

    static Value get(std::string const &name) {
        return IMPL::get(name.c_str());
    }
    /// @brief Set an environment variable.
    static void set(std::string const &name, std::string const &value) {
        IMPL::set(name.c_str(), value.c_str());
    }
    /// @brief Set or unset an environment variable.
    static void set(std::string const &name, Value const &value) {
        IMPL::set(name.c_str(), value ? value.value().c_str() : nullptr);
    }
    struct AutoRestore {
        ~AutoRestore() {
            for (auto const &v : save) {
                set(v.first, v.second);
            }
        }
        explicit AutoRestore(Dictionary const &vars)
        {
            for (auto const &v : vars) {
                save[v.first] = get(v.first);
            }
        }
    private:
        Set save;
    };
};

using EnvironmentVariables = EnvironmentVariables_<PlatformEnvironmentVariables>;
