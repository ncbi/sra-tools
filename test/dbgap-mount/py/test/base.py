import platform
import os.path
import shutil
import subprocess

class Base ( object ):

    _platform = None

    def __init__ ( self, plt ):
        self._platform = plt

    def platform ( self ):
        return self._platform

## Various checks
##
    def checkPath ( self, path ):
        if not os.path.exists ( path ):
            raise OSError ( "Can not find path [" + path + "]" )

    def checkDir ( self, path ):
        self.checkPath ( path )
        if not os.path.isdir ( path ):
            raise OSError ( "Path is not a directory [" + path + "]" )

    def checkFile ( self, path ):
        self.checkPath ( path )
        if not os.path.isfile ( path ):
            raise OSError ( "Path is not a file [" + path + "]" )

    def checkExecutable ( self, path ):
        self.checkFile ( path )
        if not os.access ( path, os.X_OK ):
            raise OSError ( "File is not executable [" + path + "]" )

    def checkExecutables ( self ):
        print ( "# Checking Executables" )
        self.checkExecutable ( self.platform ().mount_app () )
        self.checkExecutable ( self.platform ().config_app () )
        self.checkExecutable ( self.platform ().dump_app () )

    def checkInputs ( self ):
        print ( "# Checking Input Files" )
        self.checkFile ( self.platform ().ngc_file () )
        self.checkFile ( self.platform ().kart_file () )

    def makeDirectory ( self, path ):
        if not os.path.exists ( path ):
            print ( "# Create Directory [" + path + "]" )
            os.mkdir ( path )

    def makeDirectories ( self ):
        print ( "# Making Directories" )
        self.makeDirectory ( self.platform ().test_dir () )
        self.makeDirectory ( self.platform ().config_dir () )
        self.makeDirectory ( self.platform ().data_dir () )
        self.makeDirectory ( self.platform ().temp_dir () )
        if ( self.platform ().create_mount_point () ):
            self.makeDirectory ( self.platform ().mount_point () )

    def configureNo ( self ):
        cfg_file = self.platform ().config_file ()
        if os.path.exists ( cfg_file ):
            os.delete ( cfg_file )

    def configureBad ( self ):
        self.configureNo ()
        cfg_file = self.platform ().config_file ()
        print ( "# Configuring [" + cfg_file + "]" )
        file = open ( cfg_file, "w" )
        file.write ( "## auto-generated configuration file - DO NOT EDIT ##\n\n" )
        file.write ( "/repository/user/default-path = \"" + self.pathToPosix ( self.platform ().data_dir () ) + "\"\n" )
        print ( "/repository/user/default-path = \"" + self.pathToPosix ( self.platform ().data_dir () ) + "\"\n" )
        file.close ()

    def configureGood ( self ):
        self.configureBad ()
        cfg_file = self.platform ().config_file ()
        self.platform ().set_environment ()
        cfg_cmd = self.platform ().config_app() \
                        + " --import"   \
                        + self.platform ().ngc_file ()
        if subprocess.call ( [ self.platform ().config_app(), "--import", self.platform ().ngc_file () ] ) != 0:
            raise Exception ( "Failed to run command [" + cfg_cmd + "]" )
    def configure ( self ):
        self.configureGood ();

## Here long bunch of methods which will create affordable 
## configuration
##
    def removeTestEnvironment ( self ):
        test_dir = self._platform.test_dir ()
        if ( os.path.exists ( test_dir ) ):
            print ( "# Removing old test directories at [" + test_dir + "]\n" )
            shutil.rmtree ( test_dir )

    def createTestEnvironment ( self ):
        self.checkExecutables ()
        self.checkInputs ()

        test_dir = self._platform.test_dir ()
        print ( "# Creating new test environment at [" + test_dir + "]\n" )
        self.removeTestEnvironment ()
        self.makeDirectories ()
        self.configure ()

## Here is simple mount check
##
    def simpleMountCheck ( self ):
        mnt_pnt = self.platform ().mount_point ()
        print ( "# Performing mount pont check [" + mnt_pnt + "]" )

        targets = [ "cache", "kart-files", "karts", "workspace" ]
        for value in targets:
            self.checkDir ( mnt_pnt + self.platform ().separator () + value )

    def simpleUmountCheck ( self ):
        mnt_pnt = self.platform ().mount_point ()
        print ( "# Performing umount pont check [" + mnt_pnt + "]" )
        if self.platform ().create_mount_point ():
            if os.listdir ( mnt_pnt ):
                raise Exception ( "Mount point still in use [" + mnt_pnt + "]" )
        else:
            if os.path.exists ( mnt_pnt ):
                raise Exception ( "Mount point still in use [" + mnt_pnt + "]" )


## Pathetic methods ...
##
    def pathToPosix ( self, path ):
        if str.isalpha ( path [ 0 ] ) and path [ 1 ] == ':':
            retval = path [ 2 : ]
            if retval [ 0 ] == '\\':
                retval = "/" + path [ 0 ] + retval
            else:
                retval = "/" + path [ 0 ] + "/" + retval
        else:
            retval = path

        return retval.replace ( "\\", "/" )

    def bump ( self ):
        print ( "===============================================================================" );
        print ( "===============================================================================" );

    def workspacePath ( self, path ):
        return self.platform ().mount_point()   \
                + self.platform ().separator () \
                + "workspace"   \
                + self.platform ().separator () \
                + path


    def tempPath ( self, path ):
        return self.platform ().temp_dir()   \
                + self.platform ().separator () \
                + path


