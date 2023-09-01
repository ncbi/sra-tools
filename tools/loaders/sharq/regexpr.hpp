#ifndef __SHARQ_REGEXPR_HPP__
#define __SHARQ_REGEXPR_HPP__

/**
 * @file regexpr.hpp
 * @brief utulity wrapper for RE2
 *
 */

#include <memory>
#include <iostream>

#include <re2/re2.h>

class CRegExprMatcher
/// Encapsulation for RE2 regexpr matcher
{
public:
    typedef std::vector<re2::StringPiece> MatchResult;

public:
    /**
     * @brief Construct a new CRegExprMatcher object
     *
     * @param pattern regex pattern
     *
     * @throws runtime error on invalid pattern
     */
    CRegExprMatcher( const std::string& pattern )
    {
        re.reset(new re2::RE2(pattern));
        if (!re->ok())
            throw std::runtime_error("Invalid regex '" + pattern + "'");
        int groupSize = re->NumberOfCapturingGroups();
        argv.resize(groupSize);
        args.resize(groupSize);
        match.resize(groupSize);
        for (int i = 0; i < groupSize; ++i) {
            args[i] = &argv[i];
            argv[i] = &match[i];
        }
    }

    virtual ~CRegExprMatcher() {}

    /**
     * @brief Check if matcher recognizes defline
     *
     * @param[in] defline string_view de fline to check
     * @return true if defline matches
     * @return false if defline does not match
     */
/*bool Matches(const std::string_view& input)
    {
        mLastInput = input;
        return re2::RE2::PartialMatchN(input, *re, args.empty() ? nullptr : &args[0], (int)args.size());
    }
*/        
    bool Matches(const re2::StringPiece& input)
    {
        mLastInput = input.as_string();
        return re2::RE2::PartialMatchN(input, *re, args.empty() ? nullptr : &args[0], (int)args.size());
    }

    /**
     * @brief return last input line
     *
     * @return const string&
     */
    const std::string& GetLastInput() const { return mLastInput; }

    /**
     * @brief return array of matched groups
     *
     * @param const vector<re2:StringPiece> &
     */
    const MatchResult& GetMatch() const { return match; }

    /**
     * @brief return array of matched groups, non-const version
     *
     * @param vector<re2:StringPiece> &
     */
    MatchResult& GetMatch() { return match; }

    /**
     * @brief print out the matched groups to the given ostream
     *
     * @param ostream & default std::cout
     */
    void DumpMatch( std::ostream & out = std::cout ) const
    {
        for( size_t i = 0 ; i < match.size(); ++i )
        {
            out << i << ": " << "\"" << match[i] << "\"" << std::endl;
        }
    }

    /**
     * @brief return the regex pattern
    */
    const std::string& GetPattern() const { return re->pattern(); }

protected:
    std::unique_ptr<re2::RE2> re;       ///< RE2 object
    std::vector<re2::RE2::Arg> argv;    ///< internal structure to facilitate capture of matched groups
    std::vector<re2::RE2::Arg*> args;   ///< internal structure to facilitate capture of matched groups
    std::string mLastInput;             ///< last input
    MatchResult match;                  ///< captured matched groups
};

#endif
