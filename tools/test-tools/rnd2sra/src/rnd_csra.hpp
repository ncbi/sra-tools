#pragma once

#include "rnd2sra_ini.hpp"
#include "csra_spot_list.hpp"

using namespace std;
using namespace vdb;

namespace sra_convert {

static const char * csra_schema_txt = R"(
version 1;

typedef U8 INSDC:quality:phred;
typedef ascii INSDC:quality:text:phred_33;
typedef ascii INSDC:quality:text:phred_64;
typedef U8 INSDC:SRA:platform_id;
typedef ascii INSDC:dna:text;
typedef ascii INSDC:color:text;
typedef U8 INSDC:SRA:read_filter;
typedef U32 INSDC:coord:len;
typedef I32 INSDC:coord:zero;
typedef U8 INSDC:SRA:xread_type;

function any cast #1.0 ( any in ) = vdb:cast;
function < type T > T echo #1.0 < T val > ( * any row_len ) = vdb:echo;
function < type T > T sum #1.0 < * T k > ( T a, ... ) = vdb:sum;
function U32 row_len #1.0 ( any in ) = vdb:row_len;
function < type A, type B > B map #1.0 < A from, B to > ( A in, * B src ) = vdb:map;
function < type T > T meta:read #1.0 < ascii node, * bool deterministic > ();
function < type T > T meta:value #1.0 < ascii node, * bool deterministic > ();

table NCBI:align:seq #1 {

    column ascii NAME;
    column INSDC:SRA:platform_id PLATFORM;

    column INSDC:dna:text READ;
    column INSDC:dna:text CMP_READ;

    default column INSDC:quality:phred QUALITY;
    readonly column INSDC:quality:text:phred_33 QUALITY =
        ( INSDC:quality:text:phred_33 ) < B8 > sum < 33 >( QUALITY );
    readonly column INSDC:quality:text:phred_64 QUALITY =
        ( INSDC:quality:text:phred_64 ) < B8 > sum < 64 >( QUALITY );

    column ascii SPOT_GROUP;
    column INSDC:SRA:read_filter READ_FILTER;
    column INSDC:coord:len READ_LEN;
    column INSDC:coord:zero READ_START;
    column INSDC:SRA:xread_type READ_TYPE;
    column U64 PRIMARY_ALIGNMENT_ID;

    readonly column U64 BASE_COUNT = < U64 > meta:value < "STATS/TABLE/BASE_COUNT" >();
    readonly column U64 BIO_BASE_COUNT = < U64 > meta:value < "STATS/TABLE/BIO_BASE_COUNT" >();
    readonly column U64 SPOT_COUNT = < U64 > meta:value < "STATS/TABLE/SPOT_COUNT" >();

    readonly column INSDC:coord:zero TRIM_START = < INSDC:coord:zero > echo < 0 > ();
    readonly column INSDC:coord:len TRIM_LEN = ( INSDC:coord:len ) row_len( READ );
    readonly column INSDC:coord:len SPOT_LEN = ( INSDC:coord:len ) row_len( READ );
    readonly column INSDC:dna:text CS_KEY = < INSDC:dna:text > echo < "TTTT" > ();
    readonly column INSDC:color:text CSREAD = <INSDC:dna:text, INSDC:color:text> map < 'ACGT', '0123'>( READ );

};

table NCBI:align:prim #1 {
    column U64 SEQ_SPOT_ID;
    column U32 SEQ_READ_ID;
    column ascii RAW_READ;
};

table NCBI:align:ref #1 {
    column ascii READ;
};

database NCBI:align:db:alignment_sorted #1 {
    table NCBI:align:seq  #1 SEQUENCE;
    table NCBI:align:prim #1 PRIMARY_ALIGNMENT;
    table NCBI:align:ref #1 REFERENCE;
};

)";

class RndcSRA : public Rndcmn {
    private:
        // Ctor
        RndcSRA( VMgrPtr mgr, rnd2sra_ini_ptr ini, RandomPtr rnd, const string& output_dir )
            : Rndcmn( mgr, ini, rnd, output_dir, csra_schema_txt ) {
        }

