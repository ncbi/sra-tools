import platform.base

class Platform(platform.base.Base):

    def get_os ( self ):
        return "Win"   

    def input_data_dir ( self ):
        return "\\\\panfs\\pan1\\trace_work\\iskhakov\\IMPORTANT_DIR"

    def umount_app ( self ):
        return "c:\\\\ProgramFiles (x86)\\Dokan\\DokanLibrary\\dokanctl.exe /u"

    def mount_point ( self ):
        return "r:"

    def mount_app ( self ):
        return self.app_dir ()   \
                    + self.separator () \
                    + "dbgap-mount-tool.exe"

    def config_app ( self ):
        return self.app_dir ()   \
                    + self.separator () \
                    + "vdb-config.exe"

    def dump_app ( self ):
        return self.app_dir ()   \
                    + self.separator () \
                    + "fastq-dump-ngs.exe"

    def create_mount_point ( self ):
        return False
