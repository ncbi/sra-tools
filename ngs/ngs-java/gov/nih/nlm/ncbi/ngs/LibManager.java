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


import gov.nih.nlm.ncbi.ngs.error.LibraryIncompatibleVersionError;
import gov.nih.nlm.ncbi.ngs.error.LibraryLoadError;
import gov.nih.nlm.ncbi.ngs.error.LibraryNotFoundError;
import gov.nih.nlm.ncbi.ngs.error.cause.ConnectionProblemCause;
import gov.nih.nlm.ncbi.ngs.error.cause.DownloadDisabledCause;
import gov.nih.nlm.ncbi.ngs.error.cause.InvalidLibraryCause;
import gov.nih.nlm.ncbi.ngs.error.cause.JvmErrorCause;
import gov.nih.nlm.ncbi.ngs.error.cause.LibraryLoadCause;
import gov.nih.nlm.ncbi.ngs.error.cause.OutdatedJarCause;
import gov.nih.nlm.ncbi.ngs.error.cause.PrereleaseReqLibCause;
import gov.nih.nlm.ncbi.ngs.error.cause.UnsupportedArchCause;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;


/** This class is responsible for JNI dynamic library load
    and download from NCBI - when it cannot be found locally */
class LibManager implements FileCreator
{

    /** Force it in force-majeure situations.
        It also could be set without recompiling
        by setting vdb.System.loadLibrary java system property */
    private boolean JUST_DO_SIMPLE_LOAD_LIBRARY = false;

    /**
     * Will search for latest library version among all installed
     */
    private boolean SEARCH_FOR_LIBRARY = true;

    /**
     * Will check what is the latest version available online
     */
    private boolean AUTO_DOWNLOAD = true;

    /**
     * How often search for a latest installed library
     */
    private static final long SEARCH_LIB_FREQUENCY_INTERVAL = 7 * 24 * 60 * 60 * 1000;

    /**
     * How long we will trust latest version in cache before asking server
     */
    private static final long CACHE_LATEST_VERSION_INTERVAL = 7 * 24 * 60 * 60 * 1000;


    /** Possible location to search for library to load.
        The order of enum elements defines library location search order. */
    enum Location
    {
        CACHE,        // from ~/.ncbi/LibManager.properties
        NCBI_HOME,     // ~/.ncbi/lib64|32
        LIBPATH,       // iterate "java.library.path" - extended LD_LIBRARY_PATH
        NCBI_NGS_JAR_DIR, // directory where ncbi-ngs.jar is
        CLASSPATH,     // iterate "java.class.path" - where java classes are
        CWD,           // "."
        TMP,            // Temporary folder
        DOWNLOAD
    }


    enum Bits {
        b32,
        b64,
        bUNKNOWN;

        String intString()
        {   switch (this) { case b32: return "32"; default: return "64";} }
    }

    private class LibSearchResult {
        // will be true only if found version is compatible and higher or equal to minimal version
        boolean versionFits = false;
        Location location = null;
        String path = null;
        Version version = null;
        LibraryLoadCause failCause = null;
    }

    private class LibDownloadResult {
        String savedPath = null;
        DownloadManager.DownloadResult status;
    }

////////////////////// TODO check out of space condition ///////////////////////

    LibManager ( String [] libs, String [] versions )
    {
        if (versions == null || libs == null) {
            throw new RuntimeException("Neither libs nor versions can be null");
        }

        if (versions.length != libs.length) {
            throw new RuntimeException("Invalid library versions: must match number of libraries");
        }

        checkSystemProperties();
        locations = generateLocations();

        for (int i = 0; i < libs.length; ++i) {
            libraryVersions.put(libs[i], versions[i]);
        }

        properties = new LMProperties(detectJVM().intString(), libraryVersions);
        if (AUTO_DOWNLOAD) {
            downloadManager = new DownloadManager(properties);
        }

        if (!JUST_DO_SIMPLE_LOAD_LIBRARY && System.getProperty("vdb.deleteLibraries") != null) {
            /* make sure we have the latest version of ngs-sdk & ncbi-vdb dll-s */
            for (String libname : libs) {
                Logger.warning( "Deleting all JNI libraries...");
                LibPathIterator.deleteLibraries(this, libname);
            }
        }
    }

