/*===========================================================================
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
* Unit tests for libs/inc/hashing.hpp
*/

#include <cassert>
#include <hashing.hpp>
#include <klib/checksum.h>

#include <ktst/unit_test.hpp>

using namespace std;

namespace VDB {

struct SHA_224 {
    using Value = SHA2::Digest<28>;
    using State = SHA224State;
    
    static State init() {
        State state;
        SHA224StateInit(&state);
        return state;
    }
    static void update(State &state, size_t const bytes, void const *data) {
        SHA224StateAppend(&state, data, bytes);
    }
    static Value finalize(State &state) {
        Value result;
        SHA224StateFinish(&state, result.byte);
        return result;
    }
};

struct SHA_256 {
    using Value = SHA2::Digest<32>;
    using State = SHA256State;
    
    static State init() {
        State state;
        SHA256StateInit(&state);
        return state;
    }
    static void update(State &state, size_t const bytes, void const *data) {
        SHA256StateAppend(&state, data, bytes);
    }
    static Value finalize(State &state) {
        Value result;
        SHA256StateFinish(&state, result.byte);
        return result;
    }
};

struct SHA_384 {
    using Value = SHA2::Digest<48>;
    using State = SHA384State;
    
    static State init() {
        State state;
        SHA384StateInit(&state);
        return state;
    }
    static void update(State &state, size_t const bytes, void const *data) {
        SHA384StateAppend(&state, data, bytes);
    }
    static Value finalize(State &state) {
        Value result;
        SHA384StateFinish(&state, result.byte);
        return result;
    }
};

struct SHA_512 {
    using Value = SHA2::Digest<64>;
    using State = SHA512State;
    
    static State init() {
        State state;
        SHA512StateInit(&state);
        return state;
    }
    static void update(State &state, size_t const bytes, void const *data) {
        SHA512StateAppend(&state, data, bytes);
    }
    static Value finalize(State &state) {
        Value result;
        SHA512StateFinish(&state, result.byte);
        return result;
    }
};

using SHA224 = struct HashFunction< SHA_224 >;
using SHA256 = struct HashFunction< SHA_256 >;
using SHA384 = struct HashFunction< SHA_384 >;
using SHA512 = struct HashFunction< SHA_512 >;

}

TEST_SUITE(QaStatsHashingTestSuite);

TEST_CASE(Empty_SHA224)
{
    auto digest = SHA224::hash(0, "");
    REQUIRE_EQ(digest, decltype(digest){"D14A028C2A3A2BC9476102BB288234C415A2B01F828EA62AC5B3E42F"});
}

TEST_CASE(Empty_VDB_SHA224)
{
    auto digest = VDB::SHA224::hash(0, "");
    REQUIRE_EQ(digest, decltype(digest){"D14A028C2A3A2BC9476102BB288234C415A2B01F828EA62AC5B3E42F"});
}

TEST_CASE(Empty_SHA256)
{
    auto digest = SHA256::hash(0, "");
    REQUIRE_EQ(digest, decltype(digest){"E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855"});
}

TEST_CASE(Empty_VDB_SHA256)
{
    auto digest = VDB::SHA256::hash(0, "");
    REQUIRE_EQ(digest, decltype(digest){"E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855"});
}

TEST_CASE(Empty_SHA384)
{
    auto digest = SHA384::hash(0, "");
    REQUIRE_EQ(digest, decltype(digest){"38B060A751AC96384CD9327EB1B1E36A21FDB71114BE07434C0CC7BF63F6E1DA274EDEBFE76F65FBD51AD2F14898B95B"});
}

TEST_CASE(Empty_VDB_SHA384)
{
    auto digest = VDB::SHA384::hash(0, "");
    REQUIRE_EQ(digest, decltype(digest){"38B060A751AC96384CD9327EB1B1E36A21FDB71114BE07434C0CC7BF63F6E1DA274EDEBFE76F65FBD51AD2F14898B95B"});
}

TEST_CASE(Empty_SHA512)
{
    auto digest = SHA512::hash(0, "");
    REQUIRE_EQ(digest, decltype(digest){"CF83E1357EEFB8BDF1542850D66D8007D620E4050B5715DC83F4A921D36CE9CE47D0D13C5D85F2B0FF8318D2877EEC2F63B931BD47417A81A538327AF927DA3E"});
}

