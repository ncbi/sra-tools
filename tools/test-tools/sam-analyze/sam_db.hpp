#ifndef SAM_DB_H
#define SAM_DB_H

#include "database.hpp"

class sam_database_t : public mt_database::database_t {
    private :
        int status;
        mt_database::prep_stm_ptr_t ins_ref_stm;
        mt_database::prep_stm_ptr_t ins_hdr_stm;
        mt_database::prep_stm_ptr_t ins_alig_stm;
        
        sam_database_t( const sam_database_t& ) = delete;
        
        int16_t make_ref_hdr_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS REF( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "NAME TEXT NOT NULL UNIQUE," \
                "LENGTH INT NOT NULL, " \
                "OTHER TEXT NOT NULL );";
            return exec( stm );
        }

        int16_t make_hdr_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS HDR( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "VALUE TEXT NOT NULL );";
            return exec( stm );
        }

        int16_t make_alig_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS ALIG( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "NAME TEXT NOT NULL," \
                "FLAG INT NOT NULL," \
                "RNAME TEXT NOT NULL," \
                "RPOS INT NOT NULL," \
                "MAPQ INT NOT NULL," \
                "CIGAR TEXT NOT NULL," \
                "MRNAME TEXT NOT NULL," \
                "MRPOS INT NOT NULL," \
                "TLEN INT NOT NULL," \
                "SEQ TEXT NOT NULL," \
                "QUAL TEXT NOT NULL," \
                "TAGS TEXT NOT NULL );";
            return exec( stm );
        }
        
        uint64_t select_number( const char * sql ) {
            uint64_t res = 0;
            if ( ok_or_done( status ) ) {
                auto stm( make_prep_stm( sql ) );
                uint16_t st( stm -> get_status() );
                if ( SQLITE_OK == st ) { 
                    st = stm -> step();
                    if ( SQLITE_ROW == st ) { res = stm -> read_uint64_t(); }
                }
            }
            return res;
        }

    public :
        sam_database_t( const std::string& filename ) 
        : database_t( filename.c_str() ), status( SQLITE_OK ) {
            status = make_ref_hdr_tbl();
            if ( ok_or_done( status ) ) { status = make_hdr_tbl(); }
            if ( ok_or_done( status ) ) { status = make_alig_tbl(); }
            if ( ok_or_done( status ) ) {
                ins_ref_stm = make_prep_stm(
                    "INSERT INTO REF ( NAME, LENGTH, OTHER ) VALUES( ?, ?, ? );" );                    
                status = ins_ref_stm -> get_status();
            }
            if ( ok_or_done( status ) ) {
                ins_hdr_stm = make_prep_stm(
                    "INSERT INTO HDR ( VALUE ) VALUES( ? );" );
                status = ins_hdr_stm -> get_status();
            }
            if ( ok_or_done( status ) ) {
                ins_alig_stm = make_prep_stm(
                    "INSERT INTO ALIG "\
                    "( NAME, FLAG, RNAME, RPOS, MAPQ, CIGAR, MRNAME, MRPOS, TLEN, SEQ, QUAL, TAGS )"\
                    " VALUES( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? );" );
                status = ins_alig_stm -> get_status();
            }
        }
        
        int16_t drop_all( void ) {
            str_vec tables;
            tables . push_back( "REF" );
            tables . push_back( "HDR" );
            tables . push_back( "ALIG" );
            status = clear_tables( tables );
            if ( SQLITE_OK == status ) {
                status = exec( "DROP INDEX IF EXISTS ALIG_RNAME_IDX;" );
            }
            if ( SQLITE_OK == status ) {
                status = exec( "DROP INDEX IF EXISTS ALIG_NAME_IDX;" );
            }
            return status; 
        }
        
        int16_t create_alig_tbl_idx( void ) {
            uint16_t st = exec( "CREATE INDEX ALIG_RNAME_IDX on ALIG( RNAME );" );
            if ( SQLITE_OK == st ) {
                st = exec( "CREATE INDEX ALIG_NAME_IDX on ALIG( NAME );" );
            }
            return st;
        }

        int16_t add_ref( const mt_database::prep_stm_t::str_vec& data ) {
            if ( ok_or_done( status ) ) {
                status = ins_ref_stm -> bind_and_step( data );
            }
            return status;
        }
        
        int16_t add_hdr( const mt_database::prep_stm_t::str_vec& data ) {
            if ( ok_or_done( status ) ) {
                status = ins_hdr_stm -> bind_and_step( data );
            }
            return status;
        }
        
        int16_t add_alig( const mt_database::prep_stm_t::str_vec& data ) {
            if ( ok_or_done( status ) ) {
                status = ins_alig_stm -> bind_and_step( data );
            }
            return status;
        }

        uint64_t alig_count( void ) {
            return select_number( "SELECT count(*) FROM ALIG;" );            
        }
        
};

#endif
