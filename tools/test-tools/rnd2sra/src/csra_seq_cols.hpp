#pragma once

#include "../vdb/wvdb.hpp"
#include "csra_seq_rec.hpp"

using namespace std;
using namespace vdb;

namespace sra_convert {

class SeqCols;
typedef std::shared_ptr<SeqCols> SeqColsPtr;
class SeqCols {
    private :
        VCurPtr f_cur;
        bool f_ok;
        VDB_Column_Ptr f_name_column;
        VDB_Column_Ptr f_platform_column;
        VDB_Column_Ptr f_read_column;
        VDB_Column_Ptr f_cmp_read_column;
        VDB_Column_Ptr f_spot_grp_column;
        VDB_Column_Ptr f_qual_column;
        VDB_Column_Ptr f_read_start_column;
        VDB_Column_Ptr f_read_len_column;
        VDB_Column_Ptr f_prim_al_id_column;
        VDB_Column_Ptr f_read_type_column;
        VDB_Column_Ptr f_read_filter_column;

        VDB_Column_Ptr add_column( const string& name ) {
            auto res = VDB_Column::make( name, f_cur );
            if ( ! *res ) {
                cerr << "SeqCols() make "<< name << "-column failed\n";
                f_ok = false;
            }
            return res;
        }

        SeqCols( VCurPtr cur ) : f_cur( cur ), f_ok( true ) {
            f_name_column = add_column( "NAME" );
            if ( f_ok ) { f_platform_column = add_column( "PLATFORM" ); }
            if ( f_ok ) { f_read_column = add_column( "READ" ); }
            if ( f_ok ) { f_cmp_read_column = add_column( "CMP_READ" ); }
            if ( f_ok ) { f_spot_grp_column = add_column( "SPOT_GROUP" ); }
            if ( f_ok ) { f_qual_column = add_column( "QUALITY" ); }
            if ( f_ok ) { f_read_start_column = add_column( "READ_START" ); }
            if ( f_ok ) { f_read_len_column = add_column( "READ_LEN" ); }
            if ( f_ok ) { f_prim_al_id_column = add_column( "PRIMARY_ALIGNMENT_ID" ); }
            if ( f_ok ) { f_read_type_column = add_column( "READ_TYPE" ); }
            if ( f_ok ) { f_read_filter_column = add_column( "READ_FILTER" ); }
            if ( f_ok ) {
                f_ok = f_cur -> open();
                if ( !f_ok ) { cerr << "SeqCols() open cursor failed\n"; }
            }
        }

    public :
        static SeqColsPtr make( VCurPtr cur ) { return SeqColsPtr( new SeqCols( cur ) ); }
        operator bool() const { return f_ok; }

        bool set_dflt_platform( uint8_t platform ) {
            return f_platform_column -> dflt_u8( platform );
        }

        bool write( const SeqRec& rec ) {
            bool res = f_cur -> open_row();
            if ( res ) { res = f_name_column -> write_string( rec . get_name() ); }
            if ( res ) { res = f_read_column -> write_string( rec . get_read() ); }
            if ( res ) { res = f_cmp_read_column -> write_string( rec . get_cmp_read() ); }
            if ( res ) { res = f_spot_grp_column -> write_string( rec . get_spot_grp() ); }
            if ( res ) { res = f_qual_column -> write_u8arr( rec . get_qual() ); }
            if ( res ) { res = f_read_start_column -> write_u32arr( rec . get_read_start() ); }
            if ( res ) { res = f_read_len_column -> write_u32arr( rec . get_read_len() ); }
            if ( res ) { res = f_prim_al_id_column -> write_u64arr( rec . get_prim_al_id() ) ; }
            if ( res ) { res = f_read_type_column -> write_u8arr( rec . get_read_type() ); }
            if ( res ) { res = f_read_filter_column -> write_u8arr( rec . get_read_filter() ); }
            if ( res ) { res = f_cur -> commit_row(); }
            if ( res ) { res = f_cur -> close_row(); }
            return res;
        }
};

}
