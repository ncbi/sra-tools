#ifndef IMPORTER_H
#define IMPORTER_H

#include "line_split.hpp"
#include "params.hpp"
#include "result.hpp"
#include "sam_db.hpp"

class IMPORTER {
    private :
        const IMPORT_PARAMS& params;
        IMPORT_RESULT& result;
        FILE_READER reader;
        STRING_PARTS line_parts;
        STRING_PARTS hdr_parts;
        SAM_DB &db;
        mt_database::PREP_STM::str_vec data;

        IMPORTER( const IMPORTER& ) = delete;
        
        static bool StartsWith( const std::string& s, const char * p ) {
            bool res = false;
            if ( !s.empty() ) {
                size_t at = s.find( p );
                if ( at != std::string::npos ) {
                    res = ( at == 0 );
                }
            }
            return res;
        }

        bool hdr_ref_line( void ) {
            bool res = false;
            std::string name, length, part;
            std::ostringstream ss;
            unsigned int ss_nr = 0;
            for ( int i = 1; i < line_parts . size(); ++i ) {
                part = line_parts . get( i );
                hdr_parts . split( part );
                if ( hdr_parts . size() > 0 ) {
                    if ( hdr_parts . get( 0 ) == "SN" ) {
                        name = hdr_parts . get( 1 );
                    } else if ( hdr_parts . get( 0 ) == "LN" ) {
                        length = hdr_parts . get( 1 );
                    } else {
                        if ( ss_nr == 0 ) {
                            ss << part;
                        } else {
                            ss << "\t" << part;
                        }
                        ss_nr++;
                    }
                }
            }
            if ( !name.empty() && !length.empty() ) {
                data . clear();
                data . push_back( name );
                data . push_back( length );
                data . push_back( ss . str() );
                int status = db . add_ref( data );
                res = db . ok_or_done( status );
            }
            return res;
        }

        bool header_line( const std::string& a_line ) {
            bool res = true;
            if ( ( result . header_lines % params . cmn . transaction_size ) == 0 ) {
                db . commit_transaction();
                db . begin_transaction();
                if ( params . cmn . progress ) { std::cerr << '.'; }
            }
            if ( StartsWith( a_line, "@SQ" ) ) {
                line_parts . split( a_line );
                res = hdr_ref_line();
            } else {
                data . clear();
                data . push_back( a_line );
                res = db . ok_or_done( db . add_hdr( data ) );
            }
            if ( res ) { result . header_lines += 1; }
            return res;
        }

        bool alignment_line( const std::string& a_line ) {
            if ( ( result . alignment_lines % params . cmn . transaction_size ) == 0 ) {
                db.commit_transaction();
                db.begin_transaction();
                if ( params . cmn . progress ) { std::cerr << '.'; }
            }
            line_parts . split( a_line );
            data . clear();
            // copy the fields QNAME ... QUAL ( 11 of them ) + collect TAGS into one
            std::string tags;
            for ( int i = 0; i < line_parts.size(); ++i ) {
                if ( i < 11 ) { 
                    data . push_back( line_parts . get( i ) );
                } else {
                    if ( tags . empty() ) {
                        tags = line_parts . get( i );
                    } else {
                        tags += " ";
                        tags += line_parts . get( i );
                    }
                }
            }
            data . push_back( tags );
            int status = db . add_alig( data );
            bool res = db . ok_or_done( status );
            if ( res ) { result . alignment_lines += 1; }
            return res;
        }

        bool synchronous( bool on ) {
            int status;
            if ( on ) {
                status = db . exec_pragma( "synchronous=ON" );
            } else {
                status = db . exec_pragma( "synchronous=OFF" );
            }
            return db . ok_or_done( status );
        }

        bool import_lines( void ) {
            int status = db . begin_transaction();
            bool ok = db . ok_or_done( status );
            bool has_align_limit = params . align_limit > 0;
            std::string line;
            while ( ok && reader . next( line ) ) {
                if ( StartsWith( line, "@" ) ) {
                    ok = header_line( line );
                } else {
                    if ( has_align_limit ) {
                        if ( result . alignment_lines >= params . align_limit ) break;
                    }
                    ok = alignment_line( line );
                }
            }
            if ( params . cmn . progress ) { std::cerr << std::endl; }
            if ( ok ) { ok = db . ok_or_done( db . commit_transaction() ); }
            return ok;
        }

    public :
        IMPORTER( SAM_DB& a_db, const IMPORT_PARAMS& a_params, IMPORT_RESULT& a_result )
            : params( a_params ),
              result( a_result ),
              reader( a_params . import_filename ),
              line_parts( '\t' ),
              hdr_parts( ':' ),
              db( a_db ) {
                  db . drop_all();
                }
              
        void run( void ) {
            bool ok = synchronous( false );
            
            if ( ok ) { ok = import_lines(); }

            // create the name-index on the ALIG table
            if ( ok ) { ok = db . ok_or_done( db.create_alig_tbl_idx() ); }
            // analyze the occurances of reference-names...
            if ( ok ) { ok = db . ok_or_done( db . populate_refcnt() ); }
            // create the name-index on the REF_CNT table
            if ( ok ) { ok = db . ok_or_done( db.create_refcnt_tbl_idx() ); }

            // analyze for spots 
            if ( ok ) { ok = db . ok_or_done( db . populate_spots() ); }

           
            result . total_lines = reader . get_line_nr();
            result . success =  ok;
        }
};

#endif
