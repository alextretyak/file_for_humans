import os, sys

for root, dirs, files in os.walk('.'):
    dirs[:] = [d for d in dirs if d[0] != '.'] # exclude hidden folders (e.g. `.git`)
    for name in files:
        file_bytes = open(os.path.join(root, name), 'rb').read()
        if b"\r" in file_bytes:
            sys.exit(R"\r found in file '" + os.path.join(root, name) + "'")
        if b"\t" in file_bytes:
            sys.exit(R"\t found in file '" + os.path.join(root, name) + "'")
        if b" \n" in file_bytes or b"\t\n" in file_bytes or file_bytes.endswith((b' ', b"\t")):
            sys.exit(R"whitespace at the end of line found in file '" + os.path.join(root, name) + "'")
