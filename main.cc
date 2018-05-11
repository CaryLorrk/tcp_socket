#include <iostream>


#ifdef GRPC
#include "grpc/comm.h"
#else
#include "socket/comm.h"
#endif

using namespace std::chrono_literals; 
int main(int argc, char *argv[])
{
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
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(600ms);
        for (int c = 0; c < cnt; ++c) {
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
