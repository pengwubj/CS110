/**
 * File: mr-random.h
 * -----------------
 * Module providing trivial routines that can be shared by
 * word-count-mapper, word-count-reducer, and other map
 * and reduce executables.  They're useful for emulating
 * random delay times so that there's some variability in
 * how all of the various worker nodes contribute to the
 * entire MapReduce runtime.
 */
#pragma once
#include <cstdlib>

/**
 * Function: sleepRandomAmount
 * ---------------------------
 * Prompts the calling thread to sleep for between
 * low and high seconds, inclusive.  The provided
 * defaults of kMinSleepTime and kMaxTimeSleep are
 * small enough that they don't stall the MapReduce
 * workers so much that it's painfully slow, but it
 * does allow for some varying scheduling patterns
 * from run to run.
 */
static const size_t kMinSleepTime = 1;
static const size_t kMaxSleepTime = 5;
void sleepRandomAmount(size_t low = kMinSleepTime,
                       size_t high = kMaxSleepTime);

/**
 * Function: randomChance
 * ----------------------
 * Returns true with the provided probability.
 */
bool randomChance(double probability);
