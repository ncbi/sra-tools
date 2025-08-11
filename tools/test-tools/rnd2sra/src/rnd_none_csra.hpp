#pragma once

#include "rnd2sra_ini.hpp"
#include "rnd_cmn.hpp"
#include <cstdint>

using namespace std;
using namespace vdb;

namespace sra_convert {

static const char * none_csra_schema_txt = R"(
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

table SEQ_TBL #1 {

    column ascii NAME;
    column INSDC:SRA:platform_id PLATFORM;

    column INSDC:dna:text READ;

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

    readonly column U64 BASE_COUNT = < U64 > meta:value < "STATS/TABLE/BASE_COUNT" >();
    readonly column U64 BIO_BASE_COUNT = < U64 > meta:value < "STATS/TABLE/BIO_BASE_COUNT" >();
    readonly column U64 SPOT_COUNT = < U64 > meta:value < "STATS/TABLE/SPOT_COUNT" >();

    readonly column INSDC:coord:zero TRIM_START = < INSDC:coord:zero > echo < 0 > ();
    readonly column INSDC:coord:len TRIM_LEN = ( INSDC:coord:len ) row_len( READ );
    readonly column INSDC:coord:len SPOT_LEN = ( INSDC:coord:len ) row_len( READ );
    readonly column INSDC:dna:text CS_KEY = < INSDC:dna:text > echo < "TTTT" > ();
    readonly column INSDC:color:text CSREAD = <INSDC:dna:text, INSDC:color:text> map < 'ACGT', '0123'>( READ );

};

database MAIN_DB #1 {
    table SEQ_TBL #1 SEQUENCE;
};

)";

class RndNonecSra : public Rndcmn {
    protected :
        VDB_Column_Ptr f_name_column;
        VDB_Column_Ptr f_read_column;
        VDB_Column_Ptr f_qual_column;
        VDB_Column_Ptr f_spotgroup_column;
        VDB_Column_Ptr f_readfilter_column;
        VDB_Column_Ptr f_readlen_column;
        VDB_Column_Ptr f_readstart_column;
        VDB_Column_Ptr f_readtype_column;
        VDB_Column_Ptr f_platform_column;
        uint8_arr qual_arr;

        // Ctor
        RndNonecSra( VMgrPtr mgr, rnd2sra_ini_ptr ini,
                     RandomPtr rnd, const string& output_dir )
            : Rndcmn( mgr, ini, rnd, output_dir, none_csra_schema_txt ) {
                qual_arr . data = nullptr;
                qual_arr . len = 0;
            }

        bool initialize_columns( VCurPtr cur ) {
            bool use_name = with_name();
            bool use_spot_group = with_spot_group();
            if ( use_name ) { f_name_column = VDB_Column::make( "NAME", cur ); }
            f_read_column = VDB_Column::make( "READ", cur );
            f_qual_column = VDB_Column::make( "QUALITY", cur );
            if ( use_spot_group ) { f_spotgroup_column = VDB_Column::make( "SPOT_GROUP", cur ); }
            f_readfilter_column = VDB_Column::make( "READ_FILTER", cur );
            f_readlen_column = VDB_Column::make( "READ_LEN", cur );
            f_readstart_column = VDB_Column::make( "READ_START", cur );
            f_readtype_column = VDB_Column::make( "READ_TYPE", cur );
            f_platform_column = VDB_Column::make( "PLATFORM", cur );

            bool res = true;
            if ( use_name ) { res = *f_name_column; }
            if ( res ) { res = *f_read_column; }
            if ( res ) { res = *f_qual_column; }
            if ( res && use_spot_group ) { res = *f_spotgroup_column; }
            if ( res ) { res = *f_readfilter_column; }
            if ( res ) { res = *f_readlen_column; }
            if ( res ) { res = *f_readstart_column; }
            if ( res ) { res = *f_readtype_column; }
            if ( res ) { res = *f_platform_column; }
            if ( res ) {
                res = cur -> open();
                if ( !res ) cerr << "initialize_columns . open_cursor() failed\n";
            }
            return res;
        }

        static std::string_view substr_view( const std::string& source,
                        size_t offset = 0,
                        std::string_view::size_type count =
                        std::numeric_limits<std::string_view::size_type>::max()) {
            if ( offset < source . size() )
                return std::string_view( source . data() + offset,
                                std::min( source . size() - offset, count) );
            return {};
        }

