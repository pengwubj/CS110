/**
 * File: news-aggregator.cc
 * --------------------------------
 * Presents the implementation of the NewsAggregator class.
 */

#include "news-aggregator.h"
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/catalog.h>
// you will almost certainly need to add more system header includes
#include <unordered_map>
#include <assert.h>
#include "thread-pool.h"
#include <mutex>
#include <thread>
#include "semaphore.h"
#include <condition_variable>
// I'm not giving away too much detail here by leaking the #includes below,
// which contribute to the official CS110 staff solution.
#include "rss-feed.h"
#include "rss-feed-list.h"
#include "html-document.h"
#include "html-document-exception.h"
#include "rss-feed-exception.h"
#include "rss-feed-list-exception.h"
#include "utils.h"
#include "ostreamlock.h"
#include "string-utils.h"
using namespace std;

std::mutex NewsAggregator::indexLock;
std::mutex NewsAggregator::urlSetLock;
std::unordered_set<std::string> NewsAggregator::urlSet;

/**
 * Factory Method: createNewsAggregator
 * ------------------------------------
 * Factory method that spends most of its energy parsing the argument vector
 * to decide what rss feed list to process and whether to print lots of
 * of logging information as it does so.
 */
static const string kDefaultRSSFeedListURL = "small-feed.xml";
NewsAggregator *NewsAggregator::createNewsAggregator(int argc, char *argv[]) {
  struct option options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {"url", required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0},
  };
  
  string rssFeedListURI = kDefaultRSSFeedListURL;
  bool verbose = false;
  while (true) {
    int ch = getopt_long(argc, argv, "vqu:", options, NULL);
    if (ch == -1) break;
    switch (ch) {
    case 'v':
      verbose = true;
      break;
    case 'q':
      verbose = false;
      break;
    case 'u':
      rssFeedListURI = optarg;
      break;
    default:
      NewsAggregatorLog::printUsage("Unrecognized flag.", argv[0]);
    }
  }
  
  argc -= optind;
  if (argc > 0) NewsAggregatorLog::printUsage("Too many arguments.", argv[0]);
  return new NewsAggregator(rssFeedListURI, verbose);
}

/**
 * Method: buildIndex
 * ------------------
 * Initalizex the XML parser, processes all feeds, and then
 * cleans up the parser.  The lion's share of the work is passed
 * on to processAllFeeds, which you will need to implement.
 */
void NewsAggregator::buildIndex() {
  if (built) return;
  built = true; // optimistically assume it'll all work out
  xmlInitParser();
  xmlInitializeCatalog();
  processAllFeeds();
  xmlCatalogCleanup();
  xmlCleanupParser();
}

/**
 * Method: queryIndex
 * ------------------
 * Interacts with the user via a custom command line, allowing
 * the user to surface all of the news articles that contains a particular
 * search term.
 */
void NewsAggregator::queryIndex() const {
  static const size_t kMaxMatchesToShow = 15;
  while (true) {
    cout << "Enter a search term [or just hit <enter> to quit]: ";
    string response;
    getline(cin, response);
    response = trim(response);
    if (response.empty()) break;
    const vector<pair<Article, int> >& matches = index.getMatchingArticles(response);
    if (matches.empty()) {
      cout << "Ah, we didn't find the term \"" << response << "\". Try again." << endl;
    } else {
      cout << "That term appears in " << matches.size() << " article"
           << (matches.size() == 1 ? "" : "s") << ".  ";
      if (matches.size() > kMaxMatchesToShow)
        cout << "Here are the top " << kMaxMatchesToShow << " of them:" << endl;
      else if (matches.size() > 1)
        cout << "Here they are:" << endl;
      else
        cout << "Here it is:" << endl;
      size_t count = 0;
      for (const pair<Article, int>& match: matches) {
        if (count == kMaxMatchesToShow) break;
        count++;
        string title = match.first.title;
        if (shouldTruncate(title)) title = truncate(title);
        string url = match.first.url;
        if (shouldTruncate(url)) url = truncate(url);
        string times = match.second == 1 ? "time" : "times";
        cout << "  " << setw(2) << setfill(' ') << count << ".) "
             << "\"" << title << "\" [appears " << match.second << " " << times << "]." << endl;
        cout << "       \"" << url << "\"" << endl;
      }
    }
  }
}

/**
 * Private Constructor: NewsAggregator
 * -----------------------------------
 * Self-explanatory.  You may need to add a few lines of code to
 * initialize any additional fields you add to the private section
 * of the class definition.
 */
NewsAggregator::NewsAggregator(const string& rssFeedListURI, bool verbose): 
  log(verbose), rssFeedListURI(rssFeedListURI), built(false) {}

