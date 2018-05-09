#include "comm.h"

#include <iostream>
#include <cstring>
#include <future>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

static void *get_in_addr(struct sockaddr *sa);

using namespace std::chrono_literals; 
Comm::Comm(Hostid this_host, char* hosts[], int size):
        this_host_(this_host),
        hosts_(hosts, hosts + size),
        sockfds_(size),
        cmd_cnt_(size),
        cmd_mu_(size),
        cmd_cv_(size) {
    init_ip_to_host();
	std::thread server_for_connections_thread(&Comm::server_for_connections_func, this);
	client_for_connections();
	server_for_connections_thread.join();
    receiver_thread = std::thread(&Comm::receiver_func, this);
    sender_thread = std::thread(&Comm::sender_func, this);
	std::this_thread::sleep_for(10ms);
}


void Comm::init_ip_to_host() {
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *hostinfo;
    addrinfo *p;
    int rv;
    char ip[INET6_ADDRSTRLEN];
    for (Hostid host = 0; host < hosts_.size(); ++host) {
        if ((rv = getaddrinfo(hosts_[host].c_str(),
                        PORT, &hints, &hostinfo)) != 0) {
            std::cout << "getaddrinfo: " << gai_strerror(rv) << std::endl;
            exit(1);
        }

        for(p = hostinfo; p != NULL; p = p->ai_next) {
            inet_ntop(p->ai_family,  get_in_addr(p->ai_addr), ip, sizeof(ip));
            ip_to_host_[ip] = host; 
        }
        freeaddrinfo(hostinfo);
    }
}

void Comm::server_for_connections_func() {
    FileDesc server_sockfd = bind_for_connections();
    listen_for_connections(server_sockfd);
    std::cout << "waiting for connections..." << std::endl;
    accept_for_connections(server_sockfd);
    close(server_sockfd);
}


void Comm::accept_for_connections(FileDesc server_sockfd) {
    int client_sockfd;
    socklen_t sin_size;
    sockaddr_storage their_addr;
    char ip[INET6_ADDRSTRLEN];
    for(int cnt = 0; cnt < (int)this_host_; ++cnt) {
        sin_size = sizeof(their_addr);
        client_sockfd = accept(server_sockfd,
                (struct sockaddr *)&their_addr, &sin_size);
        if (client_sockfd == -1) {
            perror("accept");
            exit(1);
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            ip, sizeof(ip));
        Hostid host = ip_to_host_[ip];
        sockfds_[host] = client_sockfd;
        sockfd_to_host_[client_sockfd] = host;

        std::cout << "got connection from " << ip << std::endl;

    }
}

void Comm::client_for_connections() {
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    FileDesc sockfd;
    int rv;
    addrinfo *serverinfo;
    addrinfo *p;
    for (Hostid host = this_host_ + 1; host < hosts_.size(); ++host) {
        std::string hostname = hosts_[ host ];
        if ((rv = getaddrinfo(hostname.c_str(),
                        PORT, &hints, &serverinfo)) != 0) {
            std::cout << "getaddrinfo: " << gai_strerror(rv) << std::endl;
            exit(1);
        }

        int yes=1;
        while (true) {
            for(p = serverinfo; p != NULL; p = p->ai_next) {
                if ((sockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
                    perror("socket");
                    continue;
                }
                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                        sizeof(int)) == -1) {
					perror("setsockopt");
					exit(1);
				}

                if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                    perror("connect");
                    close(sockfd);
                    continue;
                }
                break;
            }
            if (p == NULL) {
                std::cout << "failed to connect to " << hostname << std::endl;
                std::this_thread::sleep_for(1s);

            } else {
                std::cout << "connect to " << hostname << std::endl;
                break;
            }
        }
        sockfds_[host] = sockfd;
        sockfd_to_host_[sockfd] = host;
        freeaddrinfo(serverinfo);
    }
}


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

FileDesc Comm::bind_for_connections() {
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo *serverinfo;
    int rv;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &serverinfo)) != 0) {
        std::cout << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        exit(1);
    }

    FileDesc sockfd;
    addrinfo *p;
    for(p = serverinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind");
            continue;
        }
        break;
    }

    if (p == NULL)  {
        std::cout << "failed to bind" << std::endl;
        exit(1);
    }

    freeaddrinfo(serverinfo); // all done with this structure
    return sockfd;
}

void Comm::listen_for_connections(FileDesc server_sockfd) {
    if (listen(server_sockfd, hosts_.size()) == -1) {
        perror("listen");
        exit(1);
    }
}

void Comm::sender_func() {
    while(true) {
        SendData data;
        {
            std::unique_lock<std::mutex> lock(sender_mu_);
            sender_cv_.wait(lock, [this] {
                    return !sender_queue_.empty();
            });
            data = std::move(sender_queue_.front());
            sender_queue_.pop();
        }
        size_t pkt_size = data.msgbytes.size();
        int numbytes = 0;
        do {
            numbytes += send(sockfds_[data.host], data.msgbytes.data() + numbytes, pkt_size - numbytes, 0);
            if (numbytes < 0) {
                perror("send");
                exit(1);
            }

        } while((size_t)numbytes < pkt_size);
    }
}

