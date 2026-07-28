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
* Project:
*  sratools command line tool
*
* Purpose:
*  Parse SDL Response JSON
*
*/

#pragma once

#include <cstdint>
#include "util.hpp"
#include "debug.hpp"
#include "opt_string.hpp"

#if MS_Visual_C
#pragma warning(disable: 4061)
#endif

/// @brief Event based parser. How-to-use: tl;dr Create delegate classes to fill out
/// your C++ classes with values from the JSON string. Create chains of delegates to
/// fill out nested structures structures.
///
/// @discussion The general procedure is this: Suppose the top level
/// construct in the JSON is an object (this is the usual case).
/// @par
/// 1. Create a delegate for the top level object.
/// It's job is to verify the type of the construct is an object and to return a delegate for your object.
/// There is a boilerplace class `TopLevelObjectDelegate` that is probably sufficient.
/// @par
/// 2. Create a delegate class to capture the values for your object.
/// It's job is to respond to the `evt_member_name` events and return the
/// appropriate delegate for each member. You will need to write this. This is typical:
/// @code
/// class MyObject {
///     std::string status, message;
///
///     MyObject(std::string const &status, std::string const &message)
///     : status(status)
///     , message(message)
///     {}
///     friend class MyObjectDelegate;
/// public:
///     operator bool() const { return status == "200"; }
///     void printMessage(std::ostream &os) const {
///         os << message << std::endl;
///     }
///
///     static MyObject decode(std::string const &json);
/// };
///
/// class MyObjectDelegate final : public JSONParser::Delegate {
///     JSONValueDelegate<std::string, std::string> status; ///< don't use JSONString, status is a number (so not quoted).
///     JSONValueDelegate<std::string> message; ///< use JSONString, message is a JSON string.
///
///     virtual JSONParser::Delegate *recieve(JSONParser::EventType type, StringView const &value)
///     {
///         switch (type) {
///         case JSONParser::evt_end:
///             return nullptr;  ///< switch the parser back to the previous delegate
///         case JSONParser::evt_member_name:
///             break;
///         default:
///             return this;  ///< ignore unrecognized events and continue.
///         }
///         std::string member = JSONString(value, true); // member name as a checked and decoded UTF8 std::string
///
///         if (name == "status")
///             return &status;
///         if (name == "message")
///             return &message;
///
///         return this; ///< ignore unrecognized members and continue.
///     }
///
///     MyObjectDelegate()
///     {}
///
///     MyObject get() const {
///         assert(status && message);
///         return MyObject(status, message);
///     }
/// };
///
/// MyObject MyObject::decode(std::string const &json) {
///     auto myDelegate = MyObjectDelegate();
///     auto tlo = TopLevelObjectDelegate(&myDelegate);
///     auto parser = JSONParser(json.c_str(), &tlo);
///
///     try {
///         parser.parse();
///         return myDelegate.get();
///     }
///     catch (JSONParser::Error const &) {
///         std::cerr << "Unparsable JSON:\n" << json << std::endl;
///     }
///     catch (JSONScalarConversionError const &) {
///         std::cerr << "Unusable JSON:\n" << json << std::endl;
///     }
///     throw std::domain_error("MyObject could not be constructed from the JSON string!");
/// }
/// @endcode
struct JSONParser {

    /// @brief `Error` is thrown if the JSON string is unparsable.
    struct Error {};
    struct EndOfInput : Error {};
    struct ExpectationFailure : Error {};
    struct ExpectedMemberName : ExpectationFailure {};
    struct ExpectedMemberNameValueSeparator : ExpectationFailure {};
    struct ExpectedSomeValue : ExpectationFailure {};
    struct Unexpected : ExpectationFailure {
        char unexpected;
        explicit Unexpected(char ch) : unexpected(ch) {}
    };

    /// @brief The event types.
    enum EventType {
        evt_end,            ///< In a cascading chain of delegates, this is sent to the child, which should return `nullptr`.
        evt_scalar,         ///< In a cascading chain of delegates, this is sent to the child, which should return `nullptr`.
        evt_member_name,    ///< In a cascading chain of delegates, this is sent to the parent, which should return the appropriate child delegate to capture the member's value.
        evt_array,          ///< In a cascading chain of delegates, this is sent to the parent, which should return the appropriate child delegate to capture the array's elements.
        evt_object,         ///< In a cascading chain of delegates, this is sent to the parent, which should return the appropriate child delegate to capture the object's members.
    };

