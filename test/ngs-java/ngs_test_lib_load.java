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


package gov.nih.nlm.ncbi.ngs;

import static org.junit.Assert.fail;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import gov.nih.nlm.ncbi.ngs.LibManager;
import gov.nih.nlm.ncbi.ngs.Logger;
import gov.nih.nlm.ncbi.ngs.Version;
import gov.nih.nlm.ncbi.ngs.error.LibraryIncompatibleVersionError;
import gov.nih.nlm.ncbi.ngs.error.LibraryLoadError;
import gov.nih.nlm.ncbi.ngs.error.LibraryNotFoundError;
import gov.nih.nlm.ncbi.ngs.error.cause.ConnectionProblemCause;
import gov.nih.nlm.ncbi.ngs.error.cause.DownloadDisabledCause;
import gov.nih.nlm.ncbi.ngs.error.cause.InvalidLibraryCause;
import gov.nih.nlm.ncbi.ngs.error.cause.JvmErrorCause;
import gov.nih.nlm.ncbi.ngs.error.cause.OutdatedJarCause;
import gov.nih.nlm.ncbi.ngs.error.cause.PrereleaseReqLibCause;
import gov.nih.nlm.ncbi.ngs.error.cause.UnsupportedArchCause;
import org.junit.Test;
import org.junit.Before;
import org.junit.After;

import ngs.ReadCollection;
import ngs.Alignment;
import ngs.ErrorMsg;

import gov.nih.nlm.ncbi.ngs.NGS;

import java.lang.String;
import java.lang.System;
import java.lang.Throwable;
import java.util.TreeMap;

//The purpose of this suite is to verify native library load code,
//including testing various error cases.
public class ngs_test_lib_load {
    static String test_lib = "test_lib";
    static String test_lib_ver = "1.1.0";
    LibManager libManager;
    Logger.Level savedLogLevel;
    String savedSysLoadLibrary;
    String savedSysNoLibDownload;

    @Before
    public void saveSysVars() {
        savedLogLevel = Logger.getLevel();
        savedSysLoadLibrary = System.getProperty("vdb.System.loadLibrary");
        savedSysNoLibDownload = System.getProperty("vdb.System.noLibraryDownload");
    }

    @Before
    public void setupEnv() {
        Logger.setLevel(Logger.Level.OFF);
        System.setProperty("vdb.System.loadLibrary", "0");
    }

    @After
    public void restoreSysVars() {
        Logger.setLevel(savedLogLevel);
        sysSetProp("vdb.System.loadLibrary", savedSysLoadLibrary);
        sysSetProp("vdb.System.noLibraryDownload", savedSysNoLibDownload);
    }

    private void sysSetProp(String name, String value) {
        if (value != null)
            System.setProperty(name, value);
        else
            System.clearProperty(name);
    }

    private void createLibManager() {
        libManager = new LibManager(
                new String[] { test_lib },
                new String[] { test_lib_ver }
        );
        libManager.mocksEnabled = true;
        libManager.mockLocationVersions = new TreeMap();
    }

    private Throwable loadAndCatch(String libName) {
        Throwable exception = null;
        try {
            libManager.loadLibrary(test_lib);
        } catch (Throwable e) {
            exception = e;
        }
        return exception;
    }

    @Test
    public void LibManagerMock_Ok()
    {
        createLibManager();
        libManager.mockLocationVersions.put(LibManager.Location.LIBPATH, new Version(test_lib_ver));
        libManager.mockLoadedLibraryVersion = test_lib_ver;

        assertEquals(loadAndCatch(test_lib), null);
    }

    @Test
    public void LibManagerMock_NotFound_NoDownload()
    {
        System.setProperty("vdb.System.noLibraryDownload", "1");
        createLibManager();
        Throwable e = loadAndCatch(test_lib);
        assertNotNull(e);
        assertEquals(LibraryNotFoundError.class, e.getClass());
        assertNotNull(e.getCause());
        assertEquals(DownloadDisabledCause.class, e.getCause().getClass());
    }