/**
 * Private Method: processAllFeeds
 * -------------------------------
 * Downloads and parses the encapsulated RSSFeedList, which itself
 * leads to RSSFeeds, which themsleves lead to HTMLDocuemnts, which
 * can be collectively parsed for their tokens to build a huge RSSIndex.
 * 
 * The vast majority of your Assignment 5 work has you implement this
 * method using multithreading while respecting the imposed constraints
 * outlined in the spec.
 */

// void NewsAggregator::processAllFeeds() {}
void NewsAggregator::processAllFeeds() {
	// start to deal with feed list
	RSSFeedList rssFeedList(rssFeedListURI);
	try{
		rssFeedList.parse();
	} catch(const RSSFeedListException& rfle) {
		log.noteFullRSSFeedListDownloadFailureAndExit(rssFeedListURI);
		return;
	}
	log.noteFullRSSFeedListDownloadEnd();
	const auto& feeds = rssFeedList.getFeeds();	// std::map<std::string, std::string> feeds;
	feed2articles(feeds);
	log.noteAllRSSFeedsDownloadEnd();
}

void NewsAggregator::feed2articles(std::map<std::string, std::string> feeds){
	ThreadPool childPool(childNum);
	// start to deal with feed
	for(auto it = feeds.begin(); it != feeds.end(); it++) {
		childPool.schedule([this, it] {
					std::string xmlUrl = it->first;
					std::string xmlTitle = it->second;
					urlSetLock.lock();
					if(urlSet.count(xmlUrl)) {
						log.noteSingleFeedDownloadSkipped(xmlUrl);
						urlSetLock.unlock();
						return;
					} else {
						urlSet.insert(xmlUrl);
						urlSetLock.unlock();
					}
					RSSFeed rssFeed(xmlUrl);
					log.noteSingleFeedDownloadBeginning(xmlTitle);
					try{
						rssFeed.parse();
					} catch(const RSSFeedException& rfe) {
						log.noteSingleFeedDownloadFailure(xmlUrl);
						return;
					}
					log.noteSingleFeedDownloadEnd(xmlUrl);
					const auto& articles = rssFeed.getArticles(); // std::vector<Article>& articles
					article2tokens(articles); // start a new method
					log.noteAllArticlesHaveBeenScheduled(xmlTitle);
					// thread-safe code ends
		});
	}
	childPool.wait();
}


void NewsAggregator::article2tokens(std::vector<Article> articles) {
	ThreadPool grandchildPool(grandchildNum);
	std::unordered_map<std::string, pair<Article, std::vector<std::string> > > titleMap;
	std::mutex titleMapLock;
	// start to deal with html
	for(auto iter = articles.begin(); iter != articles.end(); iter++) {
		grandchildPool.schedule([this, iter, &titleMap, &titleMapLock] {
					// thread-safe code starts
					Article article = *iter;
					std::string htmlUrl = iter->url;
					std::string htmlTitle = iter->title;
					urlSetLock.lock();
					if(urlSet.count(htmlUrl)) {
						log.noteSingleArticleDownloadSkipped(article);
						urlSetLock.unlock();
						return;
					} else {
						urlSet.insert(htmlUrl);
						urlSetLock.unlock();
					}
						HTMLDocument htmlDocument(htmlUrl);
						log.noteSingleArticleDownloadBeginning(article);
					try{
						htmlDocument.parse();
					} catch(const HTMLDocumentException& hde) {
						log.noteSingleArticleDownloadFailure(article);
						return;
					}
					const auto& const_tokens = htmlDocument.getTokens(); // std::vector<std::string> tokens;
					vector<std::string> tokens(const_tokens.begin(), const_tokens.end());
					sort(tokens.begin(), tokens.end());
					assert(is_sorted(tokens.cbegin(), tokens.cend()));
					titleMapLock.lock();
					if(titleMap.count(htmlTitle)) {
						// retrieve old values
						string oldUrl = titleMap[htmlTitle].first.url;
						vector<string> oldTokens = titleMap[htmlTitle].second;
						// populate new values
						string newUrl = oldUrl < htmlUrl ? oldUrl : htmlUrl;
						vector<string> newTokens;
						set_intersection(oldTokens.cbegin(), oldTokens.cend(), tokens.cbegin(), tokens.cend(), back_inserter(newTokens));
						// add to titleMap 
						article.url = newUrl;
						titleMap[htmlTitle] = make_pair(article, newTokens);
						titleMapLock.unlock();
					} else {
						titleMap[htmlTitle] = make_pair(article, tokens);
						titleMapLock.unlock();
					}
					// thread-safe code ends
		});
	}
	grandchildPool.wait();
	for(auto itt = titleMap.begin(); itt != titleMap.end(); itt++) {
		indexLock.lock();
		index.add(itt->second.first, itt->second.second);
		indexLock.unlock();
	}
}
