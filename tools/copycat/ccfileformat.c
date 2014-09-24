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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <klib/rc.h>
#include <klib/debug.h>
#include <klib/log.h>
#include <kfs/file.h>
#include <kfs/fileformat.h>
#include <kfs/ffext.h>
#include <kfs/ffmagic.h>
#include <krypto/wgaencrypt.h>
#include <atomic32.h>
#include <stddef.h>
#include "copycat-priv.h"
#include "debug.h"

bool CCFileFormatIsNCBIEncrypted ( void  * buffer )
{
    static const char file_sig[] = "NCBInenc";

    return (memcmp (buffer, file_sig, sizeof file_sig - 1) == 0);
}
bool CCFileFormatIsKar ( void  * buffer )
{
    static const char file_sig[] = "NCBI.sra";

    return (memcmp (buffer, file_sig, sizeof file_sig - 1) == 0);
}




CCFileFormat *filefmt;

/* ======================================================================
 * Process does up the copy portion of the copycat tool's functionality
 * and sets up ProcessOne that does the Catalog portion.
 */
struct CCFileFormat
{
    KFileFormat * magic;
    KFileFormat * ext;
    atomic32_t	  refcount;
};

static const char magictable [] = 
{
    "Generic Format for Sequence Data (SRF)\tSequenceReadFormat\n"
    "GNU tar archive\tTapeArchive\n"
    "POSIX tar archive\tTapeArchive\n"
    "POSIX tar archive (GNU)\tTapeArchive\n"
    "Standard Flowgram Format (SFF)\tStandardFlowgramFormat\n"
    "NCBI kar sequence read archive\tSequenceReadArchive\n"
    "tar archive\tTapeArchive\n"
    "XML document text\tExtensibleMarkupLanguage\n"
    "bzip2 compressed data\tBzip\n"
    "Zip archive data\tWinZip\n"
    "gzip compressed data\tGnuZip\n"
};
static const char exttable [] = 
{
    "Unknown\tUnknown\n"
    "bam\tBinaryAlignmentMap\n"
    "bz2\tBzip\n"
    "gz\tGnuZip\n"
    "tgz\tGnuZip\n"
    "sff\tStandardFlowgramFormat\n"
    "sra\tSequenceReadArchive\n"
    "srf\tSequenceReadFormat\n"
    "tar\tTapeArchive\n"
    "xml\tExtensibleMarkupLanguage\n"
    "h5\tHD5\n"
};

static const char classtable [] = 
{
    "Archive\n"
    "Cached\n"
    "Compressed\n"
    "Read\n"
};
    
static const char formattable [] = 
{
    "BinaryAlignmentMap\tRead\n"
    "Bzip\tCompressed\n"
    "GnuZip\tCompressed\n"
    "WinZip\tRead\n"
    "ExtensibleMarkupLanguage\tCached\n"
    "SequenceReadFormat\tRead\n"
    "SequenceReadArchive\tArchive\n"
    "StandardFlowgramFormat\tRead\n"
    "TapeArchive\tArchive\n"
    "HD5\tArchive\n"
};

static const char magicpath [] = "/usr/share/misc/magic";

rc_t CCFileFormatAddRef (const CCFileFormat * self)
{
    if (self != NULL)
        atomic32_inc (&((CCFileFormat*)self)->refcount);
    return 0;
}

rc_t CCFileFormatRelease (const CCFileFormat * cself)
{
    rc_t rc = 0;
    CCFileFormat *self;

    self = (CCFileFormat *)cself; /* mutable field is ref count */
    if (self != NULL)
    {
        if (atomic32_dec_and_test (&self->refcount))
        {
            DEBUG_STATUS(("%s call KFileFormatRelease for extentions\n", __func__));
            rc = KFileFormatRelease (self->ext);
            if (rc == 0)
            {
                DEBUG_STATUS(("%s call KFileFormatRelease for magic\n", __func__));
                rc = KFileFormatRelease (self->magic);
                if (rc == 0)
                {
                    free (self);
                }
            }
        }
    }
    return rc;
}

