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

#include "test_engine.hpp"

#include "ReadCollectionItf.hpp"
#include "ReadGroupItf.hpp"
#include "ReferenceItf.hpp"
#include "AlignmentItf.hpp"
#include "ReadItf.hpp"
#include "StatisticsItf.hpp"
#include "PileupItf.hpp"

  unsigned int ngs_test_engine::ReadCollectionItf::instanceCount = 0;
  unsigned int ngs_test_engine::ReadGroupItf::instanceCount = 0;
  unsigned int ngs_test_engine::ReferenceItf::instanceCount = 0;
  unsigned int ngs_test_engine::AlignmentItf::instanceCount = 0;
  unsigned int ngs_test_engine::ReadItf::instanceCount = 0;
  unsigned int ngs_test_engine::StatisticsItf::instanceCount = 0;
  unsigned int ngs_test_engine::PileupItf::instanceCount = 0;

namespace ngs_test_engine
{

	  ngs::ReadCollection NGS::openReadCollection ( const String & spec ) NGS_THROWS ( ErrorMsg )
	{
        ngs_adapt::ReadCollectionItf * ad_itf = new ngs_test_engine::ReadCollectionItf ( spec . c_str () );
        NGS_ReadCollection_v1 * c_obj = ad_itf -> Cast ();
        ngs::ReadCollectionItf * ngs_itf = ngs::ReadCollectionItf::Cast ( c_obj );
		return ngs::ReadCollection ( ngs_itf );
	}

}
