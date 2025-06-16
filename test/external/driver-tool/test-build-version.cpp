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
 *  tests for version parsing
 *
 */

#include "build-version.cpp"

struct Test_Version {
    using Version = sratools::Version;

    void test_exe_lin() {
        std::string name{"sratools.3.0.1"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(3, 0, 1))) throw __FUNCTION__;
        if (!(name == "sratools")) throw __FUNCTION__;
    }
    void test_exe_win() {
        std::string name{"sratools.3.0.1.exe"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(3, 0, 1))) throw __FUNCTION__;
        if (!(name == "sratools.exe")) throw __FUNCTION__;
    }
    void test_so0() {
        std::string name{"libncbi-vdb.so"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version())) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.so")) throw __FUNCTION__;
    }
    void test_dylib0() {
        std::string name{"libncbi-vdb.dylib"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version())) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.dylib")) throw __FUNCTION__;
    }
    void test_dylib1() {
        std::string name{"libncbi-vdb.1.dylib"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(1, 0, 0))) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.dylib")) throw __FUNCTION__;
    }
    void test_so_1() {
        std::string name{"libncbi-vdb.so.1"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(1, 0, 0))) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.so")) throw __FUNCTION__;
    }
    void test_dylib2() {
        std::string name{"libncbi-vdb.1.3.dylib"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(1, 3, 0))) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.dylib")) throw __FUNCTION__;
    }
    void test_so_2() {
        std::string name{"libncbi-vdb.so.1.3"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(1, 3, 0))) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.so")) throw __FUNCTION__;
    }
    void test_dylib3() {
        std::string name{"libncbi-vdb.1.3.2.dylib"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(1, 3, 2))) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.dylib")) throw __FUNCTION__;
    }
    void test_so_3() {
        std::string name{"libncbi-vdb.so.1.3.2"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(1, 3, 2))) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.so")) throw __FUNCTION__;
    }
    void test_dylib4() {
        std::string name{"libncbi-vdb.1.3.2.4.dylib"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(3, 2, 4))) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.1.dylib")) throw __FUNCTION__;
    }
    void test_so_4() {
        std::string name{"libncbi-vdb.so.1.3.2.4"};
        auto const vers = Version::removeVersion(name);
        if (!(vers == Version(3, 2, 4))) throw __FUNCTION__;
        if (!(name == "libncbi-vdb.so.1")) throw __FUNCTION__;
    }
    void test_to_string() {
        auto const vers = Version(1, 2, 3);
        auto const versString = (std::string)vers;
        if (!(versString == "1.2.3")) throw __FUNCTION__;
    }
    Test_Version()
    : is_good(false)
    {
        try {
            test_exe_win();
            test_exe_lin();
            test_so0();
            test_dylib1();
            test_dylib2();
            test_dylib3();
            test_dylib4();
            test_so_1();
            test_so_2();
            test_so_3();
            test_so_4();
            test_to_string();
            std::cerr << __FUNCTION__ << " passed." << std::endl;
            is_good = true;
        }
        catch (char const *func) {
            std::cerr << func << " failed!" << std::endl;
        }
        catch (...) {
            std::cerr << __FUNCTION__ << " failed: unknown exception!" << std::endl;
        }
    }

    bool is_good;
};

#include <cassert>

int main ( int argc, char *argv[], char *envp[] )
{
    try {
        Test_Version const test_Version;
        assert(test_Version.is_good);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }
    return 0;
    (void)argc, (void)argv, (void)envp;
}
