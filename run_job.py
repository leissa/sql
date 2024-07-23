import subprocess
import os
import glob

print(os.getcwd())


job_files = glob.glob("./job/*.sql")

successes = 0
failures = 0
segfaults = 0

for file in job_files:
    tmp = subprocess.call(f'./build/bin/sql -d {file}', shell=True)
    print(tmp)
    if tmp == 0:
        successes += 1
    elif tmp == 1:
        failures += 1
    elif tmp == 139:
        segfaults += 1

print("Job's done")
print(f'Successes: {successes} Failures: {failures} Segfaults: {segfaults}')