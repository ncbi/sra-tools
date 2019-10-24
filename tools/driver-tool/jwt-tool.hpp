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

#pragma once

#include <ncbi/secure/except.hpp>
#include <ncbi/secure/string.hpp>
#include <ncbi/json.hpp>
#include <ncbi/jwk.hpp>
#include <ncbi/jwt.hpp>

#include "logging.hpp"

#include <vector>

namespace ncbi
{
    enum JWTMode
    {
        decode = 1,
        sign, 
        examine,
        import_pem,
        gen_key
    };
    
    struct ParamBlock
    {
        void getInputParams ();
        void validate ( JWTMode mode );
        void validatePolicySettings ();
        
        ParamBlock ();
        
        ~ ParamBlock ()
            {
            }
        
        std :: vector <String> keyType;
        std :: vector <String> keyCurve;
        std :: vector <String> keyUse;
        std :: vector <String> keyAlg;
        std :: vector <String> keyKids;

        std :: vector <String> inputParams;
        std :: vector <String> pubKeyFilePaths;
        std :: vector <String> privKeyFilePaths;

        std :: vector <String> jwsPolicySettings;
        std :: vector <String> jwtPolicySettings;

        
        I64 duration;

        U32 numDurationOpts;
		U32 numPubKeyFilePaths;
        U32 numPrivKeyFilePaths;
        U32 numPwds;

        String privPwd;
        String alg;
    };
    
    class JWTTool
    {
    public:
        JWTTool ( const ParamBlock & params, JWTMode mode );
        ~ JWTTool () noexcept;
        
        
        void run ();

    private:
        void init ();
        void exec ();
        void cleanup () noexcept;

		void loadPublicKey ( const JWKRef & key );
		void loadPrivateKey ( const JWKRef & key );
		void loadKeyorKeySet ( const String & path );
		void importPemFile ( const String & path, const String & kid );

        void createJWT ( const String & json );
        void decodeJWT ( const JWT & jwt );
        void examineJWT ( const JWT & jwt );
        
        std :: vector <String> keyType;
        std :: vector <String> keyCurve;
        std :: vector <String> keyUse;
        std :: vector <String> keyAlg;
        std :: vector <String> keyKids;

        std :: vector <String> inputParams;
        std :: vector <String> pubKeyFilePaths;
        std :: vector <String> privKeyFilePaths;
        String privPwd;
		
        long long int duration;

        JWTMode jwtMode;
        JWKSetRef pubKeys;
        JWKSetRef privKeys;
    };
}
