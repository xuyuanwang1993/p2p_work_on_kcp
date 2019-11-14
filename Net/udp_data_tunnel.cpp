#include "udp_data_tunnel.h"
#include <assert.h>
#include<Logger.h>
#include<parase_request.h>
using namespace xop;
udp_data_tunnel::udp_data_tunnel(EventLoop *loop,int port)
    :m_event_loop(loop)
{
    assert(loop&&port>0&&port<65536);
    int fd=socket(AF_INET, SOCK_DGRAM, 0);
    if(fd<0)
    {
        LOG_ERROR("create sockets error!");
        return ;
    }
    m_run=false;
    xop::SocketUtil::setNonBlock(fd);
    xop::SocketUtil::setReuseAddr(fd);
    xop::SocketUtil::setReusePort(fd);
    xop::SocketUtil::setSendBufSize(fd, 32 * 1024);
    xop::SocketUtil::bind(fd,"0.0.0.0",static_cast<uint16_t>(port));
    m_udp_channel.reset(new xop::Channel(fd));
    m_udp_channel->setReadCallback([this](){this->handle_read();});
    m_udp_channel->enableReading();
    m_event_loop->updateChannel(m_udp_channel);
}
udp_data_tunnel::~udp_data_tunnel()
{
    m_run=false;
    m_cache_timer_id=0;
    m_session_map.clear();
    if(m_cache_timer_id>0)m_event_loop->removeTimer(m_cache_timer_id);
}
void udp_data_tunnel::run()
{
    if(!m_run)
    {
        register_data_handlers();
        m_cache_timer_id=m_event_loop->addTimer([this](){
            this->remove_invalid_tunnel();
            return true;},CHECK_INTERVAL);
        m_run=true;
    }
}
bool udp_data_tunnel::add_tunnel_session(unsigned int session_id,std::string source_name,PROTOCAL_TYPE protocal_type)
{
    if(!m_run)return false;
    auto session=m_session_map.find(session_id);
    if(session!=std::end(m_session_map))return false;
    tunnel_session new_session;
    new_session.session_id=session_id;
    new_session.source_name=source_name;
    new_session.protocal_type=protocal_type;
    m_session_map.insert(std::pair<int,tunnel_session>(session_id,std::move(new_session)));
    return true;
}
void udp_data_tunnel::remove_tunnel_session(unsigned int session_id)
{
    if(m_run)return;
    m_event_loop->addTriggerEvent([this,session_id](){
        std::lock_guard<std::mutex>locker(m_mutex);
        m_session_map.erase(session_id);});
}
void udp_data_tunnel::register_data_handlers()
{
    //just relay
    auto udp_handler=[this](struct sockaddr_in &addr, const char *buf,unsigned int size){
        LOG_INFO("udp send size %d!",size);
        ::sendto(m_udp_channel->fd(),buf,size,0,reinterpret_cast<struct sockaddr *>(&addr),sizeof addr);
    };
    m_handler_map.insert(std::make_pair(PROTOCAL_UDP,udp_handler));
    //just relay
    auto kcp_handler=[this](struct sockaddr_in &addr, const char *buf,unsigned int size){
        LOG_INFO("kcp send size %d!",size);
        ::sendto(m_udp_channel->fd(),buf,size,0,reinterpret_cast<struct sockaddr *>(&addr),sizeof addr);
    };
    m_handler_map.insert(std::make_pair(PROTOCAL_KCP,kcp_handler));
}
void udp_data_tunnel::handle_read()
{
    char buf[32768]={0};
    struct sockaddr_in addr = {0};
    socklen_t len=sizeof addr;
    int ret=::recvfrom(m_udp_channel->fd(),buf,32768,0,(struct sockaddr*)&addr,&len);
    std::cout<<ret<<std::endl;
    if(ret>=4)
    {
        std::map<std::string,std::string> recv_map=message_handle::parse_buf(buf);
        if(!recv_map.empty())
        {
            LOG_INFO("udp_data_tunnel recv:%s",buf);
            auto cmd=recv_map.find("cmd");
            if(cmd->second!="start_tunnel_transfer")return;
            auto session_id=recv_map.find("session_id");
            if(session_id==recv_map.end())return;
            unsigned int s_session_id=static_cast<unsigned int>(std::stoi(session_id->second));
            auto tunnel_session=m_session_map.find(s_session_id);
            if(tunnel_session!=std::end(m_session_map)&&tunnel_session->second.state!=ESTABLISHED)
            {
                tunnel_session->second.last_alive_time=GetTimeNow();
                if(tunnel_session->second.state==WAIT_CONNECT)
                {
                    tunnel_session->second.addr1=addr;
                    tunnel_session->second.state=HALF_CONNECT;
                }
                else
                {
                    tunnel_session->second.addr2=addr;
                    if(tunnel_session->second.addr1.sin_addr.s_addr!=tunnel_session->second.addr2.sin_addr.s_addr||\
                            tunnel_session->second.addr1.sin_port!=tunnel_session->second.addr2.sin_port)
                    {
                        tunnel_session->second.state=ESTABLISHED;
                        handle_connection_established(tunnel_session->second);
                    }
                    else {
                        std::cout<<tunnel_session->second.addr1.sin_addr.s_addr<<":"<<tunnel_session->second.addr2.sin_addr.s_addr<<std::endl;
                        std::cout<<tunnel_session->second.addr1.sin_port<<":"<<tunnel_session->second.addr2.sin_port<<std::endl;
                    }
                }
            }
            else {
                std::string response;
                message_handle::packet_buf(response,"cmd","start_tunnel_transfer");
                message_handle::packet_buf(response,"session_id",session_id->second);
                message_handle::packet_buf(response,"status","false format or not existed session!");
            }
        }
        else {
            unsigned int session_id=get_session_id(buf);
            auto tunnel_session=m_session_map.find(session_id);
            if(tunnel_session!=std::end(m_session_map)&&tunnel_session->second.state==ESTABLISHED)
            {
                tunnel_session->second.last_alive_time=GetTimeNow();
                if(addr.sin_addr.s_addr==tunnel_session->second.addr1.sin_addr.s_addr&&addr.sin_port==tunnel_session->second.addr1.sin_port)
                {
                 //   std::cout<<"111111111"<<std::endl;
                    addr=tunnel_session->second.addr2;
                }
                else if(addr.sin_addr.s_addr==tunnel_session->second.addr2.sin_addr.s_addr&&addr.sin_port==tunnel_session->second.addr2.sin_port){
               // std::cout<<"22222222222"<<std::endl;
                    addr=tunnel_session->second.addr1;
                }
                else {
                    return;
                }
                auto callback=m_handler_map.find(tunnel_session->second.protocal_type);
                if(callback!=std::end(m_handler_map)&&callback->second)
                {
                    std::cout<<"callback!"<<std::endl;
                    callback->second(addr,buf,static_cast<uint32_t>(ret));
                }
                else {
                    std::cout<<"not callback!"<<std::endl;
                }
            }
        }
    }
}
void udp_data_tunnel::handle_connection_established(tunnel_session &session)
{
    std::string response;
    message_handle::packet_buf(response,"cmd","start_tunnel_transfer");
    message_handle::packet_buf(response,"session_id",std::to_string(session.session_id));
    message_handle::packet_buf(response,"source_name",session.source_name);
    message_handle::packet_buf(response,"status","ok");
    struct sockaddr_in addr = {0};
    socklen_t len=sizeof addr;
    ::sendto(m_udp_channel->fd(),response.c_str(),response.length(),0,reinterpret_cast<struct sockaddr *>(&session.addr1),len);
    ::sendto(m_udp_channel->fd(),response.c_str(),response.length(),0,reinterpret_cast<struct sockaddr *>(&session.addr2),len);
}
