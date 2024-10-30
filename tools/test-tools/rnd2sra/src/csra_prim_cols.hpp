#pragma once

#include "../vdb/wvdb.hpp"

using namespace std;
using namespace vdb;

namespace sra_convert {

class PrimCols;
typedef std::shared_ptr<PrimCols> PrimColsPtr;
class PrimCols {
    private :
        VCurPtr f_cur;
        bool f_ok;
        VDB_Column_Ptr f_seq_spot_id_column;
        VDB_Column_Ptr f_seq_read_id_column;
        VDB_Column_Ptr f_raw_read_column;

        VDB_Column_Ptr add_column( const string& name ) {
            auto res = VDB_Column::make( name, f_cur );
            if ( ! *res ) {
                cerr << "PrimCols() make "<< name << "-column failed\n";
                f_ok = false;
            }
            return res;
        }

        PrimCols( VCurPtr cur ) : f_cur( cur ), f_ok( true ) {
            f_seq_spot_id_column = add_column( "SEQ_SPOT_ID" );
            if ( f_ok ) { f_seq_read_id_column = add_column( "SEQ_READ_ID" ); }
            if ( f_ok ) { f_raw_read_column = add_column( "RAW_READ" ); }
            if ( f_ok ) {
                f_ok = f_cur -> open();
                if ( !f_ok ) { cerr << "PrimCols() open cursor failed\n"; }
            }
        }

    public :
        static PrimColsPtr make( VCurPtr cur ) { return PrimColsPtr( new PrimCols( cur ) ); }
        operator bool() const { return f_ok; }

        bool write( int64_t seq_spot_id, uint32_t seq_read_id, const string& raw_read ) {
            bool res = f_cur -> open_row();
            if ( res ) { res = f_seq_spot_id_column -> write_i64( seq_spot_id ); }
            if ( res ) { res = f_seq_read_id_column -> write_u32( seq_read_id ); }
            if ( res ) { res = f_raw_read_column -> write_string( raw_read ); }
            if ( res ) { res = f_cur -> commit_row(); }
            if ( res ) { res = f_cur -> close_row(); }
            return res;
        }
};

}
