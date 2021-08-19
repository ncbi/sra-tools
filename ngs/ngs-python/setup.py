from distutils.core import setup
import sys
#import version

#ver = version.get_git_version()

decimal_ver = sys.version_info[0]*10 + sys.version_info[1]
min_version = 26

if decimal_ver < min_version:
    print ("At least python " + str(min_version/10.) + " is required to automatically install ngs package, and you're using " + str(decimal_ver/10.))
    exit(1)

setup(name='ngs',
      #version=ver,
      version = "1.0",
      author='sra-tools',
      author_email='sra-tools@ncbi.nlm.nih.gov',
      packages=['ngs'],
      include_package_data=True,
      #scripts=[],
      #test_suite="tests",
      )
