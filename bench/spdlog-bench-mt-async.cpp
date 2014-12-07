#include <thread>
#include <vector>
#include <atomic>

#include "spdlog/spdlog.h"

#include <iostream>
using namespace std;

int main(int argc, char* argv[])
{

    int thread_count = 10;
    if(argc > 1)
        thread_count = atoi(argv[1]);

    int howmany = 1048576;

    namespace spd = spdlog;
    spd::set_async_mode(howmany);
    ///Create a file rotating logger with 5mb size max and 3 rotated files
    auto logger = spd::rotating_logger_mt("file_logger", "logs/spd-sample", 10 *1024 * 1024 , 5);

    logger->set_pattern("[%Y-%b-%d %T.%e]: %v");

    std::atomic<int > msg_counter {0};
    vector<thread> threads;

    for (int t = 0; t < thread_count; ++t)
    {
        threads.push_back(std::thread([&]()
        {
            while (true)
            {
                int counter = ++msg_counter;
                if (counter > howmany) break;
                logger->info() << "spdlog message #" << counter << ": This is some text for your pleasure";
            }
        }));
    }


    for(auto &t:threads)
    {
        t.join();
    };

	//because spdlog async logger waits for the back thread logger to finish all messages upon destrcuting,
	//and we want to measure only the time it took to push those messages to the backthread..
	abort(); 	    
}