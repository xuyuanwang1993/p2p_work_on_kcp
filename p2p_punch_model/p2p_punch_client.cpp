#include "p2p_punch_client.h"
#include <assert.h>
#include "parase_request.h"
#define FIRST_STRING "first"
#define UDP_SND_BUF_SIZE (32*1024)
p2p_punch_client::p2p_punch_client(std::string server_ip,std::string device_id,std::shared_ptr<xop::EventLoop> event_loop)
    :m_server_ip(server_ip),m_device_id(device_id),m_event_loop(event_loop)
{
    assert(m_event_loop.get());
    assert(!m_server_ip.empty()&&!m_device_id.empty());
    m_client_sock=socket(AF_INET, SOCK_DGRAM, 0);
    assert(m_client_sock>0);
    xop::SocketUtil::setNonBlock(m_client_sock);
    xop::SocketUtil::setReuseAddr(m_client_sock);
    xop::SocketUtil::setReusePort(m_client_sock);
    xop::SocketUtil::setSendBufSize(m_client_sock, UDP_SND_BUF_SIZE);
    m_client_channel.reset(new xop::Channel(m_client_sock));
    m_client_channel->setReadCallback([this](){this->handle_read();});
    m_client_channel->enableReading();
    m_event_loop->updateChannel(m_client_channel);
    m_p2p_flag=false;
    m_wan_ip=FIRST_STRING;
    m_wan_port=FIRST_STRING;
    m_cache_timer_id=m_event_loop->addTimer([this](){this->remove_invalid_resources();return true;},CHECK_INTERVAL);//添加超时事件
}

p2p_punch_client::p2p_punch_client()
{
    assert(m_event_loop.get());
    assert(!m_server_ip.empty()&&!m_device_id.empty());
    m_client_sock=socket(AF_INET, SOCK_DGRAM, 0);
    assert(m_client_sock>0);
    xop::SocketUtil::setNonBlock(m_client_sock);
    xop::SocketUtil::setReuseAddr(m_client_sock);
    xop::SocketUtil::setReusePort(m_client_sock);
    xop::SocketUtil::setSendBufSize(m_client_sock, UDP_SND_BUF_SIZE);
    m_client_channel.reset(new xop::Channel(m_client_sock));
    m_client_channel->setReadCallback([this](){this->handle_read();});
    m_client_channel->enableReading();
    m_event_loop->updateChannel(m_client_channel);
    m_p2p_flag=false;
    m_wan_ip=FIRST_STRING;
    m_wan_port=FIRST_STRING;
    m_cache_timer_id=m_event_loop->addTimer([this](){this->remove_invalid_resources();return true;},CHECK_INTERVAL);//添加超时事件
}
bool p2p_punch_client::try_establish_connection(std::string remote_device_id)
{
    if(!m_p2p_flag)
    {
        std::cout<<"can't establish_connection!"<<std::endl;
        return false;
    }
    send_set_up_connection_packet(remote_device_id);
    return true;
}

p2p_punch_client::~p2p_punch_client()
{
    if(m_cache_timer_id>0)
    {
        m_event_loop->removeTimer(m_cache_timer_id);
    }
    if(m_alive_timer_id>0)
    {
        m_event_loop->removeTimer(m_alive_timer_id);
    }
    m_event_loop->removeChannel(m_client_channel);
}
int p2p_punch_client::get_udp_session_sock()
{
    int fd=socket(AF_INET, SOCK_DGRAM, 0);
    if(fd>0)
    {
        xop::SocketUtil::setNonBlock(fd);
        xop::SocketUtil::setReuseAddr(fd);
        xop::SocketUtil::setReusePort(fd);
        xop::SocketUtil::setSendBufSize(fd, UDP_SND_BUF_SIZE);
    }
    return fd;
}

