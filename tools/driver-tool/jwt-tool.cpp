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

#include "cmdline.hpp"
#include "logging.hpp"

#include <iostream>

namespace ncbi
{
    static LocalLogger local_logger;
    Log log ( local_logger );

    void ParamBlock :: getInputParams ()
    {
        if ( inputParams . empty () )
        {
            // take from stdin
            log . msg ( LOG_INFO )
                << "No input params. Reading from stdin "
                << endm
                ;

            std :: string line;
            getline ( std :: cin, line );

            if ( line . empty () )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Missing input parameter from stdin"
                    );

            inputParams . emplace_back ( String ( line ) );            
        }
    }

    void ParamBlock :: validatePolicySettings ()
    {
        if ( ! jwsPolicySettings . empty () )
        {
            JWSMgr :: Policy jwsPolicy = JWSMgr :: getPolicy ();

            for ( auto policy_str : jwsPolicySettings )
            {

                count_t pos = policy_str . find ( '=' );
                if ( pos == String :: npos )
                    throw InvalidArgument (
                        XP ( XLOC, rc_param_err )
                        << "Faulty policy parameters"
                        );

                String policy_name = policy_str . subString ( 0, pos );
                String policy_val = policy_str . subString ( pos + 1, policy_str . count () - 1 );

                bool toggle = policy_val . equal ( "true" ) ? true : false;

                if ( policy_name . equal ( "kid_required" ) )
                    jwsPolicy . kid_required = toggle;
                
                else if ( policy_name . equal ( "kid_gen" ) )
                    jwsPolicy . kid_gen = toggle;
                
                else if ( policy_name . equal ( "require_simple_hdr" ) )
                    jwsPolicy . require_simple_hdr = toggle;
                
                else if ( policy_name . equal ( "require_prestored_kid" ) )
                    jwsPolicy . require_prestored_kid = toggle;
                
                else if ( policy_name . equal ( "require_prestored_key" ) )
                    jwsPolicy . require_prestored_key = toggle;

                else
                {
                    throw InvalidArgument (
                        XP ( XLOC, rc_param_err )
                        << "Invalid JWT policy: "
                        << policy_name
                        );
                }
                
            log . msg ( LOG_INFO )
                << "Set JWS policy: "
                << policy_name
                << " to "
                << policy_val
                << endm
                ;
                
            }

            JWSMgr :: setPolicy ( jwsPolicy );
        }

        if ( ! jwtPolicySettings . empty () )
        {
            JWTMgr :: Policy jwtPolicy = JWTMgr :: getPolicy ();

            for ( auto policy_str : jwtPolicySettings )
            {
                count_t pos = policy_str . find ( '=' );
                if ( pos == String :: npos )
                    throw InvalidArgument (
                        XP ( XLOC, rc_param_err )
                        << "Faulty policy parameters"
                        );

                String policy_name = policy_str . subString ( 0, pos );
                String policy_val = policy_str . subString ( pos + 1, policy_str . count () - 1 );


                if ( policy_name . equal ( "skew_seconds" ) )
                    jwtPolicy . skew_seconds = decToLongLongInteger ( policy_val );

                else if ( policy_name . equal ( "sig_val_required" ) )
                    jwtPolicy . sig_val_required = policy_val . equal ( "true" ) ? true : false;
                
                else if ( policy_name . equal ( "nested_sig_val_required" ) )
                    jwtPolicy . nested_sig_val_required = policy_val . equal ( "true" ) ? true : false;
                
                else if ( policy_name . equal ( "iss_required" ) )
                    jwtPolicy . iss_required = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "sub_required" ) )
                    jwtPolicy . sub_required = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "aud_required" ) )
                    jwtPolicy . aud_required = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "exp_required" ) )
                    jwtPolicy . exp_required = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "nbf_required" ) )
                    jwtPolicy . nbf_required = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "iat_required" ) )
                    jwtPolicy . iat_required = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "jti_required" ) )
                    jwtPolicy . jti_required = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "exp_gen" ) )
                    jwtPolicy . exp_gen = policy_val . equal ( "true" ) ? true : false;
                
                else if ( policy_name . equal ( "nbf_gen" ) )
                    jwtPolicy . nbf_gen = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "iat_gen" ) )
                    jwtPolicy . iat_gen = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "jti_gen" ) )
                    jwtPolicy . jti_gen = policy_val . equal ( "true" ) ? true : false;
                
                else if ( policy_name . equal ( "pre_serial_verify" ) )
                    jwtPolicy . pre_serial_verify = policy_val . equal ( "true" ) ? true : false;

                else if ( policy_name . equal ( "zero_dur_allowed" ) )
                    jwtPolicy . zero_dur_allowed = policy_val . equal ( "true" ) ? true : false;

                else
                {
                    throw InvalidArgument (
                        XP ( XLOC, rc_param_err )
                        << "Invalid JWT policy: "
                        << policy_name
                        );
                }
                
                log . msg ( LOG_INFO )
                << "Set JWT policy: "
                << policy_name
                << " to "
                << policy_val
                << endm
                ;
                
            }

            JWTMgr :: setPolicy ( jwtPolicy );
        }
    }
    
    void ParamBlock :: validate ( JWTMode mode )
    {
		if ( numDurationOpts > 1 )
			throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "Multiple duration values"
                );
		
		if ( numPwds > 1 )
			throw InvalidArgument (
                XP ( XLOC, rc_param_err )
                << "Multiple password values"
                );        

        switch ( mode )
        {
        case decode:
        {
            getInputParams ();
            
            if ( pubKeyFilePaths . empty () )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Required public key set"
                    );
            break;
        }
        case sign:
        {
            getInputParams ();

            if ( privKeyFilePaths . empty () )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Missing required private signing key"
                    );
            if ( privKeyFilePaths . size () > 1 )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Multiple private key paths"
                    );
            if ( duration < 0 )
            {
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Required a duration of 0 or more (seconds)"
                    );
            }
            break;
        }
        case examine:
        {
            getInputParams ();
            
            if ( pubKeyFilePaths . empty () )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Required public key set"
                    );
            break;
        }
        case import_pem:
        {          
            getInputParams ();

            if ( privPwd . isEmpty () )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Missing pem file password"
                    );            
            if ( privKeyFilePaths . size () > 1 )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Multiple private key paths"
                    );
            if ( pubKeyFilePaths . size () > 1 )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Multiple private key paths"
                    );
            
            break;
        }
        case gen_key:
        {
            if ( keyType . empty () )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Missing key type"
                    );
            if ( keyType . size () > 1 )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Multiple key type"
                    );
            if ( keyType . at ( 0 ) == "EC" )
            {
                if ( keyCurve . empty () )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Missing key curve"
                    );
                if ( keyCurve . size () > 1 )
                    throw InvalidArgument (
                        XP ( XLOC, rc_param_err )
                        << "Multiple key curves"
                        );
            }
            if ( keyUse . empty () )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Missing key use"
                    );
            if ( keyUse . size () > 1 )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Multiple key use"
                    );
            if ( keyAlg . empty () )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Missing key alg"
                    );
            if ( keyAlg . size () > 1 )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Missing key alg"
                    );
            if ( privKeyFilePaths . size () > 1 )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Multiple private key paths"
                    );
            if ( pubKeyFilePaths . size () > 1 )
                throw InvalidArgument (
                    XP ( XLOC, rc_param_err )
                    << "Multiple private key paths"
                    );
            break;
        }
        }
    }

    ParamBlock :: ParamBlock ()
        : duration ( 5 * 60 )
        , numDurationOpts ( 0 )
	    , numPubKeyFilePaths ( 0 )
	    , numPrivKeyFilePaths ( 0 )
        , numPwds ( 0 )
    {
    }

    JWTTool :: JWTTool ( const ParamBlock & params, JWTMode mode )
        : keyType ( params . keyType )
        , keyCurve ( params . keyCurve )
        , keyUse ( params . keyUse )
        , keyAlg ( params . keyAlg )
        , keyKids ( params . keyKids )
        , inputParams ( params . inputParams )
        , pubKeyFilePaths ( params . pubKeyFilePaths )
        , privKeyFilePaths ( params . privKeyFilePaths )
        , privPwd ( params . privPwd )
        , duration ( params . duration )
        , jwtMode ( mode )
    {
    }
    
    JWTTool :: ~ JWTTool () noexcept
    {
    }

    void JWTTool :: run ()
    {
        // initialize and let any exceptions pass out
        init ();

        // we've been initilized
        try
        {
            exec ();
        }
        catch ( ... )
        {
            cleanup ();
            throw;
        }

        cleanup ();
    }

    static
    JWTMode handle_params ( ParamBlock & params, int argc, char * argv [] )
    {
        Cmdline cmdline ( argc, argv );
        
        cmdline . addMode ( "decode", "Extract and verify JWT claims" );
        cmdline . addMode ( "sign", "Sign a JSON claim set object" );
        cmdline . addMode ( "examine", "Examine a JWT without verification" );
        cmdline . addMode ( "import-pem", "Import a private pem file to extract and save private and public keys to files" );
        cmdline . addMode ( "gen-key", "Generate an RSA or Eliptic Curve key and save private and public keys to files" );
        
        // to the cmdline parser, all params are optional
        // we will enforce their presence manually

        // decode
        cmdline . setCurrentMode ( "decode" );
        cmdline . startOptionalParams ();
        cmdline . addParam ( params . inputParams, 0, 256, "token(s)", "optional list of tokens to process" );
        cmdline . addListOption ( params . pubKeyFilePaths, ',', 256,
                                  "", "pub-key", "", "provide one or more public JWK or JWKSets" );
        cmdline . addListOption ( params . jwsPolicySettings, ',', 256,
								 "", "jws-policy", "",
                                  "Overrite default policy settings for JWS" );
        cmdline . addListOption ( params . jwtPolicySettings, ',', 256,
								 "", "jwt-policy", "",
                                  "Overrite default policy settings for JWT" );

        // sign
        cmdline . setCurrentMode ( "sign" );
        cmdline . startOptionalParams ();
        cmdline . addParam ( params . inputParams, 0, 256, "JSON text", "JSON text to convert into a JWT" );
        cmdline . addOption ( params . duration, & params . numDurationOpts,
							 "", "duration", "" ,"amount of time JWT is valid" );
        cmdline . addListOption ( params . privKeyFilePaths, ',', 256,
								 "", "priv-key", "", "Private signing key; provide only 1" );
        cmdline . addListOption ( params . jwsPolicySettings, ',', 256,
								 "", "jws-policy", "",
                                  "Overrite default policy settings for JWS" );
        cmdline . addListOption ( params . jwtPolicySettings, ',', 256,
								 "", "jwt-policy", "",
                                  "Overrite default policy settings for JWT" );

        // examine
        cmdline . setCurrentMode ( "examine" );
        cmdline . startOptionalParams ();
        cmdline . addParam ( params . inputParams, 0, 256, "token(s)", "optional list of tokens to process" );
        
        cmdline . addListOption ( params . pubKeyFilePaths, ',', 256,
                                  "", "pub-key", "", "provide one or more public JWK or JWKSets" );
        cmdline . addListOption ( params . jwsPolicySettings, ',', 256,
								 "", "jws-policy", "",
                                  "Overrite default policy settings for JWS" );
        cmdline . addListOption ( params . jwtPolicySettings, ',', 256,
								 "", "jwt-policy", "",
                                  "Overrite default policy settings for JWT" );

        // import-pem
        cmdline . setCurrentMode ( "import-pem" );
        cmdline . startOptionalParams ();
        cmdline . addParam ( params . inputParams, 0, 256, "pem file(s)", "one or more pem files" );
        cmdline . addOption ( params . privPwd, & params . numPwds,
							 "", "pwd", "" , "Private pem file password for decryption" );
        cmdline . addListOption ( params . keyKids, ',', 256,
								 "", "kid", "kid(s) ",
								 "One kid per pem file. kids will auto-gen if not found. Ignored if in excess of pem files" );
        cmdline . addListOption ( params . privKeyFilePaths, ',', 256,
								 "", "priv-key", "",
								 "Write to private key file; default location if unspecified; will overrite" );
        cmdline . addListOption ( params . pubKeyFilePaths, ',', 256,
								 "", "pub-key", "",
                                  "Write to public key file; default location if unspecified; will overrite" );
        /*
        cmdline . addListOption ( params . jwsPolicySettings, ',', 256,
								 "", "jws-policy", "",
                                  "Overrite default policy settings for JWS" );
        cmdline . addListOption ( params . jwtPolicySettings, ',', 256,
								 "", "jwt-policy", "",
                                  "Overrite default policy settings for JWT" );
        */

        // gen-key
        cmdline . setCurrentMode ( "gen-key" );
        cmdline . startOptionalParams ();
        cmdline . addListOption ( params . keyType, ',', 256,
								 "", "type", "key-type", "Req. RSA or EC" );
        cmdline . addListOption ( params . keyCurve, ',', 256,
								 "", "curve", "ec-curve", "Req if EC. Elipctic curve name" );
        cmdline . addListOption ( params . keyUse, ',', 256,
								 "", "use", "key-use", "Req. sig or enc" );
        cmdline . addListOption ( params . keyAlg, ',', 256,
								 "", "alg", "algorithm", "Req. <RSA256, ES256...>" );
        cmdline . addListOption ( params . keyKids, ',', 256,
								 "", "kid", "kid", "kid will auto-gen if not found." );
        cmdline . addListOption ( params . privKeyFilePaths, ',', 256,
								 "", "priv-key", "",
								 "Write to private key file; default location if unspecified; will overrite" );
        cmdline . addListOption ( params . pubKeyFilePaths, ',', 256,
								 "", "pub-key", "",
                                  "Write to public key file; default location if unspecified; will overrite" );


        
        // pre-parse to look for any configuration file path
        cmdline . parse ( true );
        
        // configure params from file
        
        // normal parse
        cmdline . parse ();

        String ignore;
        JWTMode mode = static_cast <JWTMode> ( cmdline . getModeInfo ( ignore ) );
        params . validate ( mode );
        
        return mode;
    }

    static
    int run ( int argc, char * argv [] )
    {
        try
        {
            // enable local logging to stderr
            local_logger . init ( argv [ 0 ] );

            // create params
            ParamBlock params;
            JWTMode mode = handle_params ( params, argc, argv );

            // run the task object
            JWTTool jwt_tool ( params, mode );
            jwt_tool . run ();

            log . msg ( LOG_INFO )
                << "exiting. "
                << endm
                ;
        }
        catch ( Exception & x )
        {
            log . msg ( LOG_ERR )
                << "EXIT: exception - "
                << x . what ()
                << '\n'
			    << XBackTrace ( x )
                << endm
                ;
            return x . status ();
        }
        catch ( std :: exception & x )
        {
            log . msg ( LOG_ERR )
                << "EXIT: exception - "
                << x . what ()
                << endm
                ;
            throw;
        }
        catch ( ReturnCodes x )
        {
            log . msg ( LOG_NOTICE )
                << "EXIT: due to exception"
                << endm
                ;
            return x;
        }
        catch ( ... )
        {
            log . msg ( LOG_ERR )
                << "EXIT: unknown exception"
                << endm
                ;
            throw;
        }
        
        return 0;
    }
}

extern "C"
{
    int main ( int argc, char * argv [] )
    {
        int status = 1;

        /* STATE:
             fd 0 is probably open for read
             fd 1 is probably open for write
             fd 2 is hopefully open for write
             launched as child process of some shell,
             or perhaps directly as a child of init
        */

        try
        {
            // run the tool within namespace
            status = ncbi :: run ( argc, argv );
        }
#if 0
        catch ( ReturnCodes x )
        {
            status = x;
        }
#endif
        catch ( ... )
        {
        }
        
        return status;
    }
}
