#!/usr/bin/env python
from itertools import groupby
from operator import itemgetter
import sys
 
def read_mapper_output(file):
    for line in file:
        yield line.strip().split(' ')
 
def main():
    data = read_mapper_output(sys.stdin)
    for key, keygroup in groupby(data, itemgetter(0)):
        values = ' '.join(sorted(v for k, v in keygroup))
        print "%s %s" % (key, values)
 
if __name__ == "__main__":
    main()