    private void checkSystemProperties() {
        String loadLibraryProperty = System.getProperty("vdb.System.loadLibrary");
        if (loadLibraryProperty != null && loadLibraryProperty.equals("1")) {
            Logger.warning ( "Smart DLL search and library download was disabled" );
            JUST_DO_SIMPLE_LOAD_LIBRARY = true;
            AUTO_DOWNLOAD = false;
            SEARCH_FOR_LIBRARY = false;
            return;
        }

        String noLibraryDownload = System.getProperty("vdb.System.noLibraryDownload");
        if (noLibraryDownload != null && noLibraryDownload.equals("1")) {
            Logger.warning ( "DLL download was disabled" );
            AUTO_DOWNLOAD = false;
        }

        String noLibrarySearch = System.getProperty("vdb.System.noLibrarySearch");
        if (noLibrarySearch != null && noLibrarySearch.equals("1")) {
            Logger.warning ( "Search of installed DLL was disabled" );
            SEARCH_FOR_LIBRARY = false;
        }
    }

    private Location[] generateLocations() {
        Location[] allLocations = Location.values();
        Location[] result;
        Set<Location> disabledLocations = new TreeSet<Location>();
        if (JUST_DO_SIMPLE_LOAD_LIBRARY) {
            disabledLocations.addAll(Arrays.asList(allLocations));
            disabledLocations.remove(Location.LIBPATH);
        }

        if (!AUTO_DOWNLOAD) {
            disabledLocations.add(Location.DOWNLOAD);
        }

        if (!SEARCH_FOR_LIBRARY) {
            // disable everything except cache, libpath and download
            Set<Location> allowedLocations = new TreeSet<Location>(Arrays.asList(new Location[] {
                    Location.CACHE, Location.LIBPATH, Location.DOWNLOAD
            }));
            for (Location location : allLocations) {
                if (!allowedLocations.contains(location)) {
                    disabledLocations.add(location);
                }
            }
        }

        if (disabledLocations.size() > 0) {
            Logger.info("Disabled locations: " + Arrays.toString(disabledLocations.toArray()));
        }

        result = new Location[allLocations.length - disabledLocations.size()];
        int i = 0;
        for (Location location : allLocations) {
            if (disabledLocations.contains(location)) {
                continue;
            }

            result[i++] = location;
        }

        assert i == result.length;

        return result;
    }

    Location[] locations()
    {
        return locations;
    }


//////////////////////////// FileCreator interface /////////////////////////////


    /** Creates a file by finding directory by iterating the location array
        and using libname to generate the file name */
    public BufferedOutputStream create ( String libname ) {
        createdFileName = null;
        for (int i = 0; i < 2; ++i) {
            Location location = null;
            boolean model = true;
            switch (i) {
                case 0:
                    location = Location.NCBI_HOME;
                    model = false;
                    break;
                case 1:
                    break;
            }
            LibPathIterator it = new LibPathIterator
                (this, location, mapLibraryName(libname, model), true);

            while (true) {
                String pathname = it.nextName();
                if (pathname == null) {
                    return null;
                }

                Logger.fine("Trying to create " + pathname + "...");
                File file = new File(pathname);
                try {
                    pathname = file.getAbsolutePath();
                } catch (SecurityException e) {
                    Logger.warning(pathname + " : cannot getAbsolutePath " + e);
                    continue;
                }
                if (file.exists()) {
                    String dathname = pathname + ".bak";
                    File dest = new File(dathname);
                    {
                        String name = System.getProperty("os.name");
                        if (name != null && name.startsWith("Win")) {
                     /*    On Windows delete the file we are going to rename to.
                           Otherwise renaming will fail. */
                            if (dest.exists()) {
                                Logger.fine
                                    ("Trying to remove " + dathname + " ...");
                                dest.delete();
                            }
                        }
                    }
                    Logger.finest("Trying to rename " + pathname
                        + " to " + dathname + " ...");
                    if (!file.renameTo(dest)) {
                        Logger.warning
                            (pathname + ".renameTo(" + dathname + ") failed");
                    }
                }
                FileOutputStream s = null;
                try {
                    s = new FileOutputStream(pathname);
                } catch (FileNotFoundException e) {
/* e.message = pathname (Permission denied):
could be because pathname is not writable
or pathname not found and its directory is not writable */
                    Logger.warning("Cannot open " + pathname);
                    continue;
                }

                createdFileName = pathname;

                Logger.fine("Opened " + pathname);
                return new BufferedOutputStream(s, HttpManager.BUF_SZ);
            }
        }
        return null;
    }


