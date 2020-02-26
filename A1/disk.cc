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

// Function to sort requests in the SSTF manner
bool SSTF(const Request& a, const Request& b)
{
    return std::abs(a.trackNumber - currentTrack) < std::abs(b.trackNumber - currentTrack);
}

// Requester threads
void requester(void* arg)
{
    int requesterId = (int)arg;
    std::ifstream file(fileNames[requesterId].c_str());

    // if file doesn't exist, return
    if(!file)
    {
        return;
    }

    unsigned int requesterTrack;
    std::queue<int> requestQueue;

    // Store all requests for the current requester thread
    while(file >> requesterTrack)
    {
        std::cout << requesterTrack << '\n';
        requestQueue.push(requesterTrack);
    }

    
    thread_lock(requestAccessMutex);

    // Push request onto servicer's queue
    while(!requestQueue.empty())
    {
        // Wait until this thread's previous request has been fulfilled
        while(!requestServiced[requesterId] || requestList.size() >= maxDiskRequests)
        {
            thread_wait(requestAccessMutex, queueAvailable);
        }

        // --------------------------------------------------
        //  Now, we can push the request on to the list
        Request r(requesterId, requestQueue.front());
        requestQueue.pop();
        
        thread_lock(outputStreamMutex);
        std::cout << "requester " << r.threadID << " track " << r.trackNumber << std::endl;
        thread_unlock(outputStreamMutex);

        requestList.push_back(r);
        
        requestServiced[requesterId] = false;

        // --------------------------------------------------------

        thread_broadcast(requestAccessMutex, queueFull);  // signal to servicer that the queue is full 

    }


    // After last request is done, wait until our request is fulfilled
    while(!requestServiced[requesterId])
    {
        thread_wait(requestAccessMutex, queueAvailable);
    }

    numThreads -= 1;
    
    if(numThreads < maxDiskRequests)
    {
        maxDiskRequests = numThreads;
        thread_broadcast(requestAccessMutex, queueFull);
    }

    thread_unlock(requestAccessMutex);
}

// Servicer thread
void servicer(void* arg)
{
    thread_lock(requestAccessMutex);

    // Keep going until no more threads to service
    while(maxDiskRequests > 0)
    {
        
            // If number of requests is less than the max we can service, keep waiting
            while(requestList.size() < maxDiskRequests)
            {
                thread_wait(requestAccessMutex, queueFull);
            }

            // Sort the tracks
            std::sort(requestList.begin(), requestList.end(), SSTF);
    
            // Handle request
            Request request = requestList.front();
            requestList.erase(requestList.begin());

            //thread_lock(outputStreamMutex);
            std::cout << "service requester " << request.threadID << " track " << request.trackNumber << std::endl;
            //thread_unlock(outputStreamMutex);

            requestServiced[request.threadID] = true;
            currentTrack = request.trackNumber;

            // Tell the other threads that the queue is available for adding
            thread_broadcast(requestAccessMutex, queueAvailable);
        
    }

    // Unlock mutex
    thread_unlock(requestAccessMutex);

}


// This thread starts the disk scheduling routine
void startDiskScheduling(void* arg)
{
    // create servicer thread
    thread_create(servicer, NULL);

    // create requester threads
    for(unsigned int i = 0 ; i < numThreads; i++)
    {
        thread_create(requester, (void*)i);
    }
}



int main(int argc, char** argv)
{
    maxDiskRequests = atoi(argv[1]);
    numThreads = argc - 2;

    // Choose the minimum of the maxDiskRequests and numThreads
    maxDiskRequests = std::min(maxDiskRequests, numThreads);

    currentTrack = 0;
    
    for(unsigned int i = 0; i < numThreads; i++)
    {
        requestServiced.push_back(true);    
    }

    for(unsigned int i = 2; i < argc; i++)
    {
        fileNames.push_back(std::string(argv[i]));
    }


    thread_libinit(&startDiskScheduling, NULL);

}