    @Test
    public void LibManagerMock_NotFound_DownloadFail()
    {
        createLibManager();
        libManager.mockDownloadStatus = DownloadManager.DownloadResult.FAILED;

        Throwable e = loadAndCatch(test_lib);
        assertNotNull(e);
        assertEquals(LibraryNotFoundError.class, e.getClass());
        assertNotNull(e.getCause());
        assertEquals(ConnectionProblemCause.class, e.getCause().getClass());
    }

    @Test
    public void LibManagerMock_NotFound_DownloadUnsupportedOs()
    {
        createLibManager();
        libManager.mockDownloadStatus = DownloadManager.DownloadResult.UNSUPPORTED_OS;

        Throwable e = loadAndCatch(test_lib);
        assertNotNull(e);
        assertEquals(LibraryNotFoundError.class, e.getClass());
        assertNotNull(e.getCause());
        assertEquals(UnsupportedArchCause.class, e.getCause().getClass());
    }

    @Test
    public void LibManagerMock_LoadFailed_BadDownloadFile()
    {
        createLibManager();
        libManager.mockDownloadStatus = DownloadManager.DownloadResult.SUCCESS;
        libManager.mockLoadException = new UnsatisfiedLinkError();

        Throwable e = loadAndCatch(test_lib);
        assertNotNull(e);
        assertEquals(LibraryLoadError.class, e.getClass());
        assertNotNull(e.getCause());
        assertEquals(JvmErrorCause.class, e.getCause().getClass());
    }

    @Test
    public void LibManagerMock_LoadFailed_GetVersionFailed()
    {
        createLibManager();
        libManager.mockDownloadStatus = DownloadManager.DownloadResult.SUCCESS;

        Throwable e = loadAndCatch(test_lib);
        assertNotNull(e);
        assertEquals(LibraryLoadError.class, e.getClass());
        assertNotNull(e.getCause());
        assertEquals(InvalidLibraryCause.class, e.getCause().getClass());
    }

    @Test
    public void LibManagerMock_IncompatibleVers_DownloadDisabled()
    {
        System.setProperty("vdb.System.noLibraryDownload", "1");
        String incompVersions[] = new String[] { "0.0.1", "1.0.0", "2.0.0" };
        for (String version : incompVersions) {
            createLibManager();
            libManager.mockLocationVersions.put(LibManager.Location.LIBPATH, new Version(version));
            libManager.mockLoadedLibraryVersion = version;

            Throwable e = loadAndCatch(test_lib);
            assertNotNull(e);
            assertEquals(LibraryIncompatibleVersionError.class, e.getClass());
            assertNotNull(e.getCause());
            assertEquals(DownloadDisabledCause.class, e.getCause().getClass());
        }
    }

    @Test
    public void LibManagerMock_IncompatibleVers_DownloadPrereleaseVers()
    {
        String incompVersions[] = new String[] { "0.0.1", "1.0.0" };
        for (String version : incompVersions) {
            createLibManager();
            libManager.mockDownloadStatus = DownloadManager.DownloadResult.SUCCESS;
            libManager.mockLocationVersions.put(LibManager.Location.DOWNLOAD, new Version(version));
            libManager.mockLoadedLibraryVersion = version;

            Throwable e = loadAndCatch(test_lib);
            assertNotNull(e);
            assertEquals(LibraryIncompatibleVersionError.class, e.getClass());
            assertNotNull(e.getCause());
            assertEquals(PrereleaseReqLibCause.class, e.getCause().getClass());
        }
    }

    @Test
    public void LibManagerMock_IncompatibleVers_Download_OutdatedJar()
    {
        String version = "2.0.0";
        createLibManager();
        libManager.mockDownloadStatus = DownloadManager.DownloadResult.SUCCESS;
        libManager.mockLocationVersions.put(LibManager.Location.DOWNLOAD, new Version(version));
        libManager.mockLoadedLibraryVersion = version;

        Throwable e = loadAndCatch(test_lib);
        assertNotNull(e);
        assertEquals(LibraryIncompatibleVersionError.class, e.getClass());
        assertNotNull(e.getCause());
        assertEquals(OutdatedJarCause.class, e.getCause().getClass());
    }
}