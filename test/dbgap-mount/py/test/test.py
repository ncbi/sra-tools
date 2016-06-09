import os
import sys
import time
import subprocess
import gc

import platform
import base
import utils

class Test ( base.Base ):

    def __init__ ( self, plt ):
        super ( Test, self ).__init__( plt )

    def startDetached ( self ):
        super ( Test, self ).bump ()
        print ( "Start detached process" )
            ## Starting mount tool
            ##
        mnt_cmd =  super ( Test, self ).platform ().mount_app ()    \
                + " -d "    \
                + str ( super ( Test, self ).platform ().project () ) \
                + " "   \
                + super ( Test, self ).platform ().mount_point ()
        print ( "SYS [%s]\n" %( mnt_cmd ) )
        if subprocess.call (  [   \
                super ( Test, self ).platform ().mount_app (),  \
                "-d",   \
                str ( super ( Test, self ).platform ().project () ),    \
                super ( Test, self ).platform ().mount_point () \
                ]   \
                ) != 0:
            raise Exception ( "Can not execute command [" + mnt_cmd + "]" )
            ## Give one second to establish mount and checking mount
            ##
        time.sleep ( 1 )

        super ( Test, self ).simpleMountCheck ()

    def stopDetached ( self ):
            ## For now we are stoppin' without handling any exceptions
            ##
        umnt_cmd =  super ( Test, self ).platform ().mount_app ()    \
                + " -u "    \
                + super ( Test, self ).platform ().mount_point ()
        print ( "SYS [%s]\n" %( umnt_cmd ) )
#        if subprocess.call ( [   \
#                super ( Test, self ).platform ().mount_app (),  \
#                "-u",   \
#                super ( Test, self ).platform ().mount_point () \
#                ]   \
#                ) != 0:
#            raise Exception ( "Can not execute command [" + mnt_cmd + "]" )
        if subprocess.call ( [   \
                "/usr/sbin/diskutil"    \
                "unmountDisk"   \
                "force" \
                "/Users/iskhakov/DbGapMountTestMac/mount"
                ]   \
                ) != 0:
            raise Exception ( "Can not execute command [" + mnt_cmd + "]" )
        time.sleep ( 1 )

        super ( Test, self ).simpleUmountCheck ()

    def run ( self ):
        print ( "Runnin' test\n" )

            ## Configuring
            ##
        super ( Test, self ).createTestEnvironment ()

        path_1 = super ( Test, self ).workspacePath ( "file_1.file" )
        path_2 = super ( Test, self ).workspacePath ( "file_2.file" )
        path_3 = super ( Test, self ).workspacePath ( "dir" )
        path_4 = super ( Test, self ).workspacePath ( "dir/file_4.file" )
        path_2t = super ( Test, self ).tempPath ( "file_2.file" )

            ## First pass
            ##
        try:
            self.startDetached ()
                ## Creatin' 32K file and smaller files
            utils.createFile ( path_1, 33 )
            utils.createFile ( path_2, 32768 )

            gc.collect ()
            print ( "XXX: " + str ( gc.garbage ) )
            time.sleep ( 1 )

            utils.checkFileSize ( path_1, 33 )
            utils.deleteFile ( path_1 )

                ## Copyin' files
            utils.checkFileSize ( path_2, 32768 )
            utils.copyFile ( path_2, path_2t )

                ## Directoryin' files
            # utils.createDirectory ( path_3 )
            # utils.copyFile ( path_2, path_4 )
            # utils.listDirectory ( super ( Test, self ).workspacePath ( "." ) )
            # time.sleep ( 1 )


            print ( "XXX: " + str ( gc.garbage ) )
            self.stopDetached ()
        except:
            print ( "XXX: " + str ( gc.garbage ) )
            self.stopDetached ()
            print ( "Error:" + str ( sys.exc_info () [ 0 ] ) )
            raise
            return

            ## Second pass
            ##
        try:
            self.startDetached ()

            self.stopDetached ()
        except:
            self.stopDetached ()
            print ( "Error:" + str ( sys.exc_info () [ 0 ] ) )
            return

