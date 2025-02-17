#!/usr/bin/env python

import subprocess
import os
import glob

print(os.getcwd())

job_files = glob.glob("./job/*.sql")

successes = []
failures = []
segfaults = []

for file in job_files:
    ec = subprocess.call(f'./build/bin/sql -d {file}', shell=True)
    print(ec)
    if ec == 0:
        successes.append(file)
    elif ec == 1:
        failures.append(file)
    else:
        segfaults.append(file)

print("Job's done")
print(f'Successes: {len(successes)} Failures: {len(failures)} Segfaults: {len(segfaults)}')

print("Failed:")
for file in sorted(failures):
    print(file)

print("Segfaults:")
for file in sorted(segfaults):
    print(file)