TEST_CASE(Empty_VDB_SHA512)
{
    auto digest = VDB::SHA512::hash(0, "");
    REQUIRE_EQ(digest, decltype(digest){"CF83E1357EEFB8BDF1542850D66D8007D620E4050B5715DC83F4A921D36CE9CE47D0D13C5D85F2B0FF8318D2877EEC2F63B931BD47417A81A538327AF927DA3E"});
}

namespace QBF { // The quick brown fox
    static std::string_view const value = "The quick brown fox jumps over the lazy dog";
    static std::string const hash224 = "730E109BD7A8A32B1CB9D9A09AA2325D2430587DDBC0C38BAD911525";
    static std::string const hash256 = "D7A8FBB307D7809469CA9ABCB0082E4F8D5651E46D3CDB762D02D0BF37C9E592";
    static std::string const hash384 = "CA737F1014A48F4C0B6DD43CB177B0AFD9E5169367544C494011E3317DBF9A509CB1E5DC1E85A941BBEE3D7F2AFBC9B1";
    static std::string const hash512 = "07E547D9586F6A73F73FBAC0435ED76951218FB7D0C8D788A309D785436BBB642E93A252A954F23912547D1E8A3B5ED6E1BFD7097821233FA0538F3DB854FEE6";
}

TEST_CASE(SHA224_QBF)
{
    auto const digest = SHA224::hash(QBF::value).string();
    REQUIRE_EQ(digest, QBF::hash224);
}

TEST_CASE(VDB_SHA224_QBF)
{
    auto const digest = VDB::SHA224::hash(QBF::value).string();
    REQUIRE_EQ(digest, QBF::hash224);
}

TEST_CASE(SHA256_QBF)
{
    auto const digest = SHA256::hash(QBF::value).string();
    REQUIRE_EQ(digest, QBF::hash256);
}

TEST_CASE(VDB_SHA256_QBF)
{
    auto const digest = VDB::SHA256::hash(QBF::value).string();
    REQUIRE_EQ(digest, QBF::hash256);
}

TEST_CASE(SHA384_QBF)
{
    auto const digest = SHA384::hash(QBF::value).string();
    REQUIRE_EQ(digest, QBF::hash384);
}

TEST_CASE(VDB_SHA384_QBF)
{
    auto const digest = VDB::SHA384::hash(QBF::value).string();
    REQUIRE_EQ(digest, QBF::hash384);
}

TEST_CASE(SHA512_QBF)
{
    auto const digest = SHA512::hash(QBF::value).string();
    REQUIRE_EQ(digest, QBF::hash512);
}

TEST_CASE(VDB_SHA512_QBF)
{
    auto const digest = VDB::SHA512::hash(QBF::value).string();
    REQUIRE_EQ(digest, QBF::hash512);
}

TEST_CASE(SHA_224)
{
    auto const N = sizeof(uint32_t) * 16 * 2;
    auto const string = std::string(N, 'A');
    for (int i = 0; i < N; ++i) {
        auto const str = string.substr(0, i);
        auto const xpt = VDB::SHA224::hash(str);
        auto const got = SHA224::hash(str);
        REQUIRE_EQ(got, xpt);
    }
}

TEST_CASE(SHA_256)
{
    auto const N = sizeof(uint32_t) * 16 * 2;
    auto const string = std::string(N, 'A');
    for (int i = 0; i < N; ++i) {
        auto const str = string.substr(0, i);
        auto const xpt = VDB::SHA256::hash(str);
        auto const got = SHA256::hash(str);
        REQUIRE_EQ(got, xpt);
    }
}

TEST_CASE(SHA_384)
{
    auto const N = sizeof(uint64_t) * 16 * 2;
    auto const string = std::string(N, 'A');
    for (int i = 0; i < N; ++i) {
        auto const str = string.substr(0, i);
        auto const xpt = VDB::SHA384::hash(str);
        auto const got = SHA384::hash(str);
        REQUIRE_EQ(got, xpt);
    }
}

TEST_CASE(SHA_512)
{
    auto const N = sizeof(uint64_t) * 16 * 2;
    auto const string = std::string(N, 'A');
    for (int i = 0; i < N; ++i) {
        auto const str = string.substr(0, i);
        auto const xpt = VDB::SHA512::hash(str);
        auto const got = SHA512::hash(str);
        REQUIRE_EQ(got, xpt);
    }
}

int main (int argc, char *argv [])
{
    return QaStatsHashingTestSuite(argc, argv);
}