        bool write_seq_tbl( VDbPtr db, cSRASpotListPtr spots ) {
            bool res = false;
            auto tbl = db -> create_tbl( "SEQUENCE", "SEQUENCE", f_ini -> get_checksum() );
            if ( ! *tbl ) {
                cerr << "write_seq_tbl() - create_tbl() failed!\n";
            } else {
                auto cur = tbl -> writable_cursor();
                if ( ! *cur ) {
                    cerr << "write_seq_tbl() - make_cursor() failed!\n";
                } else {
                    auto seq_cols = SeqCols::make( cur );
                    if ( ! *seq_cols ) {
                        cerr << "write_seq_tbl() - SeqCols::make() failed!\n";
                    } else {
                        if ( ! seq_cols -> set_dflt_platform( f_ini -> get_platform() ) ) {
                            cerr << "write_seq_tbl() - SeqCols::set_dflt_platform() failed!\n";
                        } else {
                            // ---------------------------------------------------------
                            if ( ! spots -> write_seq_cols( seq_cols, f_counters ) ) {
                            // ---------------------------------------------------------
                                cerr << "write_seq_tbl() - write_seq_cols() failed!\n";
                            } else {
                                if ( ! cur -> commit() ) {
                                    cerr << "write_seq_tbl() - cursor-commit() failed!\n";
                                } else {
                                    if ( ! write_stats( tbl, f_counters, spots -> row_count() ) ) {
                                        cerr << "write_seq_tbl() - write-stats() failed!\n";
                                    } else {
                                        res = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return res;
        }

        bool write_prim_tbl( VDbPtr db, cSRASpotListPtr spots ) {
            auto tbl = db -> create_tbl( "PRIMARY_ALIGNMENT", "PRIMARY_ALIGNMENT", f_ini -> get_checksum() );
            if ( *tbl ) {
                auto cur = tbl -> writable_cursor();
                if ( *cur ) {
                    auto prim_cols = PrimCols::make( cur );
                    if ( *prim_cols ) {
                        if ( spots -> write_prim_cols( prim_cols ) ) {  /* <--- write the spots ( csra_spot.hpp ) */
                            return cur -> commit();
                        } else {
                            cerr << "write_prim_tbl() - write_prim_cols() failed!\n";
                        }
                    } else {
                        cerr << "write_prim_tbl() - PrimCols::make() failed!\n";
                    }
                } else {
                    cerr << "write_prim_tbl() - make_cursor() failed!\n";
                }
            } else {
                cerr << "write_prim_tbl() - create_tbl() failed!\n";
            }
            return false;
        }

        bool write_ref_tbl( VDbPtr db ) {
            auto tbl = db -> create_tbl( "REFERENCE", "REFERENCE", f_ini -> get_checksum() );
            if ( *tbl ) {
                auto cur = tbl -> writable_cursor();
                if ( *cur ) {
                    auto ref_cols = RefCols::make( cur );
                    if ( *ref_cols ) {
                        /* let us write 10 rows of random strings to satisfy the need for a REFERENCE-table*/
                        bool res = true;
                        for ( uint32_t i = 0; res && i < 10; ++i ) {
                            auto bases = f_rnd -> random_bases( 50 );
                            res = ref_cols -> write( bases );
                        }
                    }
                }
            }
            return false;
        }

        bool run( void ) {
            // we create a spot-list from the parsed ini and the random-generator:
            auto spots = cSRASpotList::make( f_ini, f_rnd );

            auto db = f_mgr -> create_db( f_schema, "NCBI:align:db:alignment_sorted", f_output_dir, f_ini -> get_checksum() );
            bool res = ( *db );
            if ( res ) { res = write_seq_tbl( db, spots ); }
            if ( res ) { res = write_prim_tbl( db, spots ); }
            if ( res ) { res = write_ref_tbl( db ); }
            return res;
        }

    public:
        static int produce( VMgrPtr mgr, rnd2sra_ini_ptr ini,
                            RandomPtr rnd, const string& output_dir ) {
            RndcSRA rnd_csra( mgr, ini, rnd, output_dir );
            return rnd_csra . run();
        }
};

}  // end of namespace sra_convert
