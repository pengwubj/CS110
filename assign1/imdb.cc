#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "imdb.h"

#include <string.h>
#include <algorithm>
#include <iterator>
using namespace std;

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";
imdb::imdb(const string& directory) {
	const string actorFileName = directory + "/" + kActorFileName;
	const string movieFileName = directory + "/" + kMovieFileName;  
	actorFile = acquireFileMap(actorFileName, actorInfo);
	movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const {
	return !( (actorInfo.fd == -1) || 
			(movieInfo.fd == -1) ); 
}

imdb::~imdb() {
	releaseFileMap(actorInfo);
	releaseFileMap(movieInfo);
}

// template<class ForwardIt, class T, class Compare=std::less<> >
// forwardIt binary_find(ForwardIt first, ForwardIt last, const T& value, Compare comp={})
// {
// Note: BOTH type T and the type after ForwardIt is dereferenced 
// must be implicitly convertible to BOTH Type1 and Type2, used in Compare. 
// This is stricter than lower_bound requirement (see above)

bool imdb::getCredits(const string& player, vector<film>& films) const { 
	// find the total actor number
	int total_actor_num = *(int*) actorFile;
//	cout << "actor num: " << total_actor_num << endl;
	int* actor_base_ptr = ((int*) actorFile) + 1;

	// binary find the target actor
	auto cmp1 = [=](const int offset, const string cstr) {
		char* value = ((char*) actorFile) + offset; 
		return value < cstr;	
	};
	auto equals = [=](const int offset, const string cstr) {
		char* value = ((char*) actorFile) + offset; 
		return value == cstr;	
	};
	auto first = std::lower_bound(actor_base_ptr, actor_base_ptr + total_actor_num, player, cmp1);
	auto it = (first != (actor_base_ptr + total_actor_num) && equals(*first, player)) ? first : (actor_base_ptr + total_actor_num);

	if(it != actor_base_ptr + total_actor_num){
		// calculate the actor's index
		int actor_idx = it - actor_base_ptr;
//		cout << "relative pos: " << actor_idx << endl;
//		cout << *it << endl;
		int offset = *(actor_base_ptr + actor_idx);

		// find the actor's name
		char* record_base_ptr = ((char*) actorFile) + offset;
//		cout << "actor: " << record_base_ptr << endl;
		int name_offset = strlen(record_base_ptr);
		if(name_offset % 2 == 0) {
			name_offset += 2;
		} else {
			name_offset += 1;
		}

		// find the actor's movie num
		short movie_num = *(short*)(record_base_ptr + name_offset);
		int name_num_offset = name_offset + 2;
		if(name_num_offset % 4 == 2) {
			name_num_offset += 2;
		}

		// find the actor's movies
		int* movie_base_ptr = (int*)(record_base_ptr + name_num_offset);
		for(int i = 0; i < movie_num; i++) {
			int movie_offset = *(movie_base_ptr + i);
			char* movie_name = ((char*) movieFile) + movie_offset;
//			cout << "movie name: " << movie_name << endl;
			int movie_name_offset = strlen(movie_name) + 1;
//			cout << "movie_name_offset: " << movie_name_offset << "!!!" << endl;
			int movie_year = ((int)*(unsigned char*)(movie_name + movie_name_offset)) + 1900;
//			cout << "movie year: " << movie_year << endl;

			// push into the vector
			film temp_film;
			temp_film.title = movie_name;
			temp_film.year = movie_year;
			films.push_back(temp_film);
		}
		return true;
	}
	return false;
}

bool imdb::getCast(const film& movie, vector<string>& players) const { 
	// find the total movie num
	int total_movie_num = *((int*) movieFile);
//	cout << "total movie num: " << total_movie_num << endl;
	int* movie_base_ptr = ((int*) movieFile) + 1;

	// binary find the target movie
	auto cmp1 = [=](const int offset, const film fi) {
		char* movie_name = ((char*) movieFile) + offset; 
		unsigned movie_name_offset = strlen(movie_name) + 1;
		unsigned movie_year = ((unsigned)*(unsigned char*)(movie_name + movie_name_offset)) + 1900;	
		film film_temp;
		film_temp.title = movie_name;
		film_temp.year = movie_year;
		return film_temp < fi;	
	};
	auto equals = [=](const int offset, const film fi) {
		char* movie_name = ((char*) movieFile) + offset; 
		unsigned movie_name_offset = strlen(movie_name) + 1;
		unsigned movie_year = ((unsigned)*(unsigned char*)(movie_name + movie_name_offset)) + 1900;	
		film film_temp;
		film_temp.title = movie_name;
		film_temp.year = movie_year;
		return film_temp == fi;	
	};
	auto first = std::lower_bound(movie_base_ptr, movie_base_ptr + total_movie_num, movie, cmp1);
	auto it = (first != (movie_base_ptr + total_movie_num) && equals(*first, movie)) ? first : (movie_base_ptr + total_movie_num);
	
	if(it != movie_base_ptr + total_movie_num){
		// calculate the movie's index
		int movie_idx = it - movie_base_ptr;
//		cout << "relative pos: " << movie_idx << endl;
//		cout << *it << endl;
		int offset = *(movie_base_ptr + movie_idx);

		// find the movie's name
		char* record_base_ptr = ((char*) movieFile) + offset;
//		cout << "movie: " << record_base_ptr << endl;
		int name_offset = strlen(record_base_ptr) + 1;
		int name_year_offset = name_offset + 1;
	    if(name_year_offset % 2 == 1) {
			name_year_offset += 1;
		}	

		// find the movie's actor num
		short actor_num = *(short*)(record_base_ptr + name_year_offset);
		int name_year_num_offset = name_year_offset + 2;
		if(name_year_num_offset % 4 == 2) {
			name_year_num_offset += 2;
		}

		// find the movie's actors
		int* actor_base_ptr = (int*)(record_base_ptr + name_year_num_offset);
		for(int i = 0; i < actor_num; i++) {
			int actor_offset = *(actor_base_ptr + i);
			char* actor_name = ((char*) actorFile) + actor_offset;

			// push into the vector
			players.push_back(actor_name);
		}
		return true;
	}
	return false; 
}

const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info) {
	struct stat stats;
	stat(fileName.c_str(), &stats);
	info.fileSize = stats.st_size;
	info.fd = open(fileName.c_str(), O_RDONLY);
	return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info) {
	if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
	if (info.fd != -1) close(info.fd);
}
