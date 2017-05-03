import platform.base

class Platform(platform.base.Base):

    def get_os ( self ):
        return "Lnx"

    def umount_app ( self ):
        return "/bin/fusermount -u"