    public void done(boolean success) {
        if (!success) {
            createdFileName = null;
        }
    }


////////////////////////////////////////////////////////////////////////////////


    /**
     * Loads the system library by finding it by iterating the location array.
     *   Try to download it from NCBI if not found.
     *
     * Will throw LibraryLoadError when failed.
     */
    void loadLibrary( String libname ) {
        Version requiredVersion = getRequiredVersion(libname);
        boolean updateCache = Arrays.asList(locations).contains(Location.CACHE);

        Logger.fine("Searching for " + libname + " library...");
        try {
            LibSearchResult searchResult = searchLibrary(libname, requiredVersion);

            if (searchResult.path == null) {
                throw new LibraryNotFoundError(libname, "No installed library was found",
                        searchResult.failCause);
            }

            Logger.fine("Found " + libname + " library");

            String libpath = searchResult.path;
            Logger.info("Loading " + libname + "...");
            try {
                if (!mocksEnabled) {
                    if (libpath.startsWith(libname)) {
                        System.loadLibrary(libpath);
                    } else {
                        System.load(libpath);
                    }
                } else if (mockLoadException != null) {
                    throw mockLoadException;
                }
            } catch (Throwable e) {
                if (searchResult.location != Location.DOWNLOAD) {
                    throw new LibraryLoadError(libname, "Failed to load found library " + libpath,
                            new JvmErrorCause(e));
                }

                throw new LibraryLoadError(libname, "No installed library was found and downloaded library '" + libpath + "' cannot be loaded",
                        new JvmErrorCause(e),
                        "Please install ngs and ncbi-vdb manually:" +
                                " https://github.com/ncbi/ngs/wiki/Downloads" +
                                " or write to \"sra-tools@ncbi.nlm.nih.gov\" if problems persist");

            }
            Logger.fine("Loaded " + libname + " library");

            Logger.fine("Checking library " + libname + " version...");
            String v;
            if (!mocksEnabled) {
                v = LibVersionChecker.getLoadedVersion(libname);
            } else {
                v = mockLoadedLibraryVersion;
            }
            if (v == null) {
                throw new LibraryLoadError(libname, "Failed to retrieve loaded library's version", new InvalidLibraryCause());
            }
            Version loadedVersion = new Version(v);
            if (loadedVersion.compareTo(requiredVersion) < 0 || !loadedVersion.isCompatible(requiredVersion)) {
                Logger.fine("Library version is not compatible. Required: " + requiredVersion.toSimpleVersion() + " loaded: " + loadedVersion.toSimpleVersion());
                LibraryLoadCause failCause = searchResult.failCause;
                if (searchResult.location == Location.DOWNLOAD || failCause == null) {
                    failCause = (loadedVersion.compareTo(requiredVersion) < 0) ?
                            new PrereleaseReqLibCause() : new OutdatedJarCause();
                }
                throw new LibraryIncompatibleVersionError(libname, "Library is incompatible",
                        libpath, failCause);
            }
            Logger.fine("Library " + libname + " was loaded successfully." +
                    " Version = " + loadedVersion.toSimpleVersion());

            if (updateCache) {
                properties.loaded(libname, searchResult.version.toSimpleVersion(), libpath);

                if (searchResult.location != Location.CACHE) {
                    properties.setLastSearch(libname);
                }
            }
        } catch (LibraryLoadError e) {
            if (updateCache) {
                properties.notLoaded(libname);
            }
            Logger.warning("Loading of " + libname + " library failed");
            throw e;
        } finally {
            if (updateCache) {
                properties.store();
            }
        }
    }

//////////////////////////// static package methods ////////////////////////////


