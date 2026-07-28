#pragma once

#include <cstdlib>
#include <string>
#include <iostream>

#include "../util/fs.hpp"

using namespace std;

namespace sra_convert {

class Normalizer {

    public:
        /* -----------------------------------------------------------------------
         example:   root1 == "A", root2 == "B", ext == "FASTA"

         CASE 1 : A.FASTA AND B.FASTA exist ---> do nothing

         CASE 2 : A_1.FASTA and B_1.FASTA exist ---> do nothing

         CASE 3 : A.FASTA and B_1.FASTA exist ---> rename A.FASTA to A_1.FASTA

         CASE 4 : A_1.FASTA and B.FASTA exist ---> rename A_1.FASTA to A.FASTA
        ----------------------------------------------------------------------- */
        static void run( const string& root1, const string& root2, const string& ext ) {
            string f1_bare = root1 + '.' + ext;
            string f2_bare = root2 + '.' + ext;
            bool f1_bare_exists = fs::exists( f1_bare );
            bool f2_bare_exists = fs::exists( f2_bare );
            if ( f1_bare_exists && f2_bare_exists ) { return; } // CASE 1

            string f1_num = root1 + "_1." + ext;
            string f2_num = root2 + "_1." + ext;
            bool f1_num_exists = fs::exists( f1_num );
            bool f2_num_exists = fs::exists( f2_num );
            if ( f1_num_exists && f2_num_exists ) { return; } // CASE 2

            if ( f1_bare_exists && f2_num_exists && !f1_num_exists && !f2_bare_exists ) {
                // CASE 3
                fs::rename( f1_bare, f1_num );
                return;
            }

            if ( f1_num_exists && f2_bare_exists && !f1_bare_exists && !f2_num_exists ) {
                // CASE 4
                fs::rename( f1_num, f1_bare );
            }
        }
};

} // end of namespace sra_convert
