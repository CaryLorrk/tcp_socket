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
    Hostid this_host_;
    std::vector<std::string> hosts_;
    std::vector<FileDesc> sockfds_; 
    std::map<std::string, Hostid> ip_to_host_;
    std::map<FileDesc, Hostid> sockfd_to_host_;

    // init
    void init_ip_to_host();
    void server_for_connections_func();
    FileDesc bind_for_connections();
    void listen_for_connections(FileDesc server_sockfd);
	void accept_for_connections(FileDesc server_sockfd);
	void client_for_connections();

    // receiver
    std::thread receiver_thread;
    void receiver_func();
	void set_host_events(FileDesc epollfd, std::vector<epoll_event>& host_events); 
    size_t read_msg_size(FileDesc sockfd);
    Command read_msg(FileDesc sockfd, size_t msg_size);
    void dispatch(Hostid host, Bytes byts);

    // sender
    struct SendData {
        Hostid host;
        Bytes msgbytes;
        SendData() = default;
        SendData(Hostid in_host, Bytes&& in_msgbytes): 
            host(in_host),
            msgbytes(std::move(in_msgbytes)) {}
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
    void cmd_handler(Hostid host, Bytes& bytes);
    std::vector<int> cmd_cnt_;
    std::vector<std::mutex> cmd_mu_;
    std::vector<std::condition_variable> cmd_cv_;

    // finish
    void finish_handler();
    unsigned finish_cnt_ = 0;
    std::mutex finish_mu_;
    std::condition_variable finish_cv_;
    
};


#endif /* end of include guard: COMM_H_EIKGSU7Y */
