#ifndef COMM_SERIALIZE_H_
#define COMM_SERIALIZE_H_

#include "comm.h"
[[gnu::unused]] static Bytes& operator<<(Bytes& msgbytes, const Bytes& val) {
    return msgbytes.append(val);
}

template<typename T>
Bytes& operator<<(Bytes& msgbytes, T& val) {
    return msgbytes.append((Byte*)&val, (Byte*)(&val + 1));
}

template<typename T>
void append_vals_to_msgbytes(Bytes& msgbytes, T& val) {
    msgbytes << val;
}

template<typename T, typename... Ts>
void append_vals_to_msgbytes(Bytes& msgbytes, T& val, Ts&... ts) {
    msgbytes << val;
    append_vals_to_msgbytes(msgbytes, ts...);
}

template<typename... Ts>
Bytes serialize(Comm::Command cmd, Ts&... ts) {
    Bytes msgbytes(sizeof(MsgSize), 0);
    append_vals_to_msgbytes(msgbytes, cmd, ts...);
    MsgSize msg_size = msgbytes.size();
    std::copy((Byte*)(&msg_size), (Byte*)(&msg_size + 1), msgbytes.begin());
    return msgbytes;
}

[[gnu::unused]] static void deserialize(
        Bytes::const_iterator first,
        Bytes::const_iterator last,
        Bytes& val) {
    val.append(first, last);
}

template<typename T>
void deserialize(
        Bytes::const_iterator first,
         Bytes::const_iterator last,
        T& val) {
    val = *reinterpret_cast<const T*>(&*first);
}

template<typename T, typename... Ts>
void deserialize(
        Bytes::const_iterator first,
        Bytes::const_iterator last,
        T& val, Ts&... ts) {
    val = *reinterpret_cast<const T*>(&*first);
    std::advance(first, sizeof(T));
    deserialize(first, last, ts...);
}

template<typename... Ts>
void deserialize(const Bytes msgbytes, Ts&... ts) {
    auto first = std::next(msgbytes.begin(), sizeof(MsgSize) + sizeof(Comm::Command));
    deserialize(first, msgbytes.end(), ts...);
}

[[gnu::unused]] static Comm::Command deserialize_cmd(const Bytes msgbytes) {
    auto it = std::next(msgbytes.begin(), sizeof(MsgSize));
    return *reinterpret_cast<const Comm::Command*>(&*it);
}

#endif /* end of include guard: COMM_SERIALIZE_H_ */
