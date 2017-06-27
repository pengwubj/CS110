/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */
#include "thread-pool.h"
#include <iostream>
#include "ostreamlock.h"
#include "thread-utils.h"
using namespace std;

// constructor
ThreadPool::ThreadPool(size_t numThreads) : wts(numThreads), workerNum(numThreads), execNum(0), sd(0), dw_wr(numThreads), dw_rd(0) {
	dt = thread([this]() { dispatcher(); }); 
	for (size_t workerID = 0; workerID < numThreads; workerID++) {
		wts[workerID] = thread([this](size_t workerID) { worker(workerID); }, workerID); 
	}
}

// schedule
void ThreadPool::schedule(const function<void(void)>& thunk) {
	v_lock.lock();
	execNum++;
	v_lock.unlock();
	q_lock.lock();
	q1.push(thunk);
	q_lock.unlock();
	sd.signal();
}

// wait
void ThreadPool::wait() {
	unique_lock<mutex> ul(v_lock);
	cv.wait(ul, [this]{ return execNum == 0; });
}

// dispatcher
void ThreadPool::dispatcher() {
	while(true) {
		sd.wait();
		dw_wr.wait();
		q_lock.lock();
		if(q1.empty()) {
			q_lock.unlock();
			break;
		} else {
			q2.push(q1.front());
			q1.pop();
			q_lock.unlock();
			dw_rd.signal();
		}
	}
}

// worker
void ThreadPool::worker(size_t workerID) {
	while(true) {
		dw_rd.wait();
		q_lock.lock();
		if(q2.empty()) {
			q_lock.unlock();
			break;
		} else {
			const function<void(void)> thunk = q2.front();
			q2.pop();
			q_lock.unlock();
			thunk();	// execute the function
			dw_wr.signal();
			v_lock.lock();
			execNum--;	// decrement total function to be excuted
			v_lock.unlock();
			cv.notify_all();	// try to awake wait
		}
	}
}

// destructor
ThreadPool::~ThreadPool() {
	while(true) { 
		if(q1.empty()) {
			sd.signal(); 
			break;
		}
	}
	while(true) {
		if(q2.empty()) {
			for (size_t workerID = 0; workerID < workerNum; workerID++) {
				dw_rd.signal(); 
			}
			break;
		}
	}
	dt.join();	// reclaim dispatcher
	for(size_t workerID = 0; workerID < workerNum; workerID++) { wts[workerID].join(); }	// reclaim workers
}