rc_t CCFileFormatMake (CCFileFormat ** p)
{
    rc_t rc;
    CCFileFormat * self;

    DEBUG_ENTRY();

    self = malloc (sizeof *self);

    if (self == NULL)
    {
        rc = RC (rcExe, rcFileFormat, rcCreating, rcMemory, rcExhausted);
    }
    else
    {
        rc = KExtFileFormatMake (&self->ext, exttable, sizeof (exttable) - 1,
                                 formattable, sizeof (formattable) - 1);
        if (rc == 0)
        {
            rc = KMagicFileFormatMake (&self->magic, magicpath, magictable,
                                       sizeof (magictable) - 1, 
                                       formattable, sizeof (formattable) - 1);
            if (rc == 0)
            {
                atomic32_set (&self->refcount , 1);
                *p = self;
                return 0;
            }
        }
        free (self);
    }
    *p = NULL;
    return rc;
}

rc_t CCFileFormatGetType (const CCFileFormat * self, const KFile * file,
     const char * path, char * buffer, size_t buffsize, uint32_t * ptype, uint32_t * pclass)
{
    static const char u_u[] = "Unknown/Unknown";
    rc_t rc, orc;

    int ret;
    size_t mtz;
    size_t etz;
    size_t num_read;
    KFileFormatType mtype;
    KFileFormatType etype;
    KFileFormatClass mclass;
    KFileFormatClass eclass;
    char	mclassbuf	[256];
    char	mtypebuf	[256];
    char	eclassbuf	[256];
    char	etypebuf	[256];
    uint8_t	preread	[8192];

    DEBUG_ENTRY();
    DEBUG_STATUS(("%s: getting type for (%s)\n",__func__,path));

    /* initially assume that we don't know the type or class
     * these are just treated as files with no special processing
     * more than we that we don't know the type or class */

    *pclass = *ptype = 0;
    strncpy (buffer, u_u, buffsize);
    buffer[buffsize-1] = '\0'; /* in case we got truncated in the copy above */

    orc = KFileRead (file, 0, preread, sizeof (preread), &num_read);
    if (orc == 0)
    {
        if (CCFileFormatIsKar (preread))
        {
            *pclass = ccffcArchive;
            *ptype = ccfftaSra;
            strncpy (buffer, "Archive/SequenceReadArchive", buffsize);
            return 0;
        }
        if (CCFileFormatIsNCBIEncrypted (preread))
        {
            *pclass = ccffcEncoded;
            *ptype = ccffteNCBI;
            strncpy (buffer, "Encoded/NCBI", buffsize);
            return 0;
        }
        /* Sorta kinda hack to see if the file is WGA encrypted 
         * We short cut the other stuff if it is WGA encoded
         */
        if (KFileIsWGAEnc (preread, num_read) == 0)
        {
            *pclass = ccffcEncoded;
            *ptype = ccffteWGA;
            strncpy (buffer, "Encoded/WGA", buffsize);
            return 0;
        }
        

        rc = KFileFormatGetTypePath (self->ext, NULL, path, &etype, &eclass,
                                 etypebuf, sizeof (etypebuf), &etz);
        if (rc == 0)
        {

            rc = KFileFormatGetTypeBuff (self->magic, preread, num_read, &mtype, 
                                         &mclass, mtypebuf, sizeof (mtypebuf), &mtz);
            if (rc == 0)
            {
                rc = KFileFormatGetClassDescr (self->ext, eclass, eclassbuf, sizeof (eclassbuf));
                if (rc == 0)
                {
                    rc = KFileFormatGetClassDescr (self->magic, mclass, mclassbuf, sizeof (mclassbuf));
                    if (rc == 0)
                    {
                        DEBUG_STATUS(("%s: (%s) %s/%s<=%s/%s\n", __func__,
                                      path, mclassbuf, mtypebuf, eclassbuf, etypebuf));

                        /* first handle known special cases */
                        if ((strcmp("WinZip", mtypebuf) == 0) &&
                            (strcmp("GnuZip", etypebuf) == 0))
                        {
                            /* we've gotten in too many Zip files with extension gz */
                            PLOGMSG (klogWarn, 
                                     (klogWarn, "File '$(path)' is in unzupported winzip/pkzip format",
                                      "path=%s", path));
                        }
                        else if (!strcmp("BinaryAlignmentMap", etypebuf) && !strcmp ("GnuZip", mtypebuf))
                        {
				/*** bam files have gnuzip magic, we need to treat them as data files ***/
				strcpy (mclassbuf, eclassbuf );
				strcpy (mtypebuf, etypebuf);
				mtype = etype;
                                mclass = eclass;
			}
                        else if ((strcmp("SequenceReadArchive", etypebuf) == 0) &&
                                 (strcmp("Unknown", mtypebuf) == 0))
                        {
                            /* magic might not detect SRA/KAR files yet */
                            DEBUG_STATUS(("%s: (%s) %s/%s<=%s/%s\n", __func__,
                                          path, mclassbuf, mtypebuf, eclassbuf, etypebuf));
                            strcpy (mclassbuf, eclassbuf);
                            strcpy (mtypebuf, etypebuf);
                            mtype = etype;
                            mclass = eclass;
                        }
                        else if ((strcmp("HD5", etypebuf) == 0) &&
                                 (strcmp("Unknown", mtypebuf) == 0))
                        {
                            DEBUG_STATUS(("%s:5 (%s) %s/%s<=%s/%s\n", __func__,
                                          path, mclassbuf, mtypebuf, eclassbuf, etypebuf));
                            strcpy (mclassbuf, eclassbuf);
                            strcpy (mtypebuf, etypebuf);
                            mtype = etype;
                            mclass = eclass;
                        }

                        /* now that we've fixed a few cases use the magic derived
                         * class and type as the extensions could be wrong and can
                         * cause failures */
                        if (strcmp ("Archive", mclassbuf) == 0)
                        {       
                            *pclass = ccffcArchive;
                            if (strcmp ("TapeArchive", mtypebuf) == 0)
                                *ptype = ccfftaTar;
                            else if (strcmp ("SequenceReadArchive", mtypebuf) == 0)
                                *ptype = ccfftaSra;
                        }
                        else if (strcmp("Compressed", mclassbuf) == 0)
                        {
                            *pclass = ccffcCompressed;
                            if (strcmp ("Bzip", mtypebuf) == 0)
                            {
                                *ptype = ccfftcBzip2;
                                if ( no_bzip2 )
                                    * pclass = *ptype = 0;
                            }
                            else if (strcmp ("GnuZip", mtypebuf) == 0)
                                *ptype = ccfftcGzip;
                        }

                        /* Hmmm... we are using extension to determine XML though
                         * Probably okay */
                        else if (strcmp ("Cached", eclassbuf) == 0)
                        {
                            *pclass = ccffcCached;
                            if (strcmp ("ExtensibleMarkupLanguage", etypebuf) == 0)
                                *ptype = ccfftxXML;
                            strcpy (mclassbuf, eclassbuf);
                            strcpy (mtypebuf, etypebuf);

                        }

                        /* build the eventual filetype string - vaguely mime type like */
#if 1
                        ret = snprintf (buffer, buffsize, "%s/%s", mclassbuf, mtypebuf);
                        if (ret >= buffsize)
                        {
                            ret = buffsize-1;
                            buffer[buffsize-1] = '\0';
                        }
                        
#else
                        ecz = strlen (eclassbuf);
                        num_read = (ecz < buffsize) ? ecz : buffsize;
                        strncpy (buffer, eclassbuf, buffsize);
                        if (num_read >= (buffsize-2))
                            buffer [num_read] = '\0';
                        else
                        {
                            buffer [num_read++] = '/';
                            strncpy (buffer+num_read, etypebuf, 
                                     buffsize - num_read);
                        }
                        buffer[buffsize-1] = '\0';
                        /* 			    buffer [num_read++] = '/'; */
#endif
                    }
                }
            }
        }
        if (rc)
        {
            *pclass = *ptype = 0;
            strncpy (buffer, u_u, buffsize);
            buffer[buffsize-1] = '\0'; /* in case we got truncated in the copy above */
        }
    }
    return orc;
}