    static String[] mapLibraryName(String libname)
    {
        return mapLibraryName(libname, true);
    }


    static String[] mapLibraryName(String libname, boolean withDataModel)
    {
        String m = libname;
        if (withDataModel) {
            m = libnameWithDataModel(libname);
        }
        String name = System.getProperty("os.name");
        int dup = 1;
        if (name != null  && name.equals("Mac OS X")) {
            dup = 2;
        }
        int n = (m == null ? 1 : 2) * dup;
        String[] ns = new String[n];
        int i = 0;
        if (m != null) {
            ns[i++] = System.mapLibraryName(m);
        }
        ns[i++] = System.mapLibraryName(libname);
        if (dup == 2) {
            if (m != null) {
                ns[i++] = m + ".dylib";;
            }
            ns[i++] = libname + ".dylib";;
        }
        return ns;
    }


    static Bits detectJVM()
    {
        final String keys [] = {
            "sun.arch.data.model",
            "com.ibm.vm.bitmode",
            "os.arch",
        };
        for (String key : keys ) {
            String property = System.getProperty(key);
            Logger.finest(key + "=" + property);
            if (property != null) {
                int errCode = (property.indexOf("64") >= 0) ? 64 : 32;
                Logger.finest(errCode + "-bit JVM");
                return errCode == 64 ? Bits.b64 : Bits.b32;
            }
        }
        Logger.fine("Unknown-bit JVM");
        return Bits.bUNKNOWN;
    }

//////////////////////////// private static methods ////////////////////////////


    /** Add 32- or 64-bit data model suffix */
    private static String libnameWithDataModel(String libname)
    {
        String m = null;
        switch (detectJVM()) {
            case b64:
                m = "-64";
                break;
            case b32:
                m = "-32";
                break;
        }
        if (m != null) {
            m = libname + m;
        }
        return m;
    }


    private static Location[] getLocationProperty()
    {
        String p = System.getProperty("vdb.loadLibraryLocations");
        if (p == null) {
            return null;
        }

        int n = 0;
        for (int i = 0; i < p.length(); ++i) {
            if ("PJCLNTWD".indexOf(p.charAt(i)) >= 0) {
                ++n;
            }
        }

        if (n == 0) {
            return null;
        }
                
        Location locations[] = new Location[n];
        n = 0;
        for (int i = 0; i < p.length(); ++i) {
            switch (p.charAt(i)) {
                case 'P':
                    locations[n] = Location.CLASSPATH;
                    break;
                case 'J':
                    locations[n] = Location.NCBI_NGS_JAR_DIR;
                    break;
                case 'C':
                    locations[n] = Location.CACHE;
                    break;
                case 'L':
                    locations[n] = Location.LIBPATH;
                    break;
                case 'N':
                    locations[n] = Location.NCBI_HOME;
                    break;
                case 'T':
                    locations[n] = Location.TMP;
                    break;
                case 'W':
                    locations[n] = Location.CWD;
                    break;
                case 'D':
                    locations[n] = Location.DOWNLOAD;
                    break;
                default:
                    continue;
            }
            ++n;
        }

        return locations;
    }

    private Version checkLibraryVersion(String libname, String libpath, boolean useLoadLibrary) {
        if (!useLoadLibrary && !fileExists(libpath)) {
            Logger.finer("File " + libpath + " not found");
            return null;
        }

        Version version = LibVersionChecker.getVersion(libname, libpath, useLoadLibrary);
        if (version == null) {
            Logger.fine("Cannot load or get library version: " + libpath);
        }

        return version;
    }