    /// @brief Event receiver.
    struct Delegate {
        /// @brief The delegate recieves the events here.
        /// @returns If `nullptr`, the current delegate is removed and the previous delegate becomes the current one. If a different delegate is returned, it becomes the new delegate and the current one becomes the previous delegate.
        virtual Delegate *receive(enum EventType type, StringView const &value) = 0;
        virtual ~Delegate() {}
    };

    /// @brief Generate the events.
    /// @returns The number of bytes processed.
    size_t parse() {
        auto const start = cur;
        value();
        return cur - start;
    }

    JSONParser(char const *const json, Delegate *delegate)
    : cur(json)
    , pstack({1})
    , delegates({delegate})
    {
        assert(delegate != nullptr);
    }

    /// @brief Determine if a scalar a string value.
    /// @note The value is not validated. Assumes the parameter is a value that accompanied an `evt_scalar` event, else the behavior is undefined.
    /// @param scalar The scalar to examine.
    /// @returns `true` if the scalar appears to be a string.
    static bool scalarIsString(StringView const &scalar) {
        return scalar.front() == '"';
    }

    /// @brief Determine if a scalar a JSON null.
    /// @param scalar The scalar to examine.
    /// @returns `true` if the scalar is a JSON null.
    static bool scalarIsNull(StringView const &scalar) {
        return scalar == "null";
    }

    /// @brief Determine if a scalar a JSON boolean `true`.
    /// @param scalar The scalar to examine.
    /// @returns `true` if the scalar is a JSON boolean `true`.
    static bool scalarIsTrue(StringView const &scalar) {
        return scalar == "true";
    }

    /// @brief Determine if a scalar a JSON boolean `false`.
    /// @param scalar The scalar to examine.
    /// @returns `true` if the scalar is a JSON boolean `false`.
    static bool scalarIsFalse(StringView const &scalar) {
        return scalar == "false";
    }

    /// @brief Determine if a scalar a JSON numeric value.
    /// @note The value is not parsed or validated. Assumes the parameter is a value that accompanied an `evt_scalar` event. The behavior is undefined elsewise.
    /// @param scalar The scalar to examine.
    /// @returns true if the scalar does not appear to be any of the other value types.
    static bool scalarIsNumber(StringView const &scalar) {
        return !scalarIsString(scalar)
            && !scalarIsNull(scalar)
            && !scalarIsTrue(scalar)
            && !scalarIsFalse(scalar);
    }

private:
    char const *cur;
    SmallArray<uint64_t, 64> pstack; ///< for parser state to avoid recursion
    SmallArray<Delegate *, 64> delegates;

    void send(enum EventType const type, char const *const start, char const *const end)
    {
        auto const delegate = delegates.back();
        auto const nd = delegate->receive(type, StringView(start, end - start));

        if (nd == nullptr)
            delegates.pop_back();
        else if (nd != delegate)
            delegates.push_back(nd);
    }

    // MARK: Parsing state
    void push_state(bool is_object) {
        auto &state = pstack.back();
        if (state < INT64_MAX)
            state = (state << 1) | (is_object ? 1 : 0);
        else
            pstack.push_back((1 << 1) | (is_object ? 1 : 0));
    }
    bool pop_state() {
        while (!pstack.empty()) {
            auto &state = pstack.back();
            assert(state > 1);
            state >>= 1;
            if (state > 1)
                return true;
            pstack.pop_back();
            return !pstack.empty();
        }
        return false;
    }
    bool is_parsing_array() const {
        auto state = pstack.back();
        return state > 1 && (state & 1) == 0;
    }
    bool is_parsing_object() const {
        auto state = pstack.back();
        return state > 1 && (state & 1) != 0;
    }

    // MARK: Parsing primitives
    /// @brief Get next character (byte) from input string.
    int next() {
        auto const ch = *cur;
        if (ch > 0) {
            ++cur;
            return ch;
        }
        DEBUG_OUT << "unexpected end of input" << std::endl;
        throw EndOfInput();
    }

    /// @brief Get next non-whitespace character (byte) from input string.
    int not_space() {
        for ( ; ; ) {
            auto const ch = next();
            if (!isspace(ch))
                return ch;
        }
    }

    /// @brief Check the next non-whitespace character and consume the character if it is equal to some particular character.
    bool expect(int const ch) {
        if (not_space() == ch)
            return true;
        --cur;
        return false;
    }

    // MARK: productions

    /// @brief Parse a value.
    ///
    /// @discussion This is the function that loops through the input and generates the events.
    /// It subsumes the `object` and `array` productions.
    void value();

    /// @brief Parse an string.
    void string();

