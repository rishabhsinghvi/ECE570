#include<iostream>
#include<string>
#include<cstdlib>
#include<fstream>
#include<vector>
#include<queue>
#include<cmath>
#include<algorithm>
#include "thread.h"


// Request contains the track number as well as the id of the thread making the request
struct Request
{
    unsigned int threadID;
    int trackNumber;

    Request(unsigned int id, int  trackNum)
    {
        threadID = id;
        trackNumber = trackNum;
    }
};

// Declare mutexes
unsigned int requestAccessMutex = 0;
unsigned int outputStreamMutex = 1;

// Declare condition variables
unsigned int queueFull = 0;
unsigned int queueAvailable = 1;


std::vector<std::string> fileNames;
std::vector<bool> requestServiced; // if set to true, it means that requester can put on a new task
std::vector<Request> requestList; // all pending requests

unsigned int numThreads;
unsigned int maxDiskRequests; // max number of requests servicer can take
unsigned int currentTrack;  // current track of the servicer




bool SSTF(const Request& a, const Request& b)
{
    return std::abs(a.trackNumber - currentTrack) < std::abs(b.trackNumber - currentTrack);
}

void requester(void* arg)
{
    int requesterId = *(int*)arg;

    std::ifstream file(fileNames[requesterId].c_str());

    unsigned int requesterTrack;

    std::queue<int> requestQueue;

    // Store all requests for the current requester thread
    while(file >> requesterTrack)
    {
        requestQueue.push(requesterTrack);
    }

    while(!requestQueue.empty())
    {
        thread_lock(requestAccessMutex);

        // Wait until this thread's previous request has been fulfilled
        while(!requestServiced.at(requesterId))
        {
            thread_wait(requestAccessMutex, queueAvailable);
        }

        // 
        Request r(requesterId, requestQueue.front());
        requestQueue.pop();
        
        thread_lock(outputStreamMutex);
        std::cout << "requester " << r.threadID << " track " << r.trackNumber << std::endl;
        thread_unlock(outputStreamMutex);

        requestList.push_back(r);
        
        requestServiced[requesterId] = false;

        thread_broadcast(requestAccessMutex, queueFull);

        thread_unlock(requestAccessMutex);
    }
}




void servicer(void* arg)
{
    thread_lock(requestAccessMutex);

    // Wait until we get the max number of requests
    while(requestList.size() < maxDiskRequests)
    {
        thread_wait(requestAccessMutex, queueFull);
    }

    // Sort the tracks
    std::sort(requestList.begin(), requestList.end(), SSTF);
    
    // Handle request
    Request request = requestList.front();
    requestList.erase(requestList.begin());

    thread_lock(outputStreamMutex);
    std::cout << "service requester " << request.threadID << " track " << request.trackNumber << std::endl;
    thread_unlock(outputStreamMutex);

    requestServiced[request.threadID] = true;
    currentTrack = request.trackNumber;

    thread_unlock(requestAccessMutex);

}

void startDiskScheduling(void* arg)
{
    // create servicer thread
    thread_create(servicer, NULL);

    // create requester threads

    for(unsigned int i = 0 ; i < numThreads; i++)
    {
        thread_create(requester, (void*)&i);
    }

}



int main(int argc, char** argv)
{
    maxDiskRequests = atoi(argv[1]);
    numThreads = argc - 2;
    currentTrack = 0;
    
    for(unsigned int i = 0; i < numThreads; i++)
    {
        requestServiced.push_back(true);    
    }

    for(unsigned int i = 2; i < argc; i++)
    {
        fileNames.push_back(std::string(argv[i]));
    }

    thread_libinit(startDiskScheduling, NULL);

}