    private static boolean fileExists(String filename) {
        File file = new File(filename);
        if (file.exists()) {
            return true;
        } else {
            Logger.finest(filename + " does not exist");
            return false;
        }
    }

////////////////////////////////////////////////////////////////////////////////

    private Version getRequiredVersion(String libname) {
        String minimalVersion = libraryVersions.get(libname);
        if (minimalVersion == null) {
            throw new RuntimeException("Library '" + libname + "' version was not specified");
        }
        return new Version(minimalVersion);
    }

    private Version getLatestVersion(String libname) {
        if (latestVersions.containsKey(libname)) {
            return latestVersions.get(libname);
        }

        String v = properties.getLatestVersion(libname, CACHE_LATEST_VERSION_INTERVAL);
        if (v == null && AUTO_DOWNLOAD) {
            v = downloadManager.getLatestVersion(libname);
            if (v != null) {
                properties.setLatestVersion(libname, v);
            }
        } else {
            Logger.fine ( "Cached LatestVersion (" + libname + ") = " + v );
        }

        // we will cache whatever we have here, even null
        Version version = null;
        if (v != null) {
            version = new Version(v);
        }
        latestVersions.put(libname, version);

        return version;
    }

    private LibSearchResult searchLibrary(String libname, Version requiredVersion) {
        LibSearchResult searchResult = new LibSearchResult();

        for (Location l : locations) {
            Logger.info("Checking " + libname + " from " + l + "...");

            List<String> pathsToCheck = new ArrayList<String>();
            boolean useLoadLibrary = false;
            boolean searchEvenAfterFound = !JUST_DO_SIMPLE_LOAD_LIBRARY;
            switch (l) {
            case LIBPATH: {
                pathsToCheck.add(libname);
                String libnameWithDataModel = libnameWithDataModel(libname);
                if (libnameWithDataModel != null) {
                    pathsToCheck.add(libnameWithDataModel);
                }
                useLoadLibrary = true;
                break;
            }
            case CACHE: {
                String filename = properties.get(libname);
                if (filename == null) {
                    continue;
                }

                // when search is enabled, we might skip it if cache has information about previously
                // loaded library and last search was less than SEARCH_LIB_FREQUENCY_INTERVAL ago
                Date lastSearchDate = properties.getLastSeach(libname);

                searchEvenAfterFound = lastSearchDate == null ||
                        (new Date().getTime() - lastSearchDate.getTime() > SEARCH_LIB_FREQUENCY_INTERVAL);

                pathsToCheck.add(filename);
                // this is kind of hack, but it does not require different checks between win/unix
                // we say that library was loaded from LIBPATH location when its path starts from library simple name
                useLoadLibrary = filename.startsWith(libname);
                break;
            }
            case DOWNLOAD: {
                Logger.info("Downloading " + libname + " from NCBI...");
                LibDownloadResult downloadResult;
                if (!mocksEnabled) {
                    downloadResult = download(libname);
                } else if (mockDownloadStatus == null) {
                    throw new RuntimeException("mockDownloadStatus must be set when mocks enabled");
                } else {
                    downloadResult = new LibDownloadResult();
                    downloadResult.status = mockDownloadStatus;
                    downloadResult.savedPath = "/some/path/" + libname;
                }
                if (downloadResult.status != DownloadManager.DownloadResult.SUCCESS) {
                    Logger.warning("Failed to download " + libname + " from NCBI");
                    if (downloadResult.status == DownloadManager.DownloadResult.UNSUPPORTED_OS) {
                        searchResult.failCause = new UnsupportedArchCause();
                    } else {
                        searchResult.failCause = new ConnectionProblemCause();
                    }
                    continue;
                }
                Logger.info("Downloaded " + libname + " from NCBI");
                Logger.fine("Checking " + libname + " library...");

                pathsToCheck.add(downloadResult.savedPath);
                break;
            }
            default: {
                String name[] = mapLibraryName(libname);
                Logger.finest("System.mapLibraryName(" + libname + ") = " + name[0]);

                LibPathIterator it = new LibPathIterator(l, name);
                while (true) {
                    String filename = it.nextName();
                    if (filename == null) {
                        break;
                    }

                    pathsToCheck.add(filename);
                }
                break;
            }
            }


            boolean foundInLocation = false;
            for (String path : pathsToCheck) {
                Version v;
                if (!mocksEnabled) {
                    v = checkLibraryVersion(libname, path, useLoadLibrary);
                } else if (mockLocationVersions == null) {
                    throw new RuntimeException("mockLocationVersions must be set when mocks enabled");
                } else {
                    v = mockLocationVersions.get(l);
                }
                if (v == null) {
                    continue;
                }

                foundInLocation = true;

                boolean versionFits = v.isCompatible(requiredVersion) && v.compareTo(requiredVersion) >= 0;
                // replace a found version if either:
                // a) none was previously found
                // b) found version which fits requirements and it is higher than previously found
                // c) found version version which fits requirements while previously found one does not
                if (searchResult.path == null ||
                        (versionFits && (v.compareTo(searchResult.version) > 0) || !searchResult.versionFits)) {
                    searchResult.versionFits = versionFits;
                    searchResult.location = l;
                    searchResult.version = v;
                    searchResult.path = path;
                }

                if (searchResult.versionFits && !searchEvenAfterFound) {
                    break;
                }
            }

            if (l == Location.DOWNLOAD) {
                // when we downloaded something that either can't be loaded or does not fit our requirements
                // or we just overwrote our best found library and cannot load it
                if (!searchResult.versionFits ||
                        (!foundInLocation && searchResult.path.equals(pathsToCheck.get(0)))) {
                    searchResult.versionFits = false;
                    searchResult.location = l;
                    searchResult.version = null;
                    searchResult.path = pathsToCheck.get(0);
                }
            }

            if (searchResult.version != null && searchResult.version.isCompatible(requiredVersion)) {
                Version latestVersion = getLatestVersion(libname);
                // if we don't know the latest version, then we might stop the search if
                // found version is okay and searchEvenAfterFound == false
                if (latestVersion == null && searchResult.version.compareTo(requiredVersion) >= 0 &&
                        !searchEvenAfterFound) {
                    break;
                }
                // if we know the latest version, then we should search until find it
                if (latestVersion != null && searchResult.version.compareTo(requiredVersion) >= 0) {
                    break;
                }
            }
        }

        boolean downloadEnabled = Arrays.asList(locations).contains(Location.DOWNLOAD);

        if (searchResult.failCause == null && !downloadEnabled) {
            searchResult.failCause = new DownloadDisabledCause();
        }

        return searchResult;
    }

////////////////////////////////////////////////////////////////////////////////


