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

#ifndef _hpp_ngs_adapt_err_block_
#define _hpp_ngs_adapt_err_block_

#ifndef _hpp_ngs_adapt_error_msg_
#include <ngs/adapter/ErrorMsg.hpp>
#endif

#ifndef _h_ngs_itf_err_block_
#include <ngs/itf/ErrBlock.h>
#endif

namespace ngs_adapt
{

    /* Throw
     *  capture error state from a C++ exception
     *
     *  NB - this doesn't actually "throw" anything, but rather
     *  sets state within "self" such that an exception is thrown
     *  upon return from dispatch
     */
    void ErrBlockThrow ( NGS_ErrBlock_v1 * self, uint32_t type, const ErrorMsg & x );
    void ErrBlockThrow ( NGS_ErrBlock_v1 * self, uint32_t type, const :: std :: exception & x );

    /* ThrowUnknown
     *  capture error state from an unknown C++ exception
     *
     *  NB - this doesn't actually "throw" anything, but rather
     *  sets state within "self" such that an exception is thrown
     *  upon return from dispatch
     */
    void ErrBlockThrowUnknown ( NGS_ErrBlock_v1 * self );


    /* ErrBlockExceptionHandler
     *  fill out ErrBlock in the case of any exception has occured
     *
     *  NB - this function is only to be called from within another
     *  (outer) catch-block (usually - from "catch (...)"), i.e.
     *  it handles an exception that has been already thrown by the
     *  caller
     */
    void ErrBlockHandleException ( NGS_ErrBlock_v1 * err );

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_err_block_
