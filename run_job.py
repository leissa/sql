import subprocess
import os

print(os.getcwd())

tmp = subprocess.call("./build/bin/sql -d ./test/select.sql", shell=True)
print(tmp)