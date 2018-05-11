#ifndef COMM_H_EIKGSU7Y
#define COMM_H_EIKGSU7Y

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

#include <sys/epoll.h>

#include <grpc++/grpc++.h>

#include "comm_server.h"

using FileDesc = int;
using Hostid = uint32_t;
using Bytes = std::string;
using Byte = char;
using MsgSize = uint32_t;


class Comm {
public:
    Comm(Hostid this_host, char* hosts[], int size);
    void Cmd(Hostid host, Bytes bytes);
    void Finish();

    enum class Command: uint32_t {
        CMD,
        FINISH,
    };


    void Sync(int cnt);
private:
    static constexpr const char* PORT = "50055";
    static constexpr const int MAX_MESSAGE_SIZE = 100*1024*1024;
    Hostid this_host_;
    std::vector<std::string> hosts_;
    std::vector<FileDesc> sockfds_; 
    std::map<std::string, Hostid> ip_to_host_;
    std::map<FileDesc, Hostid> sockfd_to_host_;

    // init
    void build_grpc_server();
    std::unique_ptr<CommServer> service_;
    std::unique_ptr<grpc::Server> rpc_server_;
    std::thread server_thread_;
    void server_func();

    void create_stubs();
    std::vector<std::unique_ptr<rpc::Comm::Stub>> stubs_;

    void create_streams();
    std::unique_ptr<std::mutex[]> cmd_streams_mu_;
    std::vector<std::unique_ptr<grpc::ClientContext>> cmd_ctxs_;
    std::vector<std::unique_ptr<grpc::ClientReaderWriter<rpc::CmdRequest, rpc::CmdResponse>>> cmd_streams_;



    // sender
    struct SendData {
        Hostid host;
        Bytes bytes;
        SendData() = default;
        SendData(Hostid in_host, Bytes&& in_bytes): 
            host(in_host),
            bytes(std::move(in_bytes)) {}
    };
    std::thread sender_thread;
    void sender_func();
    void send_message(Hostid host, Bytes&& msgbytes);
    std::mutex sender_mu_;
    std::condition_variable sender_cv_;
    std::queue<SendData> sender_queue_;
    void send_finish();
    bool sender_end_ = false;

    // cmd
    void cmd_handler(Hostid host, const Bytes& bytes);
    std::vector<int> cmd_cnt_;
    std::vector<std::mutex> cmd_mu_;
    std::vector<std::condition_variable> cmd_cv_;

    // finish
    void finish_handler();
    unsigned finish_cnt_ = 0;
    std::mutex finish_mu_;
    std::condition_variable finish_cv_;
    
    friend class CommServer;
};


#endif /* end of include guard: COMM_H_EIKGSU7Y */