    /** Downloads the library and default configuration from NCBI.
        Save them where it can be found by LibManager.loadLibrary() */
    private LibDownloadResult download(String libname) {
        if (!AUTO_DOWNLOAD) {
            throw new RuntimeException("AUTO_DOWNLOAD is disabled. This method should not be called");
        }

        LibDownloadResult result = new LibDownloadResult();

        Version latestVersion = getLatestVersion(libname);
        if (latestVersion == null) {
            result.status = DownloadManager.DownloadResult.FAILED;
            return result;
        }
        result.status = downloadManager.downloadLib(this, libname, latestVersion);

        if (result.status == DownloadManager.DownloadResult.SUCCESS) {
            result.savedPath = createdFileName;
            createdFileName = null;
            properties.saved(libname, latestVersion.toSimpleVersion(), result.savedPath);
        }

        return result;

//        return downloadKfg(knownLibPath[i + 1]);
    }

    /** Fetches the configuration from NCBI */
    private boolean downloadKfg(String libpath) {
        Logger.finest("configuration download is disabled");
/*
        // this is broken. if enabled, move download part to a DownloadManager
        File l = new File(libpath);
        String d = l.getParent();
        if (d == null) {
            Logger.finest("cannot get parent path of " + libpath);
            return true;
        }
        String n = d + File.separatorChar + "ncbi";
        File fn = new File(n);
        if (fn.exists()) {
            if (fn.isDirectory()) {
                Logger.finest("configuration directory '" + n + "' exists");
            } else {
                Logger.finest("'" + n + "' is not a directory");
                return true;
            }
        } else {
            Logger.finest("configuration directory '" + n + "' does not exist");
            try {
                if (!fn.mkdir()) {
                    Logger.finest("cannot mkdir '" + n + "'");
                    return true;
                }
            } catch (SecurityException e) {
                Logger.finest(e);
                return true;
            }
        }
        try {
            fn.setExecutable(true, true);
        } catch (SecurityException e) {
            Logger.finest(e);
        }
        try {
            fn.setReadable(true, true);
        } catch (SecurityException e) {
            Logger.finest(e);
        }
        try {
            fn.setWritable(true, true);
        } catch (SecurityException e) {
            Logger.finest(e);
        }
        String k = n + File.separatorChar + "default.kfg";
        File fk = new File(k);
        if (fk.exists()) {
            Logger.finest("'" + fk + "' exists");
            return true;
        }
        String request = "cmd=lib&libname=kfg";
        for (SratoolkitCgis cgis = new SratoolkitCgis(); ; ) {
            String spec = cgis.nextSpec();
            if (spec == null) {
                break;
            }
            try {
                String f = HttpManager.post(spec, request);
                try {
                    FileOutputStream out = new FileOutputStream(fk);
                    try {
                        out.write(f.getBytes());
                        out.close();
                    } catch (IOException e) {
                        Logger.finest(e);
                        continue;
                    }
                    Logger.finest("created '" + fk + "'");
                    return true;
                } catch (FileNotFoundException e) {
                    Logger.finest(e);
                }
            } catch (HttpException e) {
                Logger.finest(e);
            }
        }
        Logger.finest("cannot create '" + fk + "'");
*/
        return true;
    }

////////////////////////////////////////////////////////////////////////////////

