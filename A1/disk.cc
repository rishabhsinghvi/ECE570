#include<iostream>
#include<string>
#include<cstdlib>
#include<fstream>
#include<vector>
#include<queue>
#include<algorithm>
#include<stdint.h>

#include "thread.h"
#include "interrupt.h"


struct Request
{
    unsigned int threadID;
    int trackNumber;

    Request(unsigned int tID, int tN)
    {
        threadID = tID;
        trackNumber = tN;
        //fulfilled = false;
    }
};


unsigned int requestMutex = 0;

unsigned int queueFree = 100;
unsigned int queueFull = 101;


std::vector<std::string> fileNames;
std::vector<Request> requests;

unsigned int numThreads;
unsigned int maxDiskRequests;
unsigned int currentTrack = 0;


unsigned int getClosestTrack()
{
    unsigned int index = 0;
    int min = 10000000;

    for(unsigned int i = 0 ; i < requests.size(); i++)
    {
        
        int diff = ::abs(requests[i].trackNumber - currentTrack);
        if(diff < min)
        {
            //std::cout << "Track: " << requests[i].trackNumber << ", Diff: " << diff << ", min: " << min << std::endl;
            min = diff;
            index = i;
        }
        
    }

    return index;
}

void requester(void* arg)
{
    int id = (intptr_t)arg;

    std::queue<int> threadQueue;

    std::ifstream file(fileNames[id].c_str());

    if(!file)
    {
        return;
    }


    int x;

    while (file >> x)
    {
        threadQueue.push(x);
    }

    thread_lock(requestMutex);

    while(!threadQueue.empty())
    {
        while(requests.size() >= maxDiskRequests)
        {
            thread_wait(requestMutex, queueFree);
            //thread_lock(requestMutex);
        }

        Request request(id, threadQueue.front());
        threadQueue.pop();

        requests.push_back(request);

        thread_signal(requestMutex, queueFull);

        std::cout << "requester " << request.threadID << " track "<< request.trackNumber << std::endl;
        //std::cout << "Number of threads remaining: " << numThreads << '\n';

        //if(!threadQueue.empty())
        thread_wait(requestMutex, id);
    }

    //thread_wait(requestMutex, id);
    
    if(threadQueue.empty())
    {
        numThreads -= 1;
        if(numThreads < maxDiskRequests)
        {
            maxDiskRequests = numThreads;
            thread_signal(requestMutex, queueFull);
        }
    }
    

    thread_unlock(requestMutex);

}


void servicer(void* arg)
{
    thread_lock(requestMutex);

    while(requests.size() > 0  && numThreads > 0)
    {
        while(requests.size() < maxDiskRequests)
        {
            thread_wait(requestMutex, queueFull);
            //thread_lock(requestMutex);
        }

        //thread_lock(requestMutex);
        //std::cout << "Max disk requests : "<< maxDiskRequests << '\n';
        
        // std::cout << "-----------\n"; 
        // std::cout << "Max disk requests : " << maxDiskRequests << ", number of requests in queue: " << requests.size() << ", number of threads remaining: " << numThreads << '\n';
        // std::cout << "-----------\n";

        unsigned int index = getClosestTrack();

        Request r = requests[index];
        //r.fulfilled = true;

        requests.erase(requests.begin() + index);

        thread_broadcast(requestMutex, queueFree);


        std::cout << "service requester " << r.threadID << " track " << r.trackNumber << std::endl;

        // std::cout << "-----------\n"; 
        // std::cout << "Max disk requests : " << maxDiskRequests << ", number of requests in queue: " << requests.size() << ", number of threads remaining: " << numThreads << '\n';
        // std::cout << "-----------\n";

        currentTrack = r.trackNumber;

        thread_signal(requestMutex, r.threadID);

    }

    thread_unlock(requestMutex);
}






void startDiskScheduling(void* arg)
{
    for(unsigned int i = 0; i < numThreads; i++)
    {
        thread_create(requester, (void*)i);
    }

    thread_create(servicer, NULL);
}






int main(int argc, char** argv)
{
    maxDiskRequests = std::atoi(argv[1]);
    numThreads = argc - 2;
    maxDiskRequests = std::min(maxDiskRequests, numThreads);

    for(int i = 2; i < argc; i++)
    {
        fileNames.push_back(std::string(argv[i]));
    }

    thread_libinit(startDiskScheduling, NULL);
}