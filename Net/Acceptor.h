#ifndef XOP_ACCEPTOR_H
#define XOP_ACCEPTOR_H

#include <functional>
#include <memory>
#include "Channel.h"
#include "TcpSocket.h"

namespace xop
{
//定义新tcp连接到来时的回调函数
typedef std::function<void(int)> NewConnectionCallback;

class EventLoop;

class Acceptor
{
public:	
    Acceptor(EventLoop* eventLoop, std::string ip, uint16_t port);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    { _newConnectionCallback = cb; }

    void setNewConnectionCallback(NewConnectionCallback&& cb)
    { _newConnectionCallback = cb; }

    int listen();

private:
    void handleAccept();

    EventLoop* _eventLoop = nullptr;

    std::shared_ptr<TcpSocket> _tcpSocket;
    ChannelPtr _acceptChannel;

    NewConnectionCallback _newConnectionCallback;
};

}

#endif  //


