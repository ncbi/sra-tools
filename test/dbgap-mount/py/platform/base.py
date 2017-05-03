import os
import os.path

class Base:

    TEST_DIR_NAME = "DbGapMountTest"

    def __init__ ( self, appd ):
        if os.path.exists ( appd ) == False:
            raise OSError ( "Can not stat directory '%s'" %( appd ) )

        self._app_dir = appd
        self._sep = os.sep
        self._user_dir = os.path.expanduser("~")

    def get_os ( self ):
        raise NameError ( "Unsupported OS" )

    def input_data_dir ( self ):
        return "/panfs/pan1/trace_work/iskhakov/IMPORTANT_DIR"

    def separator ( self ):
        return self._sep

    def user_dir ( self ):
        return self._user_dir

    def app_dir ( self ):
        return self._app_dir

    def test_dir ( self ):
        return self.user_dir () \
                    + self.separator () \
                    + self.TEST_DIR_NAME \
                    + self.get_os ()

    def config_dir ( self ):
        return self.test_dir () \
                    + self.separator () \
                    + ".ncbi"

    def data_dir ( self ):
        return self.test_dir () \
                    + self.separator ()    \
                    + "ncbi"

    def temp_dir ( self ):
        return self.test_dir () \
                    + self.separator ()    \
                    + "temp"

    def config_file ( self ):
        return self.config_dir () \
                    + self.separator ()     \
                    + "user-settings.mkfg"

    def mount_point ( self ):
        return self.test_dir () \
                    + self.separator ()     \
                    + "mount"

    def project ( self ):
        return 6570

    def ngc_file ( self ):
        return self.input_data_dir ()   \
                    + self.separator () \
                    + self.TEST_DIR_NAME \
                    + self.separator () \
                    + "prj_"    \
                    + str ( self.project () )   \
                    + ".ngc"

    def kart_file ( self ):
        return self.input_data_dir ()   \
                    + self.separator () \
                    + self.TEST_DIR_NAME \
                    + self.separator () \
                    + "kart_prj_"    \
                    + str ( self.project () )   \
                    + ".krt"

    def mount_app ( self ):
        return self.app_dir ()  \
                    + self.separator () \
                    + "dbgap-mount-tool"

    def config_app ( self ):
        return self.app_dir ()  \
                    + self.separator () \
                    + "vdb-config"

    def dump_app ( self ):
        return self.app_dir ()  \
                    + self.separator () \
                    + "fastq-dump-new"

    def umount_app ( self ):
        raise NameError ( "Unsupported OS" )

    def create_mount_point ( self ):
        return True

    def set_environment ( self ):
        os.putenv ( "NCBI_HOME", self.config_dir () )

    def about ( self ):
        print ( str ( self ) )
        print ( self.separator () )
        print ( self.config_dir () )
        print ( self.test_dir () )
        print ( self.data_dir () )
        print ( self.temp_dir () )
        print ( self.config_file () )
        print ( self.mount_point () )
        print ( str ( self.project () ) )
        print ( str ( self.ngc_file () ) )
        print ( str ( self.kart_file () ) )
        print ( self.mount_app () )
        print ( self.config_app () )
        print ( self.dump_app () )
        print ( self.umount_app () )
        print ( str ( self.create_mount_point () ) )
