/**
 * File: mr-names.h
 * ----------------
 * Defines some utilities to help create new file names out of old ones.
 */

#pragma once
#include <string>

/**
 * Function: extractBase
 * ---------------------
 * Accepts an absolute or relative file name and returns the last component.
 *
 *   extractBase("/usr/class/cs110/WWW/index.html") returns "index.html"
 *   extractBase("assign1/inode.c") returns "inode.c"
 *   extractBase("alone.txt") returns "alone.txt"
 */
std::string extractBase(const std::string& name);

/**
 * Function: changeExtension
 * -------------------------
 * Provided the supplied name ends in the supplied old extension, changeExtension returns the
 * same name, except that the old extension has been replaced by new one.  If name
 * doesn't end in the old extension, then the name is returned as is without any modifications.
 *
 *   changeExtension("00000.foo", "foo", "bar") returns "00001.bar"
 *   charngExtension("00002.bing", "ring", "finger") return "00002.bing" (since the old extension didn't match)
 *   changeExtension("00004.foobar", "bar", "baz") returns "00004.foobar" (since the old extension didn't match)
 */
std::string changeExtension(const std::string& name, const std::string& oldext, const std::string& newext);

/**
 * Function: numberToString
 * ------------------------
 * Converts the provided number to a string of the given width by
 * padding it with additional leading zeroes if necessary.  The
 * second argument defaults to 5.
 * 
 *   numberToString(15, 5) returns "00015"
 *   numberToString(15) returns "00015"
 *   numberToString(100000, 2) returns "100000"
 */
std::string numberToString(size_t number, size_t width = 5);
