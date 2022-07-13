/*==============================================================================
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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#include "jwt-tool.hpp"
#include <ncbi/jws.hpp>

#include <fstream>
#include <cassert>

namespace ncbi
{
    static
    String readTextFile ( const String & path )
    {
		log . msg ( LOG_INFO )
		<< "reading file:  '"
        << path
		<< endm
		;

        // declare String for return
        String contents;
        
        // open the file using the nul-terminated path string
       std :: ifstream file;
        file . open ( path . toSTLString () );

        // handle exceptions or file-not-found
        if ( ! file . is_open () )
            throw RuntimeException (
                XP ( XLOC, rc_runtime_err )
                << "Failed to open file"
                );

        // measure the size of the file
        file . seekg ( 0, file . end );
        size_t size = file . tellg ();
        file . seekg ( 0, file . beg );
        
        // allocate a char[] buffer 
        char * buf = new char [ size ];

        try
        {
            // read file into buffer within try block
            file . read ( buf, size );
            file . close ();
            
            contents = String ( buf, size );
        }
        catch ( ... )
        {
            delete [] buf;
            throw;
        }
        
        delete [] buf;

        return contents;
    }

    static
    void writeTextFile ( const String & text, const String & path )
    {
		log . msg ( LOG_INFO )
		<< "writing to file:  '"
        << path
		<< endm
		;

        std :: ofstream file;
        file . open ( path . toSTLString () );

        if ( ! file . is_open () )
            throw RuntimeException (
                XP ( XLOC, rc_runtime_err )
                << "Failed to open/create file"
                );

        try
        {
            file . write ( text . toSTLString () . c_str (), text . length () );

            file . close ();
        }
        catch ( ... )
        {
            throw;
        }

        return;    
    }

	void JWTTool :: loadPublicKey ( const JWKRef & key )
	{
		log . msg ( LOG_INFO )
		<< "Loading pub key to JWKS  '"
		<< endm
		;
		
		if ( pubKeys == nullptr )
			pubKeys = JWKMgr :: makeJWKSet ();
		
		pubKeys -> addKey ( key );
	}

	void JWTTool :: loadPrivateKey ( const JWKRef & key )
	{
		log . msg ( LOG_INFO )
		<< "Loading priv key to JWKS  '"
		<< endm
		;
		
		if ( privKeys == nullptr )
			privKeys = JWKMgr :: makeJWKSet ();
		
		privKeys -> addKey ( key );
	}
	

	void JWTTool :: loadKeyorKeySet ( const String & path )
	{
		log . msg ( LOG_INFO )
		<< "Attempting to load key sets from '"
		<< path
		<< '\''
		<< endm
		;
		
		// capture String with contents of file
		String contents = readTextFile ( path );
		
		log . msg ( LOG_INFO )
		<< "Parsing keyset JSON"
		<< endm
		;
		
		JWKSetRef key_set = JWKMgr :: parseJWKorJWKSet ( contents );
		
		// otherwise, merge in the keys
		std :: vector <String> kids = key_set -> getKeyIDs ();
		
		for ( auto kid : kids )
		{
			JWKRef key = key_set -> getKey ( kid );
			if ( key -> isPrivate () )
				loadPrivateKey ( key );
			else
				loadPublicKey ( key );
		}
	}
	
    void JWTTool :: importPemFile ( const String & path, const String & kid )
    {
        log . msg ( LOG_INFO )
            << "Reading private key from '"
            << path
            << '\''
            << endm
            ;

        // capture String with contents of file
        String contents = readTextFile ( path );
		
		log . msg ( LOG_INFO )
		<< "Parsing PEM '"
		<< endm
		;

        bool isRSA = false;
        bool isEC = false;
        JWKRef priv_key;
        if ( contents . find ( "BEGIN RSA PRIVATE KEY" ) != String :: npos )
        {
            priv_key = JWKMgr :: parsePEM ( contents, privPwd, "sig", "RS256", kid );
            isRSA = true;
        }
        else if ( contents . find ( "BEGIN EC PRIVATE KEY" ) != String :: npos )
        {
            priv_key = JWKMgr :: parsePEM ( contents, privPwd, "sig", "ES256", kid );
            isEC = true;
        }
        else
            throw RuntimeException (
                XP ( XLOC, rc_param_err )
                << "Invalid pem file"
                );

        loadPrivateKey ( priv_key );
        
		assert ( privKeys -> count () == 1 ); //shouldnt have more than one private key from pem file
		
		log . msg ( LOG_INFO )
		<< "Attempting to translate a private key to public '"
		<< endm
		;
		
        // translate private key to public
        JWKRef pub_key = privKeys -> getKey ( 0 ) -> toPublic ();
		
		loadPublicKey ( pub_key );
		
        String privOut;
        if ( privKeyFilePaths . empty () )
        {
            if ( isRSA )
                privOut = "tool/input/importRSAPrivKey.jwk";
            else if ( isEC )
                privOut = "tool/input/importECPrivKey.jwk";
        }
        else
            privOut = privKeyFilePaths . at ( 0 );
        
        String pubOut;
        if ( pubKeyFilePaths . empty () )
        {
            if ( isRSA )
                pubOut = "tool/input/importRSAPubKey.jwk";
            else if ( isEC )
                pubOut = "tool/input/importECPubKey.jwk";                    
        }
        else
            pubOut = pubKeyFilePaths . at ( 0 );
        
        // save keys to text files
        writeTextFile ( privKeys -> readableJSON (), privOut );
        writeTextFile ( privKeys -> readableJSON (), pubOut );
    }

    void JWTTool :: init ()
    {
        switch ( jwtMode )
        {
        case decode:
        {
            // load keysets in keyfilepaths into JWK obj
            log . msg ( LOG_INFO )
                << "Attempting to load "
                << pubKeyFilePaths . size ()
                << " keyset files"
                << endm
                ;
            
            for ( auto path : pubKeyFilePaths )
                loadKeyorKeySet ( path );
            
            log . msg ( LOG_INFO )
                << "Successfully loaded "
                << pubKeys -> count ()
                << " keys"
                << endm
                ;
            break;
        }
        case sign:
        {
            JWTMgr :: Policy jwtPolicy = JWTMgr :: getPolicy ();

            jwtPolicy . pre_serial_verify = false;
            // jwtPolicy . zero_dur_allowed = true;

            JWTMgr :: setPolicy ( jwtPolicy );
            
            log . msg ( LOG_INFO )
                << "Attempting to load "
                << 1
                << " pem file"
                << endm
                ;
            loadKeyorKeySet ( privKeyFilePaths [ 0 ] );
			
            log . msg ( LOG_INFO )
                << "Successfully loaded "
                << 1
                << " private key"
                << endm
                ;
            break;
        }
        case examine:
        {
            JWSMgr :: Policy jwsPolicy = JWSMgr :: getPolicy ();
            
            // allow ANY JOSE header
            jwsPolicy . kid_required = false;
            jwsPolicy . kid_gen = false;
            
            // unprotected JWS verification - turns off hardening
            jwsPolicy . require_simple_hdr = false;
            jwsPolicy . require_prestored_kid = false;
            jwsPolicy . require_prestored_key = false;
            
            JWSMgr :: setPolicy ( jwsPolicy );
            
            // load keysets in keyfilepaths into JWK obj
            log . msg ( LOG_INFO )
                << "Attempting to load "
                << pubKeyFilePaths . size ()
                << " keyset files"
                << endm
                ;
            
            for ( auto path : pubKeyFilePaths )
                loadKeyorKeySet ( path );
            
            log . msg ( LOG_INFO )
                << "Successfully loaded "
                << pubKeys -> count ()
                << " keys"
                << endm
                ;
            break;
        }
        case import_pem:
        {
            for ( count_t i = 0; i < inputParams . size (); ++ i )
            {
                String path = inputParams . at ( i );
                String kid;
                if ( i < keyKids . size () )
                    kid = keyKids . at ( i );
                else
                    kid = JWKMgr :: makeID ();

                importPemFile ( path, kid );
            }
            break;
        }
        case gen_key:
        {
            log . msg ( LOG_INFO )
                << "Attempting to generate a new private key '"
                << endm;

            
            JWKRef priv_key;
            if ( keyType . at ( 0 ) == "RSA" )
            {            
                priv_key = JWKMgr :: generateRSAKey ( keyUse . at ( 0 ), keyAlg . at ( 0 ),
                                                      keyKids . empty () ? JWKMgr :: makeID () : keyKids . at ( 0 ) );

            }
            else if ( keyType . at ( 0 ) == "EC" )
            {
                priv_key = JWKMgr :: generateECKey ( keyCurve . at ( 0 ), keyUse . at ( 0 ), keyAlg . at ( 0 ),
                                                     keyKids . empty () ? JWKMgr :: makeID () : keyKids . at ( 0 ) );
            }
            else
            {
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Invalid key type"
                    );            
            }
            
            log . msg ( LOG_INFO )
                << "Success generating a new private key '"
                << endm;
            
            loadPrivateKey ( priv_key );
            
            assert ( privKeys -> count () == 1 ); 
            
            log . msg ( LOG_INFO )
                << "Attempting to translate a private key to public '"
                << endm
                ;
            
            // translate private key to public
            JWKRef pub_key = privKeys -> getKey ( 0 ) -> toPublic ();
            
            loadPublicKey ( pub_key );
            
            String privOut;
            if ( privKeyFilePaths . empty () )
            {
                if ( keyType . at ( 0 ) == "RSA" )
                    privOut = "tool/input/generateRSAPrivKey.jwk";
                else
                    privOut = "tool/input/generateECPrivKey.jwk";
            }
            else
                privOut = privKeyFilePaths . at ( 0 );
            
            String pubOut;
            if ( pubKeyFilePaths . empty () )
            {
                if ( keyType . at ( 0 ) == "RSA" )
                    pubOut = "tool/input/generateRSAPubKey.jwk";
                else
                    pubOut = "tool/input/generateECPubKey.jwk";                    
            }
            else
                pubOut = pubKeyFilePaths . at ( 0 );

                // save keys to text files
            writeTextFile ( privKeys -> readableJSON (), privOut );
            writeTextFile ( privKeys -> readableJSON (), pubOut );
            break;
        }
        
        default:
            break;
        }
    }
}
