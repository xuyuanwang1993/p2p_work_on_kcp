// PHZ
// 2018-5-15

#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "Logger.h"
#include <cstdio>  

using namespace xop;
using namespace std;

TcpServer::TcpServer(EventLoop* eventLoop, std::string ip, uint16_t port)
    : _eventLoop(eventLoop),
      _acceptor(new Acceptor(eventLoop, ip, port))
{
    _ip = ip;
    _port = port;

    _acceptor->setNewConnectionCallback([this](SOCKET sockfd) { 
        TcpConnection::Ptr tcpConn = this->newConnection(sockfd);
        if (tcpConn)
        {
            this->addConnection(sockfd, tcpConn);
            tcpConn->setDisconnectCallback([this] (TcpConnection::Ptr conn){ 
                    auto taskScheduler = conn->getTaskScheduler();
                    int sockfd = conn->fd();
                    if (!taskScheduler->addTriggerEvent([this, sockfd] {this->removeConnection(sockfd); }))
                    {
                        taskScheduler->addTimer([this, sockfd]() {this->removeConnection(sockfd); return false;}, 1);
                    }
            });
        }
    });
}

TcpServer::~TcpServer()
{
	
}

int TcpServer::start()
{
	return _acceptor->listen();
}

TcpConnection::Ptr TcpServer::newConnection(SOCKET sockfd)
{
	return std::make_shared<TcpConnection>(_eventLoop->getTaskScheduler().get(), sockfd);
}

void TcpServer::addConnection(SOCKET sockfd, TcpConnection::Ptr tcpConn)
{
	std::lock_guard<std::mutex> locker(_conn_mutex);
	_connections.emplace(sockfd, tcpConn);
}

void TcpServer::removeConnection(SOCKET sockfd)
{
	std::lock_guard<std::mutex> locker(_conn_mutex);
	_connections.erase(sockfd);
}
void TcpServer::addActiveTcpConnection(std::string ip,std::string port)
{
    TcpSocket sockfd;
    sockfd.create();
    if(sockfd.fd()<=0)return;
    int fd=sockfd.fd();
    SocketUtil::setNonBlock(fd);
    struct sockaddr_in addr = { 0 };
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(std::stoi(port));
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    ::connect(fd, (struct sockaddr*)&addr, addrlen);
    this->_eventLoop->addTimer([this,fd,addr,addrlen](){
        ::connect(fd, (struct sockaddr*)&addr, addrlen);
        if(errno!=106)
        {
            this->removeConnection(fd);
        }
        return false;},5000);
    TcpConnection::Ptr tcpConn = this->newConnection(fd);
    if (tcpConn)
    {
        this->addConnection(fd, tcpConn);
        tcpConn->setDisconnectCallback([this] (TcpConnection::Ptr conn){
                auto taskScheduler = conn->getTaskScheduler();
                int sockfd = conn->fd();
                if (!taskScheduler->addTriggerEvent([this, sockfd] {this->removeConnection(sockfd); }))
                {
                    taskScheduler->addTimer([this, sockfd]() {this->removeConnection(sockfd); return false;}, 1);
                }
        });
    }
}
void TcpServer::addActiveTcpConnection(int fd)
{
    if(fd<=0)return;
    SocketUtil::setNonBlock(fd);
    TcpConnection::Ptr tcpConn = this->newConnection(fd);
    if (tcpConn)
    {
        this->addConnection(fd, tcpConn);
        tcpConn->setDisconnectCallback([this] (TcpConnection::Ptr conn){
                auto taskScheduler = conn->getTaskScheduler();
                int sockfd = conn->fd();
                if (!taskScheduler->addTriggerEvent([this, sockfd] {this->removeConnection(sockfd); }))
                {
                    taskScheduler->addTimer([this, sockfd]() {this->removeConnection(sockfd); return false;}, 1);
                }
        });
    }
}
