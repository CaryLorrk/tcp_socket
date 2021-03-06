#include <iostream>


#ifdef GRPC
#include "grpc/comm.h"
#else
#include "socket/comm.h"
#endif

#define ITERATION 10

using namespace std::chrono_literals; 
int main(int argc, char *argv[])
{
    
    std::cout << "the build is compiled at " << __DATE__ << " " << __TIME__ << std::endl;
    if (argc < 5) {
        std::cout << "usage: " << argv[0] << " bytesize cnt this_host_id hosts..." << std::endl;
        exit(1);
    }
    int bytesize = atoi(argv[1]);
    int cnt = atoi(argv[2]);
    int this_host = atoi(argv[3]);
    char **hosts = &argv[4];
    int numhosts = argc - 4;
    std::cout << "bytesize: " << bytesize << " cnt: " << cnt << std::endl;
    Comm comm(this_host, hosts, numhosts);
    Bytes bytes(bytesize, 0);
    for (int i = 0; i < ITERATION; ++i) {
        // dummy work
        std::this_thread::sleep_for(3ms);
        for (int c = 0; c < cnt; ++c) {
            // dummy work
            std::this_thread::sleep_for(1us);
            for(int h = 0; h < numhosts; ++h) {
                comm.Cmd(h, bytes);
            }
        }
        comm.Sync(cnt);
        std::cout << "end of iteration " << i << std::endl;
    }
    comm.Finish();
    
    return 0;
}
