/*===========================================================================
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
* ===========================================================================
*
*/

#ifndef _hpp_ngs_itf_err_block_
#define _hpp_ngs_itf_err_block_

#ifndef _hp_ngs_itf_error_msg_
#include <ngs/itf/ErrorMsg.hpp>
#endif

#ifndef _h_ngs_itf_err_block_
#include <ngs/itf/ErrBlock.h>
#endif

namespace ngs
{

    /*----------------------------------------------------------------------
     * ErrBlock
     *  holds a message describing what happened
     */
    struct   ErrBlock : public :: NGS_ErrBlock_v1
    {
        void Throw () const
            NGS_THROWS ( ErrorMsg );

        inline
        void Check () const
            NGS_THROWS ( ErrorMsg )
        {
            if ( xtype != xt_okay )
                Throw ();
        }

        ErrBlock ()
        {
            xtype = xt_okay;
            msg [ 0 ] = 0;
        }
    };

} // namespace ngs

#endif // _hpp_ngs_itf_err_block_
