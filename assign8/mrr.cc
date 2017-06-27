/**
 * File: mrr.cc
 * ------------
 * Provides the entry point into the worker that's
 * operating as a reducer.  Inspect the documentation
 * of the MapReduceReducer class for more information
 * as to how this works.
 */

#include "mapreduce-reducer.h"
static const int kExpectedArgumentCount = 7;
int main(int argc, char *argv[]) {
  if (argc != kExpectedArgumentCount) return 1;
  MapReduceReducer mrr(/* serverHost = */ argv[1],
                       /* serverPort = */ atoi(argv[2]),
                       /* cwd = */ argv[3],
                       /* mapper = */ argv[4],
					   /* inputPath = */ argv[5],
                       /* outputPath = */ argv[6]);
  mrr.reduce();
  return 0;
}