void Comm::send_message(Hostid host, Bytes&& msgbytes) {
    {
        std::lock_guard<std::mutex> lock(sender_mu_);
        sender_queue_.emplace(host, std::move(msgbytes));
    }
    sender_cv_.notify_one();
}

void Comm::receiver_func() {
    FileDesc epollfd = epoll_create1(0);
    if (epollfd < 0) {
        perror("epoll_create1");
        exit(1);
    }

    std::vector<epoll_event> host_events(hosts_.size());
    set_host_events(epollfd, host_events);

    int maxevent = hosts_.size();
    std::vector<epoll_event> avail_events(maxevent);
    while(true) {
        int nfds = epoll_wait(epollfd, avail_events.data(), maxevent, -1);
        if (nfds < 0) {
            perror("epoll_wait");
            exit(1);
        }

        for (int n = 0; n < nfds; ++n) {
            FileDesc sockfd = avail_events[n].data.fd;
            size_t msg_size = read_msg_size(sockfd);
            read_msg(sockfd, msg_size);
        }
    }
}

void Comm::set_host_events(FileDesc epollfd, std::vector<epoll_event>& host_events) {
    int rv;
    for (Hostid host = 0; host < hosts_.size(); ++host) {
        if (host == this_host_) continue;
        FileDesc sockfd = sockfds_[host];
        epoll_event& ev = host_events[host];
        ev.data.fd = sockfd;
        ev.events = EPOLLIN;
        if ((rv = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev)) < 0) {
            perror("epoll_ctl");
            exit(1);
        }
    }
}


size_t Comm::read_msg_size(FileDesc sockfd) {
    Byte buffer[sizeof(MsgSize)];
    memset(buffer, '\0', sizeof(MsgSize));
    int numbytes;
    do {
        numbytes = recv(sockfd, buffer, sizeof(MsgSize), MSG_PEEK);
        if (numbytes < 0) {
            perror("recv");
            exit(1);

        } else if (numbytes == 0) {
            std::cout << hosts_[ sockfd_to_host_[sockfd] ] << " disconncted" << std::endl;
            exit(0);
        }
    } while(numbytes < 4);
    return *(MsgSize*)&buffer;
}

void Comm::read_msg(FileDesc sockfd, size_t msg_size) {
    Hostid host = sockfd_to_host_[sockfd];
    Bytes msgbytes(msg_size, 0);
    
    int numbytes = 0;
    do {
        numbytes += recv(sockfd, &msgbytes[0] + numbytes, msg_size - numbytes, 0);
        if(numbytes < 0){
            perror("recv");
            exit(1);

        } else if (numbytes == 0) {
            std::cout << hosts_[ host ] << " disconncted" << std::endl;
            exit(0);

        }
    } while((size_t)numbytes < msg_size);
    auto dummy = std::async(std::launch::async, &Comm::dispatch, this, host, std::move(msgbytes));
    //std::thread(&Comm::dispatch, this, host, std::move(msgbytes)).detach();
}
static Bytes serialize(Comm::Command cmd, const Bytes& bytes) {
    Bytes msgbytes(sizeof(MsgSize) + sizeof(Comm::Command) + bytes.size(), 0);
    MsgSize msg_size = msgbytes.size();
    std::copy((Byte*)&msg_size, (Byte*)(&msg_size + 1), msgbytes.begin());
    std::copy((Byte*)&cmd, (Byte*)(&cmd + 1), msgbytes.begin() + sizeof(MsgSize));
    std::copy(bytes.begin(), bytes.end(),
            msgbytes.begin() + sizeof(MsgSize) + sizeof(Comm::Command));
    return msgbytes;
}

static void deserialize(const Bytes& msgbytes, Comm::Command& cmd, Bytes& bytes) {
    cmd = *reinterpret_cast<const Comm::Command*>(&msgbytes[sizeof(MsgSize)]);
    bytes.append(msgbytes.begin() + sizeof(MsgSize) + sizeof(Comm::Command), msgbytes.end());
}

void Comm::dispatch(Hostid host, Bytes msgbytes) {
    Command cmd;
    Bytes bytes;
    deserialize(msgbytes, cmd, bytes);
    switch(cmd) {
    case Command::CMD:
        cmd_handler(host, bytes);
        break;
    default:
        std::cout << "Unknown command." << std::endl;
        exit(1);
    }
}

void Comm::Cmd(Hostid host, Bytes bytes) {
    if (host == this_host_) {
        cmd_handler(host, bytes);
        return;
    }
    send_message(host, serialize(Command::CMD, bytes));
}

void Comm::cmd_handler(Hostid host, Bytes& bytes) {
    {
        std::lock_guard<std::mutex> lock(cmd_mu_[host]);
        cmd_cnt_[host]++;
    }
    cmd_cv_[host].notify_all();
}

void Comm::Sync(int cnt) {
    for(Hostid host = 0; host < hosts_.size(); ++host) {
        std::unique_lock<std::mutex> lock(cmd_mu_[host]);
        cmd_cv_[host].wait(lock, [this, host, cnt]() {
            return cmd_cnt_[host] >= cnt;
        });
        cmd_cnt_[host] = 0;
    }
}
