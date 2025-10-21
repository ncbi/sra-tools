import re
import sys

num_arguments = len(sys.argv)
print(f"grep.py: Total number of command-line arguments: {num_arguments}")
print("grep.py: sys.argv[1]='" + sys.argv[1] + "'")
found = 0
version = ""

for line in sys.stdin:
  line = line.rstrip()
  if line != "":
#   print(">>" + line + "<<")
    version=line
    x = re.search(sys.argv[1], line)
#   print(x)
    if x is not None:
#     print("not found")
#   else:
#     print("found")
      found = 1
#   print

if not found:
#  print("FOUND")
#else:
   print("bad version '" + version + "' (expected " + sys.argv[1] + ")")
   exit(1)
