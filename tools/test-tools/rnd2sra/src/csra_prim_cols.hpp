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
        VDB_Column_Ptr f_ref_id_column;
        VDB_Column_Ptr f_ref_pos_column;
        VDB_Column_Ptr f_ref_len_column;

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
            if ( f_ok ) { f_ref_id_column = add_column( "REF_ID" ); }
            if ( f_ok ) { f_ref_pos_column = add_column( "REF_POS" ); }
            if ( f_ok ) { f_ref_len_column = add_column( "REF_LEN" ); }
            if ( f_ok ) {
                f_ok = f_cur -> open();
                if ( !f_ok ) { cerr << "PrimCols() open cursor failed\n"; }
            }
        }

    public :
        static PrimColsPtr make( VCurPtr cur ) { return PrimColsPtr( new PrimCols( cur ) ); }
        operator bool() const { return f_ok; }

        bool write( int64_t seq_spot_id, uint32_t seq_read_id, const string& raw_read, int64_t ref_id ) {
            bool res = f_cur -> open_row();
            if ( res ) { res = f_seq_spot_id_column -> write_i64( seq_spot_id ); }
            if ( res ) { res = f_seq_read_id_column -> write_u32( seq_read_id ); }
            if ( res ) { res = f_raw_read_column -> write_string( raw_read ); }
            if ( res ) { res = f_ref_id_column -> write_i64( ref_id ); }
            if ( res ) { res = f_ref_pos_column -> write_u32( 1 ); }
            if ( res ) { res = f_ref_len_column -> write_u32( ( uint32_t )raw_read.length() ); }
            if ( res ) { res = f_cur -> commit_row(); }
            if ( res ) { res = f_cur -> close_row(); }
            return res;
        }
};

class RefCols;
typedef std::shared_ptr<RefCols> RefColsPtr;
class RefCols {
    private :
        VCurPtr f_cur;
        bool f_ok;
        VDB_Column_Ptr f_read_column;
        VDB_Column_Ptr f_prim_al_ids_column;
        VDB_Column_Ptr f_read_len_column;
        VDB_Column_Ptr f_name_column;
        VDB_Column_Ptr f_circular_column;

        VDB_Column_Ptr add_column( const string& name ) {
            auto res = VDB_Column::make( name, f_cur );
            if ( ! *res ) {
                cerr << "PrimCols() make "<< name << "-column failed\n";
                f_ok = false;
            }
            return res;
        }

        RefCols( VCurPtr cur ) : f_cur( cur ), f_ok( true ) {
            f_read_column = add_column( "READ" );
            if ( f_ok ) { f_prim_al_ids_column = add_column( "PRIMARY_ALIGNMENT_IDS" ); }
            if ( f_ok ) { f_read_len_column = add_column( "READ_LEN" ); }
            if ( f_ok ) { f_name_column = add_column( "NAME" ); }
            if ( f_ok ) { f_circular_column = add_column( "CIRCULAR" ); }
            if ( f_ok ) {
                f_ok = f_cur -> open();
                if ( !f_ok ) { cerr << "RefCols() open cursor failed\n"; }
            }
        }

    public :
        static RefColsPtr make( VCurPtr cur ) { return RefColsPtr( new RefCols( cur ) ); }
        operator bool() const { return f_ok; }

        bool write( const string& read, const int64_t* prim_al_ids, size_t prim_al_ids_count ) {
            bool res = f_cur -> open_row();
            if ( res ) { res = f_read_column -> write_string( read ); }
            if ( res ) { res = f_prim_al_ids_column -> write_i64arr( prim_al_ids, prim_al_ids_count ); }
            if ( res ) { res = f_read_len_column -> write_u32( read . length() ); }
            if ( res ) { res = f_name_column -> write_string( "chr1" ); }
            if ( res ) { res = f_circular_column -> write_u8( 0 ); }
            return res;
        }
};

}