    /// @brief Parse a word (stops at whitespace or punctuation).
    void word();
};

static inline std::ostream &operator <<(std::ostream &os, JSONParser::EventType evt) {
    switch (evt) {
    case JSONParser::evt_end:
        return os << "object or array end";
    case JSONParser::evt_array:
        return os << "array start";
    case JSONParser::evt_object:
        return os << "object start";
    case JSONParser::evt_scalar:
        return os << "simple value";
    case JSONParser::evt_member_name:
        return os << "object member";
    default:
        throw std::logic_error("unreachable");
    }
}

/// @brief Thrown on failure to convert a JSON scalar value to a C++ type.
struct JSONScalarConversionError {};

/// @brief Thrown if member name is not a valid C identifier.
struct BadMemberNameError : public JSONScalarConversionError {};

struct JSONStringConversionInvalidUTF8Error : public JSONScalarConversionError {};
struct JSONStringConversionInvalidUTF16Error : public JSONScalarConversionError {};

#ifndef PEDANTIC_MEMBER_NAMES
#define PEDANTIC_MEMBER_NAMES 1
#endif

/// @brief Represents a JSON string; converts to UTF-8 `std::string`.
struct JSONString {
    StringView value;
#if PEDANTIC_MEMBER_NAMES
    bool isMemberName;
#endif
    explicit JSONString(StringView const &value, bool isMemberName = false)
    : value(value.data() + (isMemberName ? 0 : 1), value.size() - (isMemberName ? 0 : 2))
#if PEDANTIC_MEMBER_NAMES
    , isMemberName(isMemberName)
#endif
    {
        if (!isMemberName && !(JSONParser::scalarIsString(value) && value.back() == '"')) {
            DEBUG_OUT << "not a string value: " << value << std::endl;
            throw JSONScalarConversionError();
        }
        if (isMemberName && value.empty()) {
            DEBUG_OUT << "member names can not be empty" << std::endl;
            throw JSONScalarConversionError();
        }
    }
    operator std::string() const;
};

/// @brief Represents a JSON `null`; used to convert to `void`.
struct JSONNull {
    explicit JSONNull(StringView const &value) {
        if (value == "null")
            return;

        DEBUG_OUT << "not null: " << value << std::endl;
        throw JSONScalarConversionError();
    }
};

/// @brief Represents JSON numeric value; does nothing but store it.
struct JSONNumber {
    StringView value;
    explicit JSONNumber(StringView const &value)
    : value(value)
    {}
    operator std::string() const {
        return std::string(value.begin(), value.end());
    }
};

/// @brief Represents JSON `true` and `false`; used to convert to `bool`.
struct JSONBool {
    bool value;

    static bool decode(StringView const &value) {
        if (value == "true")
            return true;

        if (value == "false")
            return false;

        DEBUG_OUT << "not true or false: " << value << std::endl;
        throw JSONScalarConversionError();
    }
    explicit JSONBool(StringView const &value)
    : value(decode(value))
    {}
    operator bool() const { return value; }
};

/// @brief Provides the conversion mapping.
template <typename T> struct DefaultJSONType {
    using Source = void;
};

template <> struct DefaultJSONType<std::string> {
    using Source = JSONString;
};

template <> struct DefaultJSONType<opt_string> {
    using Source = JSONString;
};

template <> struct DefaultJSONType<bool> {
    using Source = JSONBool;
};

template <> struct DefaultJSONType<void> {
    using Source = JSONNull;
};

/// @brief Parser delegate for capturing a JSON scalar value for constructing a C++ object.
/// @param RESULT the C++ type of the object to be assigned.
/// @param SOURCE the representation class for the JSON scalar type.
template <typename RESULT, typename SOURCE = typename DefaultJSONType<RESULT>::Source>
struct JSONValueDelegate final
: public JSONParser::Delegate
{
private:
    bool wasSet;
    StringView value;

    virtual JSONParser::Delegate *receive(JSONParser::EventType type, StringView const &value)
    {
        TRACE(type);
        TRACE(value);
        assert(type == JSONParser::evt_scalar);
        this->value = value;
        wasSet = true;
        return nullptr;
    }
public:
    JSONValueDelegate()
    : wasSet(false)
    {}
    bool isSet() const { return wasSet; }
    operator bool() const { return wasSet; }
    RESULT get() const {
        TRACE(wasSet);
        assert(wasSet);
        return SOURCE(value);
    }
    StringView const &rawValue() const {
        return value;
    }
    void unset() {
        wasSet = false;
    }
};

