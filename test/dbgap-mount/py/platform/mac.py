import platform.base

class Platform(platform.base.Base):

    def get_os ( self ):
        return "Mac"

    def input_data_dir ( self ):
        return "/net/pan1/trace_work/iskhakov/IMPORTANT_DIR"

    def umount_app ( self ):
        return "/sbin/umount"
