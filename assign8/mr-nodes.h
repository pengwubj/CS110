/**
 * File: mr-nodes.h
 * ----------------
 * Exports a single function that can be called to dynamically
 * load the collection of myth machines that are up, working, stable, and
 * ssh'able.
 */

#pragma once
#include <vector>
#include <string>

/**
 * Function: loadMapReduceNodes
 * ----------------------------
 * Surfaces a vector of all of the nodes in the myth cluster
 * that are to be enlisted to run MapReduce workers.
 */

std::vector<std::string> loadMapReduceNodes();
