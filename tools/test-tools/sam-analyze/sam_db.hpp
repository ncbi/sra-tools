#ifndef SAM_DB_H
#define SAM_DB_H

#include <memory>
#include "database.hpp"

struct SAM_SPOT {
    std::string NAME;
    uint16_t FLAGS;
    std::string RNAME;
    uint32_t RPOS;
    uint8_t MAPQ;
    std::string CIGAR;
    std::string MRNAME;
    uint32_t MRPOS;
    int32_t TLEN;
    std::string SEQ;
    std::string QUAL;
    std::string TAGS;
};

class SAM_DB : public mt_database::DB {
    private :
        int status;
        std::shared_ptr< mt_database::PREP_STM > ins_ref_stm;
        std::shared_ptr< mt_database::PREP_STM > ins_hdr_stm;
        std::shared_ptr< mt_database::PREP_STM > ins_alig_stm;
        
        SAM_DB( const SAM_DB& ) = delete;
        
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
        
        int16_t make_refcnt_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS REF_CNT( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "RNAME TEXT NOT NULL UNIQUE," \
                "CNT INT NOT NULL );";
            return exec( stm );
        }

        int16_t make_spots_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS SPOTS( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "NAME TEXT NOT NULL UNIQUE," \
                "CNT INT NOT NULL );";
            return exec( stm );
        }

        int16_t make_spot_mode_tbl( void ) {
            const char * stm = "CREATE TABLE IF NOT EXISTS SPOT_MODE( " \
                "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
                "NAME TEXT NOT NULL UNIQUE," \
                "CNT INT NOT NULL );";
            int16_t res = exec( stm );
            if ( ok_or_done( res ) ) {
                const char * stm = "CREATE TRIGGER IF NOT EXISTS after_spot_insert " \
                    "AFTER INSERT ON SPOTS " \
                    "BEGIN" \
                    " INSERT INTO SPOT_MODE ( NAME, CNT )" \
                    " VALUES ( " \
                    "     NEW.NAME, " \
                    "     ( SELECT COUNT( NAME ) FROM ALIG WHERE NAME = NEW.NAME AND RNAME = '*' ) " \
                    " );" \
                    "END;";
                res = exec( stm );
            }
            return res;
        }

        uint64_t select_number( const char * sql ) {
            uint64_t res = 0;
            if ( ok_or_done( status ) ) {
                mt_database::PREP_STM stm( get_db_handle(), sql );
                uint64_t tmp;
                status = stm . read_long( tmp );
                if ( ok_or_done( status ) ) { res = tmp; }
            }
            return res;
        }

    public :
        SAM_DB( const std::string& filename ) 
            : DB( filename.c_str() ), status( SQLITE_OK ) {
            status = make_ref_hdr_tbl();
            if ( ok_or_done( status ) ) { status = make_hdr_tbl(); }
            if ( ok_or_done( status ) ) { status = make_alig_tbl(); }
            if ( ok_or_done( status ) ) { make_refcnt_tbl(); }
            if ( ok_or_done( status ) ) { make_spots_tbl(); }
            if ( ok_or_done( status ) ) { make_spot_mode_tbl(); }
            if ( ok_or_done( status ) ) {
                ins_ref_stm = std::make_shared< mt_database::PREP_STM >(
                    get_db_handle(), 
                    "INSERT INTO REF ( NAME, LENGTH, OTHER ) VALUES( ?, ?, ? );" );
                status = ins_ref_stm -> get_status();
            }
            if ( ok_or_done( status ) ) {
                ins_hdr_stm = std::make_shared< mt_database::PREP_STM >(
                    get_db_handle(),
                    "INSERT INTO HDR ( VALUE ) VALUES( ? );" );
                status = ins_hdr_stm -> get_status();
            }
            if ( ok_or_done( status ) ) {
                ins_alig_stm = std::make_shared< mt_database::PREP_STM >(
                    get_db_handle(),
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
            return status; 
        }
        
        int16_t create_alig_tbl_idx( void ) {
            exec( "CREATE INDEX ALIG_RNAME_IDX on ALIG( RNAME );" );
            return exec( "CREATE INDEX ALIG_NAME_IDX on ALIG( NAME );" );            
        }

        int16_t create_refcnt_tbl_idx( void ) {
            return exec( "CREATE INDEX REF_CNT_RNAME_IDX on REF_CNT( RNAME );" );            
        }

        int16_t add_ref( const mt_database::PREP_STM::str_vec& data ) {
            if ( ok_or_done( status ) ) {
                status = ins_ref_stm -> bind_and_step( data );
            }
            return status;
        }
        
        int16_t add_hdr( const mt_database::PREP_STM::str_vec& data ) {
            if ( ok_or_done( status ) ) {
                status = ins_hdr_stm -> bind_and_step( data );
            }
            return status;
        }
        
        int16_t add_alig( const mt_database::PREP_STM::str_vec& data ) {
            if ( ok_or_done( status ) ) {
                status = ins_alig_stm -> bind_and_step( data );
            }
            return status;
        }
        
        int16_t populate_refcnt( void ) {
            if ( ok_or_done( status ) ) {
                const char * stm = "INSERT INTO REF_CNT( RNAME, CNT ) " \
                    "SELECT RNAME,COUNT(RNAME) AS RNCNT FROM ALIG " \
                    "GROUP BY RNAME ORDER BY RNCNT DESC;";
                status = exec( stm );
            }
            return status;
        }

        int16_t populate_spots( void ) {
            if ( ok_or_done( status ) ) {
                const char * stm = "INSERT INTO SPOTS( NAME, CNT ) " \
                    "SELECT NAME,COUNT(NAME) AS NCNT FROM ALIG " \
                    "GROUP BY NAME ORDER BY NCNT DESC;";
                status = exec( stm );
            }
            return status;
        }

        uint64_t spot_count( void ) {
            return select_number( "SELECT count(*) FROM SPOTS;" );
        }
        
        uint64_t refs_in_use_count( void ) {
            return select_number( "SELECT count(*) FROM REF_CNT WHERE RNAME != '*';" );
        }
        
        uint64_t alig_count( void ) {
            return select_number( "SELECT count(*) FROM ALIG;" );            
        }

        uint64_t count_unaligned( void ) {
            return select_number( "select count( NAME ) from SPOT_MODE where CNT > 1;" );
        }

        uint64_t count_fully_aligned( void ) {
            return select_number( "select count( NAME ) from SPOT_MODE where CNT = 0;" );
        }

        uint64_t count_half_aligned( void ) {
            return select_number( "select count( NAME ) from SPOT_MODE where CNT = 1;" );
        }
        
        int16_t exec_pragma( const std::string& value ) {
            if ( ok_or_done( status ) ) {
                std::string stm( "PRAGMA " );
                stm += value;
                stm += ";";
                status = exec( stm . c_str() );
            }
            return status;
        }
        
        std::shared_ptr< mt_database::PREP_STM > make_prep_stm( const std::string& stm ) {
            return std::make_shared< mt_database::PREP_STM >( get_db_handle(), stm );
        }
};

#endif