void p2p_punch_client::remove_invalid_resources()
{
    int64_t time_now=GetTimeNow();
    for(auto i=std::begin(m_session_map);i!=std::end(m_session_map);)
    {
        if(time_now-i->second.alive_time>SESSION_CACHE_TIME)
        {
            m_event_loop->removeChannel(i->second.channelPtr);
            m_session_map.erase(i++);
        }
        else
        {
            i++;
        }
    }
}
void p2p_punch_client::handle_read()
{
    char buf[32768]={0};
    struct sockaddr_in addr = {0};
    socklen_t len=sizeof addr;
    int ret=::recvfrom(m_client_sock,buf,32768,0,(struct sockaddr*)&addr,&len);
    if(ret>0)
    {
        LOG_INFO("%s",buf);
        std::map<std::string,std::string> recv_map=message_handle::parse_buf(buf);
        auto cmd=recv_map.find("cmd");
        if(cmd!=std::end(recv_map))
        {
            if(cmd->second=="nat_type_probe")
            {
                handle_nat_type_probe(recv_map);
            }
            else if(cmd->second=="keep_alive")
            {
                handle_keep_alive(recv_map);
            }
            else if(cmd->second=="set_up_connection")
            {
                handle_set_up_connection(recv_map);
            }
            else if(cmd->second=="punch_hole_response")
            {
                handle_punch_hole_response(recv_map);
            }
            else
            {
                LOG_INFO("recv false command!");
            }
        }
    }
}
void p2p_punch_client::session_handle_read(int fd)
{
    char buf[32768]={0};
    struct sockaddr_in addr = {0};
    socklen_t len=sizeof addr;
    int ret=::recvfrom(fd,buf,32768,0,(struct sockaddr*)&addr,&len);
    if(ret>0)
    {
        LOG_INFO("%s",buf);
        std::map<std::string,std::string> recv_map=message_handle::parse_buf(buf);
        auto cmd=recv_map.find("cmd");
        if(cmd!=std::end(recv_map))
        {
            if(cmd->second=="punch_hole")
            {
                handle_punch_hole(recv_map);
            }
            else
            {
                LOG_INFO("recv false command!");
            }
        }
    }
}
void p2p_punch_client::send_alive_packet()
{
    std::string request;
    message_handle::packet_buf(request,"cmd","keep_alive");
    message_handle::packet_buf(request,"device_id",m_device_id);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
    addr.sin_port = htons(UDP_RECV_PORT);
    ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_client::send_nat_type_probe_packet()
{
    if(m_p2p_flag)return;
    std::string request;
    message_handle::packet_buf(request,"cmd","nat_type_probe");
    message_handle::packet_buf(request,"device_id",m_device_id);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
    addr.sin_port = htons(UDP_RECV_PORT);
    ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
    addr.sin_port = htons(UDP_RECV_PORT+1);
    ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_client::send_punch_hole_packet(m_p2p_session &session)
{
    if(!m_p2p_flag)return;
    std::string request;
    message_handle::packet_buf(request,"cmd","punch_hole");
    message_handle::packet_buf(request,"device_id",m_device_id);
    message_handle::packet_buf(request,"session_id",std::to_string(session.session_id));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
    addr.sin_port = htons(UDP_RECV_PORT);
    ::sendto(session.channelPtr->fd(),request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_client::send_low_ttl_packet(m_p2p_session &session,std::string ip,std::string port)
{
    int curTTL=64;
    int tmpTTL=2;
    socklen_t lenTTL=sizeof(curTTL);
    getsockopt(session.channelPtr->fd(),IPPROTO_IP,IP_TTL,(char   *)&curTTL,&lenTTL);
    setsockopt(session.channelPtr->fd(), IPPROTO_IP, IP_TTL, (char *)&tmpTTL, lenTTL);
    std::string punch="########";
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(std::stoi(port));
    ::connect(session.channelPtr->fd(),(sockaddr *)&addr,sizeof addr);
    ::sendto(session.channelPtr->fd(),punch.c_str(),punch.length(),0,(sockaddr *)&addr,sizeof addr);
    setsockopt(session.channelPtr->fd(), IPPROTO_IP, IP_TTL, (char *)&curTTL, lenTTL);
}
void p2p_punch_client::send_punch_hole_response_packet(m_p2p_session &session)
{
    std::string request;
    message_handle::packet_buf(request,"cmd","punch_hole_response");
    message_handle::packet_buf(request,"device_id",m_device_id);
    message_handle::packet_buf(request,"session_id",std::to_string(session.session_id));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
    addr.sin_port = htons(UDP_RECV_PORT);
    ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_client::send_set_up_connection_packet(std::string remote_device_id)
{
    std::string request;
    message_handle::packet_buf(request,"cmd","set_up_connection");
    message_handle::packet_buf(request,"device_id",m_device_id);
    message_handle::packet_buf(request,"remote_device_id",remote_device_id);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
    addr.sin_port = htons(UDP_RECV_PORT);
    ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
}

void p2p_punch_client::handle_punch_hole_response(std::map<std::string,std::string> &recv_map)//打洞成功回复
{
    auto session_id=recv_map.find("session_id");
    if(session_id!=std::end(recv_map))
    {
        auto session=m_session_map.find(std::stoi(session_id->second));
        if(session!=std::end(m_session_map))
        {
            m_p2p_session &t_session=session->second;
            m_event_loop->removeChannel(t_session.channelPtr);
            if(t_session.callback)
            {
                std::cout<<"callback"<<std::endl;
                t_session.callback(t_session.channelPtr,t_session.session_id);
            }
            m_session_map.erase(t_session.session_id);
        }
    }
}

void p2p_punch_client::handle_punch_hole(std::map<std::string,std::string> &recv_map)//打洞
{
    auto session_id=recv_map.find("session_id");
    auto ip=recv_map.find("ip");
    auto port=recv_map.find("port");
    if(session_id!=std::end(recv_map)&&ip!=std::end(recv_map)&&port!=std::end(recv_map))
    {
        auto session=m_session_map.find(std::stoi(session_id->second));
        if(session!=std::end(m_session_map))
        {
            m_p2p_session &t_session=session->second;
            t_session.alive_time=this->GetTimeNow();
            send_low_ttl_packet(t_session,ip->second,port->second);
            send_punch_hole_response_packet(t_session);
        }
    }
}

void p2p_punch_client::handle_set_up_connection(std::map<std::string,std::string> &recv_map)//请求连接
{
    auto session_id=recv_map.find("session_id");
    if(session_id!=std::end(recv_map))
    {
        int fd=get_udp_session_sock();
        if(fd<0)return;
        m_p2p_session t_session;
        t_session.session_id=std::stoi(session_id->second);
        t_session.callback=m_connectCB;
        t_session.alive_time=GetTimeNow();
        t_session.channelPtr.reset(new xop::Channel(fd));
        t_session.channelPtr->setReadCallback([this,fd](){this->session_handle_read(fd);});
        t_session.channelPtr->enableReading();
        m_event_loop->updateChannel(t_session.channelPtr);
        m_session_map.insert(std::make_pair(t_session.session_id,t_session));
        send_punch_hole_packet(t_session);
    }
}

void p2p_punch_client::handle_keep_alive(std::map<std::string,std::string> &recv_map)//心跳保活
{
    std::cout<<"recv alive_time ack!"<<std::endl;
}

void p2p_punch_client::handle_nat_type_probe(std::map<std::string,std::string> &recv_map)//外网端口探测
{
    if(m_p2p_flag)return;
    auto ip=recv_map.find("ip");
    auto port=recv_map.find("port");
    if(ip==std::end(recv_map)||port==std::end(recv_map))
    {
        return;
    }
    if(m_wan_ip==FIRST_STRING)
    {
        m_wan_ip=ip->second;
        m_wan_port=port->second;
    }
    else
    {
        if(m_wan_ip==ip->second&&m_wan_port==port->second)
        {
            m_p2p_flag=true;
            send_alive_packet();
            m_alive_timer_id=m_event_loop->addTimer([this](){this->send_alive_packet();return true;},ALIVE_TIME_INTERVAL);
        }
        else
        {
            m_wan_ip=ip->second;
            m_wan_port=port->second;
            m_event_loop->addTimer([this](){this->send_nat_type_probe_packet();return false;},ALIVE_TIME_INTERVAL);
        }
    }
}