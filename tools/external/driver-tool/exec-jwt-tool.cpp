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

#include <iostream>

namespace ncbi
{
    void JWTTool :: createJWT ( const String &json )
    {
        JWTClaimSetBuilderRef builder = JWTMgr :: parseClaimSetBuilder ( json );
        builder -> setDuration ( duration );
        
        JWTClaimSetRef claimSet = builder -> stealClaimSet ();
        log . msg ( LOG_INFO )
		<< "Duration set to"
        << claimSet -> getDuration ()
        << " seconds"
		<< endm
		;

        log . msg ( LOG_INFO )
		<< "Signing with key:\n "
        << privKeys -> getKey ( 0 ) -> readableJSON ()
        << "\n"
		<< endm
		;

        JWT jwt = JWTMgr :: sign ( *privKeys -> getKey ( 0 ), *claimSet );

        std :: cout << jwt << std :: endl;
    }
    
    void JWTTool :: decodeJWT ( const JWT & jwt )
    {
        log . msg ( LOG_INFO )
		<< "Decoding JWT'"
		<< endm
		;

        JWTClaimSetRef claimSet = JWTMgr :: decode ( * pubKeys, jwt );

        std :: cout << claimSet -> readableJSON () << std :: endl;
    }
    
    void JWTTool :: examineJWT ( const JWT & jwt )
    {
        log . msg ( LOG_INFO )
		<< "Examining unverified JWT'"
		<< endm
		;

        UnverifiedJWTClaimsRef claimSet = JWTMgr :: inspect ( * pubKeys, jwt );

        std :: cout << claimSet -> readableJSON () << std :: endl;
    }

    

    void JWTTool :: exec ()
    {
        switch ( jwtMode )
        {
        case decode:
        {
            for ( auto jwt : inputParams )
                decodeJWT ( jwt );
            break;
        }
        case sign:
        {
            for ( auto json : inputParams )
                createJWT ( json );
            break;
        }
        case examine:
        {
            for ( auto jwt : inputParams )
                examineJWT ( jwt );
            break;
        }
        default:
            break;
        }
    }
}
