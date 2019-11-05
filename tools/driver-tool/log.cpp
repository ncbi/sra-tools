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

#include "logging.hpp"

namespace ncbi
{

    void Log :: resetProcessId ( pid_t pid )
    {
        if ( logger != 0 )
            logger -> resetProcessId ( pid );
    }

    Fmt & Log :: msg ( int _priority )
    {
#if MAC
        const clockid_t CLOCK_ID = CLOCK_REALTIME;
#else
        const clockid_t CLOCK_ID = CLOCK_REALTIME_COARSE;
#endif
        // capture timestamp
        int status = clock_gettime ( CLOCK_ID, & timestamp );
        if ( status != 0 )
        {
            // probably wouldn't work either
            timestamp . tv_sec = time ( 0 );
            timestamp . tv_nsec = 0;
        }

        // capture priority
        priority = _priority;

        // start message
        return fmt;
    }

    void Log :: write ( const char * data, size_t bytes )
    {
        if ( logger != 0 )
            logger -> msg ( priority, timestamp, data, bytes );
    }


    // connect to another Logger
    // used by master process
    void Log :: use ( Logger & _logger )
    {
        logger = & _logger;
    }

    Log :: Log ()
        : fmt ( * this )
        , logger ( 0 )
        , priority ( 0 )
    {
        timestamp . tv_sec = 0;
        timestamp . tv_nsec = 0;
    }

    Log :: Log ( Logger & _logger )
        : fmt ( * this )
        , logger ( & _logger )
        , priority ( 0 )
    {
        timestamp . tv_sec = 0;
        timestamp . tv_nsec = 0;
    }

    Log :: ~ Log ()
    {
        logger = 0;
        timestamp . tv_sec = 0;
        timestamp . tv_nsec = 0;
        priority = 0;
    }

}
