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
* ==============================================================================
*
*/

package gov.nih.nlm.ncbi.ngs.error;

import gov.nih.nlm.ncbi.ngs.error.cause.LibraryLoadCause;

public class LibraryLoadError extends ExceptionInInitializerError {
    String libName;
    LibraryLoadCause cause;
    String errorMessage;
    String recommendation;

    public LibraryLoadError(String libName, String msg, LibraryLoadCause cause) {
        this(libName, msg, cause, cause.getRecommendation());
    }

    public LibraryLoadError(String libName, String msg, LibraryLoadCause cause, String recommendation) {
        super(generateMsg(libName, msg, cause, recommendation));

        this.libName = libName;
        this.cause = cause;
        this.errorMessage = generateMsg(libName, msg, cause, null);
        this.recommendation = recommendation;
    }

    @Override
    public LibraryLoadCause getCause() {
        return cause;
    }

    /**
     * @return error message, without recommendation
     */
    public String getErrorMessage() {
        return errorMessage;
    }

    /**
     * @return recommendation for the user regarding error, can be null
     */
    public String getRecommendation() {
        return recommendation;
    }

    private static String generateMsg(String libName, String msg, LibraryLoadCause cause, String recommendation) {
        String result = "Failed to load '" + libName + "' - " + msg;
        if (cause != null) {
            result += ", " + cause.getMessage();
            if (recommendation != null) {
                result += "\n" + recommendation;
            }
        }

        return result;
    }
}
