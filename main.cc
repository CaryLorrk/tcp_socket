#include <iostream>

#include "comm.h"

#define BYTESIZE 10000
#define CNT 1000

using namespace std::chrono_literals; 
int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cout << "usage: main this_host_id hosts..." << std::endl;
        exit(1);
    }
    int this_host = atoi(argv[1]);
    char **hosts = &argv[2];
    int numhosts = argc - 2;
    Comm comm(this_host, hosts, numhosts);
    Bytes bytes(BYTESIZE, 0);
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(600ms);
        for (int c = 0; c < CNT; ++c) {
            for(int h = 0; h < numhosts; ++h) {
                comm.Cmd(h, bytes);
            }
        }
        comm.Sync(CNT);
        std::cout << "end of iteration " << i << std::endl;
    }
    
    return 0;
}
