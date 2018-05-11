#ifndef COMM_SERVER_H_UHRYITXV
#define COMM_SERVER_H_UHRYITXV

#include "comm.grpc.pb.h"

class Comm;
class CommServer final: public rpc::Comm::Service
{
public:
    CommServer(Comm& comm);
    grpc::Status CheckAlive(grpc::ServerContext* ctx,
            const rpc::CheckAliveRequest* req, rpc::CheckAliveResponse* res) override;

    grpc::Status Cmd(grpc::ServerContext* ctx,
            grpc::ServerReaderWriter<rpc::CmdResponse, rpc::CmdRequest>* stream) override;

    grpc::Status Finish(grpc::ServerContext* ctx,
            const rpc::FinishRequest* req, rpc::FinishResponse* res) override;
private:
    Comm& comm_;
};

#endif /* end of include guard: COMM_SERVER_H_UHRYITXV */
