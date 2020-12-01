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
* ==============================================================================
*
* CSVM Orders
*/

#include <cloud/manager.h> /* CloudProviderId */
#include <klib/container.h> /* SLList */
#include <kfs/directory.h> /* KDirectory */

struct CSVMOrder;

/* Order status */
typedef enum {
    eSubmitted, /* order was submitted but not completed yet */
    eCompleted, /* order was submitted and completed */
    eRemoved,   /* order was submitted, completed and removed from order list,
                   not shown when printing stats, kept to request quotas */
} EStatus;

/* List of submitted CSVM Orders */
typedef struct {
    SLList list;
    bool dirty; /* needs to be saved */
} CSVMOrders;

/* Read content of file into [allocated] buffer [with returned size] */
rc_t KDirectory_ReadFile(const KDirectory * self,
    const char * path, char ** buffer, uint64_t * size);

/* Load Saved CSVM Orders file */
rc_t CSVMOrdersLoad(CSVMOrders * self,
    const KDirectory * dir, const char * path);

/* Add a new Submitted Order to CSVM Orders List */
rc_t CSVMOrdersAdd(CSVMOrders * self, CloudProviderId provider,
    const char * email, const char * workorder, int * number);

/* Release CSVMOrders object */
rc_t CSVMOrdersFini(CSVMOrders * self, KDirectory * dir, const char * path);

/* Get Data of CSVMOrder */
void CSVMOrderData(const struct CSVMOrder * self, EStatus * status,
    CloudProviderId * provider, const char ** email, const char ** workorder,
    int * number);

/* Update Order Status */
bool CSVMOrderUpdate(struct CSVMOrder * self, const char * status,
    int * number);
