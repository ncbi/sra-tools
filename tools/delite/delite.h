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

#ifndef _delite_h_
#define _delite_h_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

struct KConfig;

    /*  Very important structure, and most importatn method
     */
struct DeLiteParams {
    const char * _program;          /* Program name */
    const char * _accession;        /* Accession */
    const char * _accession_path;   /* Resolved accession path */
    const char * _output;           /* Output file path */
    const char * _schema;           /* Shema dirs path separated by : */
    const char * _transf;           /* File with list of transforms */
    bool _noedit;                   /* Just produce output, no edit */
    bool _update;                   /* Update schemas for tables */
    bool _delite;                   /* Delete quality scores */
    bool _output_stdout;            /* Output is stdout */
    bool _force_write;              /* If we need to force write */

    struct KConfig * _config;       /* Good config */
};

LIB_EXPORT rc_t Delite ( struct DeLiteParams * Params );
LIB_EXPORT rc_t Checkite ( const char * PathToArchive );

    /*  length limit for strings I am working with
     */
#define KAR_MAX_ACCEPTED_STRING 16384

LIB_EXPORT rc_t CC copyStringSayNothingRelax (
                                            const char ** Dst,
                                            const char * Src
                                            );

LIB_EXPORT rc_t CC copySStringSayNothing (
                                            const char ** Dst,
                                            const char * Begin,
                                            const char * End
                                            );

LIB_EXPORT rc_t CC copyLStringSayNothing (
                                            const char ** Dst,
                                            const char * Str,
                                            size_t Len
                                            );

LIB_EXPORT rc_t CC DeLiteResolvePath (
                                            char ** ResolvedLocalPath,
                                            const char * PathOrAccession
                                            );

    /*  Remember, ColumnName should be 'o' terminated
     */
LIB_EXPORT bool CC IsQualityName ( const char * Name );

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Time, more and more
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
LIB_EXPORT rc_t CC NowAsString ( char ** Str );
LIB_EXPORT rc_t CC NowToString ( char * B, size_t BSize );

LIB_EXPORT rc_t CC VersAsString ( char ** Str );
LIB_EXPORT rc_t CC VersToString ( char * B, size_t BSize );

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Some weird stuff, we need to do eventually. Very simple buffer.
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karCBuf {
    void * _b;  /* bufer */
    size_t _s;  /* bufer size */
    size_t _c;  /* bufer capacity */
};

/*  Two methods need to uses on preallocated structure itself
 */
    /*  If Reserve == 0, then it will be reserved by random size :lol:
     */
rc_t CC karCBufInit ( struct karCBuf * self, size_t Reserve );
rc_t CC karCBufWhack ( struct karCBuf * self );

/*  Two methods need to uses when You need allocate structure
 */
    /*  If Reserve == 0, then it will be reserved by random size :lol:
     */
rc_t CC karCBufMake ( struct karCBuf ** Buf, size_t Reserve );
rc_t CC karCBufDispose ( struct karCBuf * self );


    /*  Copy data to buffer to some certain position
     */
rc_t CC karCBufSet (
                    struct karCBuf * self,
                    size_t Offset,
                    void * Data,
                    size_t DSize
                    );

    /*  Appends data to buffer
     */
rc_t CC karCBufAppend (
                    struct karCBuf * self,
                    void * Data,
                    size_t DSize
                    );

    /*  This one will return inner buffer to caller, and it is 
     *  responcibility of caller not ot damage it :LOL:
     */
rc_t CC karCBufGet (
                    struct karCBuf * self,
                    const void ** Bufer,
                    size_t * BSize
                    );

    /*  This one will safely detach inner bufer and return to caller
     */
rc_t CC karCBufDetatch (
                    struct karCBuf * self,
                    const void ** Bufer,
                    size_t * BSize
                    );

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Another weird stuff. Line reader
 *  May be later I will attach AddRef/Release, now it is not needed 
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karLnRd;

rc_t CC karLnRdOpen (
                    const struct karLnRd ** LineReader,
                    const char * Path
                    );
    /*  Dat is kinda dangerous, cuz it just keep addr and size, not copy
     *  so, there is no control if somebody else will free these :LOL:
     */
rc_t CC karLnRdMake (
                    const struct karLnRd ** LineReader,
                    const void * Buf,
                    size_t BufSz
                    );
rc_t CC karLnRdDispose ( const struct karLnRd * self );

bool CC karLnRdIsGood ( const struct karLnRd * self );
bool CC karLnRdNext ( const struct karLnRd * self );
    /*  That method allocate new string each time, so
     *  feel free to delete it
     */
rc_t CC karLnRdGet ( const struct karLnRd * self, const char ** Line );
rc_t CC karLnRdGetNo ( const struct karLnRd * self, size_t * LineNo );
rc_t CC rarLnRdRewind ( const struct karLnRd * self );

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _delite_h_ */
