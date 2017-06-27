#include <iostream>
#include <utility>
#include <vector>
#include "path.h"
#include <list>
#include <set>
#include <unordered_set>
#include "imdb.h"
using namespace std;

static const int kWrongArgumentCount = 1;
static const int kDatabaseNotFound = 2;

int main(int argc, char *argv[]) {
	if (argc != 3) {
		cerr << "Usage: " << argv[0] << " <source-actor> <target-actor>" << endl;
		return kWrongArgumentCount;
	}
	imdb db(kIMDBDataDirectory);
	if (!db.good()) {
		cerr << "Data directory not found!  Aborting..." << endl; 
		return kDatabaseNotFound;
	}
	string source = argv[1];
	string target = argv[2];

	// create a list for BFS
	list<vector<pair<string, film>>> todo;
	unordered_set<string> visited_actor;
	set<film> visited_movie;

	// put the first element
	film dummy_film;
	dummy_film.title = "zhangrao";
	dummy_film.year = 1990;
	vector<pair<string, film>> temp_vect;
	temp_vect.push_back(make_pair(source, dummy_film));
	todo.push_back(temp_vect);
	visited_actor.insert(source);

	// while loop to find the path
	while(!todo.empty()) {
		vector<pair<string, film>> curr_vect = todo.front();
		pair<string, film> curr_pair = curr_vect[curr_vect.size() - 1];
		string curr_name = curr_pair.first;

		if(curr_name == target) {
			break;
		}
		todo.pop_front();

		vector<film> neighbor_movies;
		if(db.getCredits(curr_name, neighbor_movies)) {
			for(unsigned i = 0; i < neighbor_movies.size(); i++) {
				film neighbor_movie = neighbor_movies[i];
				if(visited_movie.count(neighbor_movie)) {
					continue;
				} else {
					visited_movie.insert(neighbor_movie);
				}
				vector<string> neighbor_actors;
				if(db.getCast(neighbor_movie, neighbor_actors)) {
					for(unsigned j = 0; j < neighbor_actors.size(); j++) {
						string neighbor_actor = neighbor_actors[j];
						if(visited_actor.count(neighbor_actor)) {
							continue;
						} else {// many things to do
							visited_actor.insert(neighbor_actor);
							vector<pair<string, film>> copy = curr_vect;
							pair<string, film> temp_pair = make_pair(neighbor_actor, neighbor_movie);
							copy.push_back(temp_pair);
							todo.push_back(copy);
						}
					}
				}
			}
		}
	}
	if(todo.empty() || todo.front().empty()) {
		cout << "No path between those two people could be found." << endl;
	} else {
		// print the path
		vector<pair<string, film>> vec = todo.front();
		path p(vec[0].first);
		for(unsigned i = 1; i < vec.size(); i++) {
			p.addConnection(vec[i].second, vec[i].first);	
		}
		cout << p;
	}
	return 0;
}

