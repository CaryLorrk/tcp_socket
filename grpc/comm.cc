#include "comm.h"

#include <iostream>
#include <cstring>
#include <future>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "comm.grpc.pb.h"

using namespace std::chrono_literals; 
Comm::Comm(Hostid this_host, char* hosts[], int size):
        this_host_(this_host),
        hosts_(hosts, hosts + size),
        cmd_cnt_(size),
        cmd_mu_(size),
        cmd_cv_(size) {
    build_grpc_server();
    create_stubs();
    create_streams();
    sender_thread = std::thread(&Comm::sender_func, this);
	std::this_thread::sleep_for(10ms);
}

void Comm::build_grpc_server() {
    grpc::ServerBuilder builder;
    builder.SetMaxMessageSize(MAX_MESSAGE_SIZE);
    builder.AddListeningPort(std::string("0.0.0.0:")+PORT, grpc::InsecureServerCredentials());
    service_ = std::make_unique<CommServer>(*this);
    builder.RegisterService(service_.get());
    rpc_server_ = builder.BuildAndStart();
    server_thread_ = std::thread(&Comm::server_func, this);
    std::this_thread::sleep_for(10ms);
}

void Comm::create_stubs() {
    std::cout << "Check servers:" << std::endl;  
    grpc::ChannelArguments channel_args;
    channel_args.SetInt("grpc.max_message_length", MAX_MESSAGE_SIZE);
    for (auto&& host: hosts_) {
        while(1) {
            auto stub = rpc::Comm::NewStub(grpc::CreateCustomChannel(
                            host+":"+PORT,
                            grpc::InsecureChannelCredentials(),
                            channel_args));
            grpc::ClientContext ctx;
            rpc::CheckAliveRequest req;
            rpc::CheckAliveResponse res;
            stub->CheckAlive(&ctx, req, &res);
            if (res.status()) {
                std::cout << host << " is up." << std::endl;
                stubs_.push_back(std::move(stub));
                break;
            } else {
                std::cout << "Failed to connect to " << host <<"." << std::endl;
                std::this_thread::sleep_for(1s);
            }
        }
    }

}

void Comm::create_streams() {
    cmd_streams_mu_ = std::make_unique<std::mutex[]>(hosts_.size());
    for (Hostid server = 0; server < hosts_.size(); server++) {
        auto cmd_ctx = std::make_unique<grpc::ClientContext>();
        cmd_ctx->AddMetadata("from_host", std::to_string(this_host_));
        cmd_streams_.push_back(stubs_[server]->Cmd(cmd_ctx.get()));
        cmd_ctxs_.push_back(std::move(cmd_ctx));
    }
}

void Comm::server_func() {
    rpc_server_->Wait();
}

void Comm::sender_func() {
    while(true) {
        SendData data;
        {
            std::unique_lock<std::mutex> lock(sender_mu_);
            sender_cv_.wait(lock, [this] {
                    return sender_end_ || !sender_queue_.empty();
            });
            if (sender_end_ && sender_queue_.empty()) return;
            data = std::move(sender_queue_.front());
            sender_queue_.pop();
        }

        rpc::CmdRequest req;
        req.set_data(std::move(data.bytes));
        std::lock_guard<std::mutex> lock(cmd_streams_mu_[data.host]);
        cmd_streams_[data.host]->Write(req);
    }
}

void Comm::send_message(Hostid host, Bytes&& bytes) {
    {
        std::lock_guard<std::mutex> lock(sender_mu_);
        sender_queue_.emplace(host, std::move(bytes));
    }
    sender_cv_.notify_one();
}

void Comm::send_finish() {
    {
        std::lock_guard<std::mutex> lock(sender_mu_);
        sender_end_ = true;
    }
    sender_cv_.notify_one();
}

void Comm::Cmd(Hostid host, Bytes bytes) {
    if (host == this_host_) {
        cmd_handler(host, bytes);
        return;
    }

    rpc::CmdRequest req;
    req.set_data(std::move(bytes));
    std::lock_guard<std::mutex> lock(cmd_streams_mu_[host]);
    cmd_streams_[host]->Write(req);
    //send_message(host, std::move(bytes));
}

void Comm::cmd_handler(Hostid host, [[gnu::unused]] const Bytes& bytes) {
    {
        std::lock_guard<std::mutex> lock(cmd_mu_[host]);
        cmd_cnt_[host]++;
    }
    cmd_cv_[host].notify_all();
}


void Comm::Finish() {
    send_finish();
    finish_handler();
    for (Hostid host = 0; host < hosts_.size(); ++host) {
        if (host != this_host_) {
            grpc::ClientContext ctx;
            rpc::FinishRequest req;
            rpc::FinishResponse res;
            stubs_[host]->Finish(&ctx, req, &res);
        }
    }

    std::unique_lock<std::mutex> lock(finish_mu_);
    finish_cv_.wait(lock, [this]() {
        return finish_cnt_ >= hosts_.size();
    });
    std::chrono::system_clock::time_point deadline = 
        std::chrono::system_clock::now() + 1ms;
    rpc_server_->Shutdown(deadline);
    sender_thread.join();
    server_thread_.join();
}

void Comm::finish_handler() {
    {
        std::unique_lock<std::mutex> lock(finish_mu_);
        finish_cnt_++;
    }
    finish_cv_.notify_one();

}

void Comm::Sync(int cnt) {
    for(Hostid host = 0; host < hosts_.size(); ++host) {
        std::unique_lock<std::mutex> lock(cmd_mu_[host]);
        cmd_cv_[host].wait(lock, [this, host, cnt]() {
            return cmd_cnt_[host] >= cnt;
        });
        cmd_cnt_[host] -= cnt;
    }
}
