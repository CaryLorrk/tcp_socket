#include "comm_server.h" 

#include "comm.h"

CommServer::CommServer(Comm& comm): comm_(comm) {}

grpc::Status CommServer::CheckAlive(
        [[gnu::unused]] grpc::ServerContext* ctx,
        [[gnu::unused]] const rpc::CheckAliveRequest* req,
        rpc::CheckAliveResponse* res){
    res->set_status(true);
    return grpc::Status::OK;
}

grpc::Status CommServer::Finish(
        [[gnu::unused]] grpc::ServerContext* ctx,
        [[gnu::unused]] const rpc::FinishRequest* req,
        [[gnu::unused]] rpc::FinishResponse* res) {
    comm_.finish_handler();
    return grpc::Status::OK;
}

grpc::Status CommServer::Cmd(grpc::ServerContext* ctx,
        grpc::ServerReaderWriter<rpc::CmdResponse, rpc::CmdRequest>* stream) {
    Hostid host = std::stoi(ctx->client_metadata().find("from_host")->second.data());
    rpc::CmdRequest req;
    while(stream->Read(&req)) {
        comm_.cmd_handler(host, req.data());
    }
    return grpc::Status::OK;
}