    /** Possible location to search for library to load.
        The order of elements defines library location search order. */
    private Location[] locations;

    /** Minimal versions for each of the libraries */
    private Map<String, String> libraryVersions = new HashMap<String, String>();

    /** Latest versions downloaded from NCBI or taken from cache for each of the libraries */
    private HashMap<String, Version> latestVersions = new HashMap<String, Version>();

    /** Knows how to check and download latest libraries versions */
    private DownloadManager downloadManager;

    /** Is updated by FileCreator methods called by HttpManager */
    private String createdFileName;

    /** File that plays a role of a cache for information
     * of libraries locations/versions between runs */
    private LMProperties properties; // to keep dll path/version-s

////////////////////////////////////////////////////////////////////////////////
/// These variables alter LibManager behaviour and should only be used in tests

    boolean mocksEnabled = false;
    Map<Location, Version> mockLocationVersions;
    DownloadManager.DownloadResult mockDownloadStatus;
    Throwable mockLoadException;
    String mockLoadedLibraryVersion;


////////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
-Djava.library.path
-Dvdb.log
off if JUST_DO_SIMPLE_LOAD_LIBRARY

-Dvdb.System.noLibraryDownload=1 - will turn auto-download off
-Dvdb.System.noLibrarySearch=1  - with previous option will not try
                                  to find latest installed lib

TODO
try to load the library if LibManager.loadLibrary() was never called.
What if loadLibrary() is called several times? load()
      https://docs.oracle.com/javase/6/docs/api/java/util/logging/Level.html
      https://en.wikipedia.org/wiki/Log4j
      https://logging.apache.org/log4j/1.2/apidocs/org/apache/log4j/Level.html
-Dlog=0   1
-Dlog=OFF SEVERE/FATAL
-Dlog=O   S/F
*******************************************************************************/
}