/// @brief A consume-and-discard reciever. Use it to ignore some part of the whole.
template <>
struct JSONValueDelegate<void> final
: public JSONParser::Delegate
{
private:
    int level;
    bool wasSet;

    virtual JSONParser::Delegate *receive(JSONParser::EventType type, StringView const &value)
    {
        TRACE(type);

        switch (type) {
        case JSONParser::evt_array:
        case JSONParser::evt_object:
            ++level;
            break;
        case JSONParser::evt_end:
            if (--level > 0)
                break;
            // fallthrough
        case JSONParser::evt_scalar:
            if (level == 0) {
                wasSet = true;
                return nullptr;
            }
            break;
        default:
            break;
        }
        return this;

        (void)(value);
    }
public:
    JSONValueDelegate()
    : level(0)
    , wasSet(false)
    {}
    operator bool() const { return wasSet; }
    void unset() {
        wasSet = false;
        level = 0;
    }
};

/// @brief A receiver for arrays of objects.
/// @param ITEM The C++ type for the objects.
/// @param COL The type for the collection.
/// @param RCVR The delegate type to receive the objects.
template <typename ITEM, typename COL = std::vector<ITEM>, typename RCVR = typename ITEM::Delegate>
struct ArrayDelegate final
: public JSONParser::Delegate
{
private:
    COL collection;
    RCVR rcvr;
    bool wasSet = false; ///< becomes true when the array start event is received.
    bool haveObject = false;

    void collect() {
        collection.emplace_back(rcvr.get((int)(collection.size() + 1)));
        rcvr.unset();
    }

    virtual JSONParser::Delegate *receive(JSONParser::EventType type, StringView const &value)
    {
        TRACE(type);

        switch (type) {
        case JSONParser::evt_array:
            wasSet = true;
            return this;
        case JSONParser::evt_object:
            if (haveObject)
                collect();
            haveObject = true;
            return &rcvr;
        case JSONParser::evt_end:
            if (haveObject)
                collect();
            haveObject = false;
            return nullptr;
        default:
            throw std::runtime_error("not an object in an array of objects!?");
        }
        (void)(value);
    }
public:
    ArrayDelegate()
    {}
    operator bool() const { return wasSet; }
    COL const &get() const { return collection; }
    void unset() {
        auto empty = COL();
        empty.reserve(collection.size());
        wasSet = false;
        collection.swap(empty);
    }
};

/// @brief A receiver that captures in a dictionary the members of a non-hierarchical object.
/// @param ITEM The C++ type for the objects, needs to be constructible from a dictionary.
template <typename ITEM>
struct ObjectDelegate
: public JSONParser::Delegate
{
    using Dictionary = std::map<std::string, StringView>;
private:
    Dictionary members;
    Dictionary::iterator member, noMember;
    bool wasSet = false;

    virtual JSONParser::Delegate *receive(JSONParser::EventType type, StringView const &value)
    {
        TRACE(type);

        switch (type) {
        case JSONParser::evt_end:
            return nullptr;
        case JSONParser::evt_member_name:
            member = members.find(std::string(JSONString(value, true)));
            break;
        case JSONParser::evt_scalar:
            if (member != noMember) {
                member->second = value;
                member = noMember;
                wasSet = true;
            }
            break;
        default:
            if (member != noMember)
                throw std::runtime_error(member->first + " is not a simple value!?");
        }
        return this;
    }
public:
    ObjectDelegate(std::initializer_list<char const *> const &membrs)
    {
        for (auto & str : membrs)
            members[str] = StringView();
        noMember = members.end();
        member = noMember;
    }
    ObjectDelegate(std::vector<std::string> const &membrs)
    {
        for (auto & str : membrs)
            members[str] = StringView();
        noMember = members.end();
        member = noMember;
    }
    operator bool() const { return wasSet; }
    operator ITEM() const {
        assert(wasSet);
        return ITEM(members);
    }
    void unset() {
        wasSet = false;
    }
};

struct JSONTopLevelObjectError {};

/// @brief A utility delegate to handle the common top level object in JSON.
struct TopLevelObjectDelegate final : public JSONParser::Delegate {
private:
    JSONParser::Delegate *mainDelegate;

    virtual JSONParser::Delegate *receive(JSONParser::EventType type, StringView const &value)
    {
        TRACE(type);

        switch (type) {
        case JSONParser::evt_object:
            return mainDelegate;
        default:
            throw JSONTopLevelObjectError();
        }

        (void)(value);
    }
public:
    TopLevelObjectDelegate(JSONParser::Delegate *mainDelegate)
    : mainDelegate(mainDelegate)
    {}
};