        bool write_row( VCurPtr cur, int64_t row ) {
            if ( !cur -> open_row() ) {
                cerr << "open_row( " << row << " ) failed\n";
                return false;
            }

            // --- NAME
            if ( with_name() ) {
                if ( ! f_name_column -> write_string( make_name() ) ) {
                    return false;
                }
            }

            auto spot_layout = f_ini -> select_spot_layout( f_rnd );
            spot_layout -> randomize( f_rnd );
            size_t row_len = spot_layout -> get_len();

            // --- READ
            auto bases = f_rnd -> random_bases( row_len );
            if ( ! f_read_column -> write_string( bases ) ) {
                return false;
            }

            // --- QUALITY
            if ( row_len > qual_arr . len ) {
                if ( nullptr != qual_arr . data ) { free( ( void * ) qual_arr . data ); }
                qual_arr . data = ( uint8_t * ) malloc( row_len );
                if ( nullptr != qual_arr . data ) {
                    qual_arr .  len = row_len;
                } else {
                    qual_arr .  len = 0;
                }
            }
            if ( nullptr == qual_arr . data ) {
                return false;
            }
            // optionally introduce an error in the quality length
            size_t q_row_len = make_random_qual( ( uint8_t * )qual_arr . data, row, row_len );
            if ( ! f_qual_column -> write_u8arr( ( uint8_t * )qual_arr . data, q_row_len ) ) {
                return false;
            }

            // --- SPOT_GROUP
            if ( with_spot_group() ) {
                if ( ! f_spotgroup_column -> write_string( f_ini -> select_spot_group( f_rnd ) ) ) {
                    return false;
                }
            }

            size_t read_count = spot_layout -> section_count();
            if ( read_count > 0 ) {
                // --- READ_START
                int32_t rs[ 1024 ];
                spot_layout -> populate_read_start( rs );
                if ( ! f_readstart_column -> write_i32arr( rs, read_count ) ) { return false; }

                // --- READ_LEN
                uint32_t rl[ 1024 ];
                spot_layout -> populate_read_len( rl );
                // optional fault injection!
                int32_t ofs = f_ini -> read_len_offset( row );
                if ( ofs != 0 ) {
                    if ( read_count > 1 ) { rl[ 1 ] += ofs; } else { rl[ 0 ] += ofs; }
                }
                if ( ! f_readlen_column -> write_u32arr( rl, read_count ) ) { return false; }

                // --- record the fingerprint( because we have to split the SPOT into READs !!! )
                size_t offset = 0;
                for ( uint32_t read_idx = 0; read_idx < read_count; ++read_idx ) {
                    size_t count = rl[ read_idx ];
                    auto read = substr_view( bases, offset, count );
                    f_fingerprint . record( read );
                    offset += count;
                }

                // --- READ_FILTER
                uint8_t rf[ 1024 ];
                spot_layout -> populate_read_filter( rf );
                if ( ! f_readfilter_column -> write_u8arr( rf, read_count ) ) { return false; }

                // --- READ_TYPE
                uint8_t rt[ 1024 ];
                spot_layout -> populate_read_type( rt );
                if ( ! f_readtype_column -> write_u8arr( rt, read_count ) ) { return false; }
            } else {
                cerr << "no sections in spot-layout in row : " << row << endl;
                return false;
            }

            if ( ! cur -> commit_row() ) {
                cerr << "commit_row ( " << row << " ) failed\n";
                return false;
            }

            if ( ! cur -> close_row() ) {
                cerr << "close_row ( " << row << " ) failed\n";
                return false;
            }

            f_counters . total += row_len;
            f_counters . bio += spot_layout -> get_bio_base_count();

            return true;
        }

        bool populate_table( VTblPtr tbl ) {
            //  (1) ... create a writable cursor
            auto cur = tbl -> writable_cursor();
            bool res = ( *cur );
            if ( !res ) {
                cerr << "populate_table() - make_cursor() failed!\n";
            }

            //  (2) ... populate the cursor with all the columns we want to write
            if ( res ) { res = initialize_columns( cur ); }

            //  (3) ... set the default-value for the PLATFORM-column
            if ( res ) {
                uint8_t platform = 2; // ILLUMINA
                res = f_platform_column -> dflt_u8( platform );
            }

            //  (4) ... write the rows  [[[ the main loop ]]]
            uint64_t row_count = f_ini -> get_rows();
            base_counters counters;
            for ( int64_t row = 1; res && ( uint64_t )row <= row_count; ++row ) {
                res = write_row( cur, row );
            }

            //  (5) ... commit all the changes on the cursor
            if ( res ) { res = cur -> commit(); }

            //  (6) ... write the STATS-metadata
            if ( res ) { res = write_stats( tbl, f_counters, row_count ); }

            //  (7) ... write the fingerprint
            if ( res ) { res = write_fingerprint( tbl ); }

            return res;
        }

    public :
        // Dtor
        ~RndNonecSra( void ) {
            if ( nullptr != qual_arr . data ) {
                free( ( void * ) qual_arr . data );
                qual_arr . data = nullptr;
            }
        }
};

class RndNonecSraFlat : public RndNonecSra {
    private:

        // Ctor
        RndNonecSraFlat( VMgrPtr mgr, rnd2sra_ini_ptr ini,
                         RandomPtr rnd, const string& output_dir )
            : RndNonecSra( mgr, ini, rnd, output_dir ) {
        }

        bool run( void ) {
            auto tbl = f_mgr -> create_tbl( f_schema, "SEQ_TBL", f_output_dir, f_ini -> get_checksum() );
            if ( ! *tbl ) {
                cerr << "make tbl failed for " << f_output_dir << endl;
                return false;
            }
            return populate_table( tbl );
        }

    public:
        static bool produce( VMgrPtr mgr, rnd2sra_ini_ptr ini,
                             RandomPtr rnd, const string& output_dir ) {
            RndNonecSraFlat writer( mgr, ini, rnd, output_dir );
            return writer . run();
        }

};

class RndNonecSraDb : public RndNonecSra {
    private:

        // Ctor
        RndNonecSraDb( VMgrPtr mgr, rnd2sra_ini_ptr ini,
                       RandomPtr rnd, const string& output_dir )
            : RndNonecSra( mgr, ini, rnd, output_dir ) {
        }

        bool run( void ) {
            auto db = f_mgr -> create_db( f_schema, "MAIN_DB", f_output_dir, f_ini -> get_checksum() );
            bool res = ( *db );
            if ( !res ) {
                cerr << "make db failed for " << f_output_dir << endl;
                return res;
            }

            auto tbl = db -> create_tbl( "SEQUENCE", "SEQUENCE", f_ini -> get_checksum() );
            res = ( *tbl );
            if ( !res ) { cerr << "make tbl SEQUENCE failed\n"; return res; }

            return populate_table( tbl );
        }

    public:
        static bool produce( VMgrPtr mgr, rnd2sra_ini_ptr ini,
                             RandomPtr rnd, const string& output_dir ) {
            RndNonecSraDb writer( mgr, ini, rnd, output_dir );
            return writer . run();
        }
};

} // end of namespace sra_convert
