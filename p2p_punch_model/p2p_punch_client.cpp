#include "p2p_punch_client.h"
#include <assert.h>
#include "parase_request.h"
#include "NetInterface.h"
#define FIRST_STRING "first"
#define UDP_SND_BUF_SIZE (32*1024)
typedef enum{
    CLIENT_BEGIN=0,
    CLIENT_NAT_TYPE_PROBE,
    CLIENT_KEEP_ALIVE,
    CLIENT_SET_UP_CONNECTION,
    CLINET_PUNCH_HOLE_RESPONSE,
    CLIENT_ACTIVE_CONNECTION,
    CLIENT_GET_STREAM_SERVER_INFO,
    CLIENT_RESET_STREAM_STATE,
    CLIENT_RESTART_DEVICE,
    CLIENT_GET_EXTERNAL_IP,
    CLIENT_STREAM_SERVER_REGISTER,
    CLIENT_RESTART_STREAM_SERVER,
    CLIENT_STREAM_SERVER_KEEPALIVE,
    CLIENT_END,
}CLIENT_COMMAND;
static const char COMMAND_LIST[][64]={
    "begin",
    "nat_type_probe",
    "keep_alive",
    "set_up_connection",
    "punch_hole_response",
    "active_connection",
    "get_stream_server_info",
    "reset_stream_state",
    "restart_device",
    "get_external_ip",
    "stream_server_register",
    "restart_stream_server",
    "stream_server_keepalive",
    "end",
};
std::unordered_map<std::string,int> p2p_punch_client::m_command_map;
p2p_punch_client::p2p_punch_client(std::string server_ip,std::string device_id,std::shared_ptr<xop::EventLoop> event_loop)
    :m_server_ip(server_ip),m_device_id(device_id),m_event_loop(event_loop)
{
    static std::once_flag oc_init;
    std::call_once(oc_init, [] {
        CLIENT_COMMAND cmd=static_cast<CLIENT_COMMAND>(CLIENT_BEGIN+1);
        while(cmd!=CLIENT_END)
        {
            m_command_map.insert(std::make_pair(std::string(COMMAND_LIST[cmd]),static_cast<int>(cmd)));
            cmd=static_cast<CLIENT_COMMAND>(cmd+1);
        }
    });
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
    m_alive_timer_id=0;
    m_stream_client_check_timer_id=0;
    m_stream_server_check_timer_id=0;
    m_stream_server_getip_timer_id=0;
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
void p2p_punch_client::stream_client_init(std::string account_name,int load_size,ResetStreamServerCallback cb)
{
    if(m_stream_server_info)return;
    m_stream_server_info.reset(new stream_server_info);
    m_stream_server_info->account_name=account_name;
    m_stream_server_info->load_size=std::to_string(load_size);
    m_stream_server_info->mode=STREAM_CLIENT;
    m_resetstreamserverCB=cb;
}
void p2p_punch_client::stream_server_init(std::string path,RestartStreamServerCallback cb)
{
    if(m_stream_server_info)return;
    m_restartstreamserverCB=cb;
    m_stream_server_info.reset(new stream_server_info);
    m_stream_server_info->mode=STREAM_SERVER;
    FILE *fp=fopen(path.c_str(),"r");
    if(!fp)return;
    do{
        if(fseek(fp,0,SEEK_END)!=0)break;
        long len=ftell(fp);
        if(len<=0)break;
        if(fseek(fp,0,SEEK_SET)!=0)break;
        std::shared_ptr<char> buf(new char[static_cast<size_t>(len+1)]);
        size_t ret=fread(buf.get(),1,static_cast<size_t>(len),fp);
#if defined(__linux) || defined(__linux__) 
        if(ret!=static_cast<size_t>(len))break;
#endif
        auto message_map=message_handle::parse_buf(buf.get());
        auto load_size=message_map.find("load_size");
        auto internal_port=message_map.find("internal_port");
        auto external_port=message_map.find("external_port");
        auto account_name=message_map.find("account_name");
        auto igd_ip=message_map.find("igd_ip");

        if(load_size==message_map.end()||internal_port==message_map.end()||external_port==message_map.end()\
                ||account_name==message_map.end()||igd_ip==message_map.end())break;
        m_stream_server_info->load_size=load_size->second;
        m_stream_server_info->internal_port=internal_port->second;
        m_stream_server_info->external_port=external_port->second;
        m_stream_server_info->account_name=account_name->second;
        std::string s_igd_ip=igd_ip->second;
        upnp::UpnpMapper::Instance().Init(m_event_loop.get(),s_igd_ip);
        std::cout<<"stream_server init success!"<<std::endl;
    }while(0);
    fclose(fp);
}
void p2p_punch_client::start_stream_check_task()
{
    if(!m_stream_server_info||m_stream_server_info->check_task)return;
    m_stream_server_info->check_task=true;
    if(m_stream_server_info->mode==STREAM_CLIENT)
    {
        send_get_stream_server_info();
        m_stream_client_check_timer_id=m_event_loop->addTimer([this](){
        this->send_get_stream_server_info();
        return true;},ALIVE_TIME_INTERVAL*2);
    }
    else {
        m_stream_server_getip_timer_id=m_event_loop->addTimer([this](){
            upnp::UpnpMapper::Instance().Api_addportMapper(upnp::SOCKET_TCP,xop::NetInterface::getLocalIPAddress(),std::stoi(this->m_stream_server_info->internal_port),\
                                                           std::stoi(m_stream_server_info->external_port),"stream_server");
            return false;},2000);
        m_stream_server_check_timer_id=m_event_loop->addTimer([this](){
        auto func=[this](bool status){
            if(!status)
            {
                upnp::UpnpMapper::Instance().Api_addportMapper(upnp::SOCKET_TCP,xop::NetInterface::getLocalIPAddress(),std::stoi(this->m_stream_server_info->internal_port),\
                                                               std::stoi(m_stream_server_info->external_port),"stream_server");
            }
        };
        if( upnp::UpnpMapper::Instance().Api_discoverOk())
        {
            upnp::UpnpMapper::Instance().Api_GetSpecificPortMappingEntry(upnp::SOCKET_TCP,std::stoi(m_stream_server_info->external_port),func);
        }
        return true;},ALIVE_TIME_INTERVAL*10);
    }
}
void p2p_punch_client::start()
{
    if(!m_p2p_flag)
    {
        if(m_stream_server_info&&m_stream_server_info->mode==STREAM_SERVER)
        {
            send_stream_server_register();
            this->m_event_loop->addTimer([this](){this->send_stream_server_keepalive();return true;},ALIVE_TIME_INTERVAL);
            m_p2p_flag=true;
            return;
        }
        send_nat_type_probe_packet();
    }
}
bool p2p_punch_client::try_establish_connection(std::string remote_device_id,int channel_id,std::string src_name)
{
    if(!m_p2p_flag)
    {
        std::cout<<"can't establish_connection!"<<std::endl;
        return false;
    }
    send_set_up_connection_packet(remote_device_id,channel_id,src_name);
    return true;
}
bool p2p_punch_client::try_establish_active_connection(std::string remote_device_id,int port,int channel_id,std::string src_name,int mode)
{
    send_active_connection(remote_device_id,channel_id,src_name,port,mode);
    return true;
}

p2p_punch_client::~p2p_punch_client()
{
    if(m_stream_server_info&&m_stream_server_info->mode==STREAM_SERVER)
    {
        if(upnp::UpnpMapper::Instance().Api_discoverOk())
        {
            upnp::UpnpMapper::Instance().Api_deleteportMapper(upnp::SOCKET_TCP,std::stoi(m_stream_server_info->external_port));
        }
    }
    if(m_cache_timer_id>0)
    {
        m_event_loop->removeTimer(m_cache_timer_id);
    }
    if(m_alive_timer_id>0)
    {
        m_event_loop->removeTimer(m_alive_timer_id);
    }
    if(m_stream_client_check_timer_id>0)
    {
        m_event_loop->removeTimer(m_stream_client_check_timer_id);
    }
    if(m_stream_server_check_timer_id>0)
    {
        m_event_loop->removeTimer(m_stream_server_check_timer_id);
    }
    if(m_stream_server_getip_timer_id>0)
    {
        m_event_loop->removeTimer(m_stream_server_getip_timer_id);
    }
    m_event_loop->removeChannel(m_client_channel);
}
int p2p_punch_client::get_udp_session_sock()
{
    int fd=socket(AF_INET, SOCK_DGRAM, 0);
    if(fd>0)
    {
        xop::SocketUtil::setNonBlock(fd);
        xop::SocketUtil::random_bind(fd,1000);
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
            auto command_name=m_command_map.find(cmd->second);
            if(command_name==std::end(m_command_map))
            {
                LOG_INFO("recv false command!");
            }
            else {
                switch(command_name->second)
                {
                case CLIENT_NAT_TYPE_PROBE:
                    handle_nat_type_probe(recv_map);
                    break;
                case CLIENT_KEEP_ALIVE:
                    handle_keep_alive(recv_map);
                    break;
                case CLIENT_SET_UP_CONNECTION:
                    handle_set_up_connection(recv_map);
                    break;
                case CLINET_PUNCH_HOLE_RESPONSE:
                    handle_punch_hole_response(recv_map);
                    break;
                case CLIENT_ACTIVE_CONNECTION:
                    handle_active_connection(recv_map);
                    break;
                case CLIENT_GET_STREAM_SERVER_INFO:
                    if(m_stream_server_info&&m_stream_server_info->mode==STREAM_CLIENT)
                        handle_get_stream_server_info(recv_map);
                    break;
                case CLIENT_RESET_STREAM_STATE:
                    if(m_stream_server_info&&m_stream_server_info->mode==STREAM_CLIENT)
                        handle_reset_stream_state();
                    break;
                case CLIENT_RESTART_DEVICE:
                    if(!m_stream_server_info||m_stream_server_info->mode!=STREAM_SERVER)
                        handle_restart_device();
                case CLIENT_GET_EXTERNAL_IP:
                    handle_get_external_ip(recv_map);
                    break;
                case CLIENT_STREAM_SERVER_REGISTER:
                    if(m_stream_server_info&&m_stream_server_info->mode==STREAM_SERVER)
                        handle_stream_server_register(recv_map);
                    break;
                case CLIENT_RESTART_STREAM_SERVER:
                    if(m_stream_server_info&&m_stream_server_info->mode==STREAM_SERVER)
                        handle_restart_stream_server();
                    break;
                case CLIENT_STREAM_SERVER_KEEPALIVE:
                    if(m_stream_server_info&&m_stream_server_info->mode==STREAM_SERVER)
                        handle_stream_server_keepalive();
                    break;
                default:
                    LOG_INFO("unknown command!");
                    break;
                }
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
    message_handle::packet_buf(request,"local_ip",xop::NetInterface::getLocalIPAddress());
    if(m_stream_server_info)
    {
        message_handle::packet_buf(request,"stream_server_id",m_stream_server_info->stream_server_id);
    }
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
    message_handle::packet_buf(request,"local_port",std::to_string(xop::SocketUtil::getLocalPort(session.channelPtr->fd())));
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
    std::cout<<"send_punch_hole_response_packet"<<std::endl;
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
void p2p_punch_client::send_set_up_connection_packet(std::string remote_device_id,int channel_id,std::string src_name)
{
    std::string request;
    message_handle::packet_buf(request,"cmd","set_up_connection");
    message_handle::packet_buf(request,"device_id",m_device_id);
    message_handle::packet_buf(request,"remote_device_id",remote_device_id);
    message_handle::packet_buf(request,"channel_id",std::to_string(channel_id));
    message_handle::packet_buf(request,"src_name",src_name);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
    addr.sin_port = htons(UDP_RECV_PORT);
    ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_client::send_active_connection(std::string remote_device_id,int channel_id,std::string src_name,int port,int mode)
{
    std::string request;
    message_handle::packet_buf(request,"cmd","active_connection");
    message_handle::packet_buf(request,"device_id",m_device_id);
    message_handle::packet_buf(request,"remote_device_id",remote_device_id);
    message_handle::packet_buf(request,"channel_id",std::to_string(channel_id));
    message_handle::packet_buf(request,"src_name",src_name);
    message_handle::packet_buf(request,"port",std::to_string(port));
    if(mode==0)message_handle::packet_buf(request,"mode","udp");
    else message_handle::packet_buf(request,"mode","tcp");
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
    addr.sin_port = htons(UDP_RECV_PORT);
    ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_client::send_get_stream_server_info()
{
    if(!m_stream_server_info&&m_stream_server_info->mode!=STREAM_CLIENT)return;
    std::string request;
    message_handle::packet_buf(request,"cmd","get_stream_server_info");
    message_handle::packet_buf(request,"device_id",m_device_id);
    message_handle::packet_buf(request,"stream_server_id",m_stream_server_info->stream_server_id);
    message_handle::packet_buf(request,"account_name",m_stream_server_info->account_name);
    message_handle::packet_buf(request,"load_size",m_stream_server_info->load_size);
    if(std::stoi(m_stream_server_info->stream_server_id)>=0)
    {
        message_handle::packet_buf(request,"in_use","true");
    }
    else {
        message_handle::packet_buf(request,"in_use","false");
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
    addr.sin_port = htons(UDP_RECV_PORT);
    ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_client::send_get_external_ip()
{
    std::string request;
    message_handle::packet_buf(request,"cmd","get_external_ip");
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
    addr.sin_port = htons(UDP_RECV_PORT);
    ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_client::send_stream_server_register()
{
    if(m_stream_server_info&&m_stream_server_info->mode==STREAM_SERVER&&m_stream_server_info->stream_server_id=="-1")
    {
        std::string request;
        message_handle::packet_buf(request,"cmd","stream_server_register");
        message_handle::packet_buf(request,"account_name",m_stream_server_info->account_name);
        message_handle::packet_buf(request,"max_load_size",m_stream_server_info->load_size);
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
        addr.sin_port = htons(UDP_RECV_PORT);
        ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
    }
}
void p2p_punch_client::send_stream_server_keepalive()
{
    if(m_stream_server_info&&m_stream_server_info->mode==STREAM_SERVER&&m_stream_server_info->stream_server_id!="-1")
    {
        std::string request;
        message_handle::packet_buf(request,"cmd","stream_server_keepalive");
        message_handle::packet_buf(request,"stream_server_id",m_stream_server_info->stream_server_id);
        message_handle::packet_buf(request,"external_port",m_stream_server_info->external_port);
        message_handle::packet_buf(request,"internal_port",m_stream_server_info->internal_port);
        message_handle::packet_buf(request,"internal_ip",xop::NetInterface::getLocalIPAddress());
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(m_server_ip.c_str());
        addr.sin_port = htons(UDP_RECV_PORT);
        ::sendto(m_client_sock,request.c_str(),request.length(),0,(struct sockaddr*)&addr, sizeof addr);
    }
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
                t_session.callback(t_session.channelPtr,t_session.session_id,t_session.channel_id,t_session.src_name);
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
    auto channel_id=recv_map.find("channel_id");
    auto src_name=recv_map.find("src_name");
    if(session_id!=std::end(recv_map)&&channel_id!=std::end(recv_map)&&src_name!=std::end(recv_map))
    {
        int fd=get_udp_session_sock();
        if(fd<0)return;
        m_p2p_session t_session;
        t_session.session_id=std::stoi(session_id->second);
        t_session.channel_id=std::stoi(channel_id->second);
        t_session.src_name=src_name->second;
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
void p2p_punch_client::handle_active_connection(std::map<std::string,std::string> &recv_map)
{
    auto mode=recv_map.find("mode");
    auto session_id=recv_map.find("session_id");
    auto channel_id=recv_map.find("channel_id");
    auto src_name=recv_map.find("src_name");
    auto ip=recv_map.find("ip");
    auto port=recv_map.find("port");
    if(mode==std::end(recv_map)||session_id==std::end(recv_map)||channel_id==std::end(recv_map)||src_name==std::end(recv_map)||\
            ip==std::end(recv_map)||port==std::end(recv_map))return;
    if(mode->second=="udp"&&m_connectCB)
    {
        udp_active_connect_task(session_id->second,channel_id->second,src_name->second,ip->second,port->second,mode->second);
    }
    else if(mode->second=="tcp"&&m_TcpconnectCB)
    {
        tcp_active_connect_task(session_id->second,channel_id->second,src_name->second,ip->second,port->second,mode->second);
    }
    else
    {
        std::cerr<<"handle_active_connection error! try check p2p_punch_client's config!"<<std::endl;
    }
}
void p2p_punch_client::handle_get_stream_server_info(std::map<std::string,std::string> &recv_map)
{
    auto stream_server_id=recv_map.find("stream_server_id");
    if(stream_server_id==recv_map.end()||std::stoi(stream_server_id->second)<0)return;
    auto external_ip=recv_map.find("external_ip");
    auto external_port=recv_map.find("external_port");
    auto internal_ip=recv_map.find("internal_ip");
    auto internal_port=recv_map.find("internal_port");
    auto lan_flag=recv_map.find("lan_flag");
    m_stream_server_info->stream_server_id=stream_server_id->second;
    if(internal_ip->second!=m_stream_server_info->internal_ip||internal_port->second!=m_stream_server_info->internal_port\
    ||external_ip->second!=m_stream_server_info->external_ip||external_port->second!=m_stream_server_info->external_port)
    {
        m_stream_server_info->internal_ip=internal_ip->second;
        m_stream_server_info->internal_port=internal_port->second;
        m_stream_server_info->external_ip=external_ip->second;
        m_stream_server_info->external_port=external_port->second;
        if(lan_flag->second=="true")
        {
            if(m_resetstreamserverCB)m_resetstreamserverCB(m_stream_server_info->internal_ip,std::stoi(m_stream_server_info->internal_port),\
            m_stream_server_info->external_ip,std::stoi(m_stream_server_info->external_port));
        }
        else {
            if(m_resetstreamserverCB)m_resetstreamserverCB(m_stream_server_info->external_ip,std::stoi(m_stream_server_info->external_port),\
            m_stream_server_info->external_ip,std::stoi(m_stream_server_info->external_port));
        }
    }
}
void p2p_punch_client::handle_reset_stream_state()
{
    m_stream_server_info->internal_ip="";
    m_stream_server_info->internal_port="0";
    m_stream_server_info->external_ip="";
    m_stream_server_info->external_port="";
    m_stream_server_info->stream_server_id="-1";
    if(m_resetstreamserverCB)m_resetstreamserverCB("0.0.0.0",0,"0.0.0.0",0);
}
void p2p_punch_client::handle_restart_device()
{
    std::cout<<"restart test!"<<std::endl;
    //    system("reboot");
}
void p2p_punch_client::handle_get_external_ip(std::map<std::string,std::string> &recv_map)
{
    auto external_ip=recv_map.find("external_ip");
    if(external_ip==recv_map.end())return;
    if(m_stream_server_info&&m_stream_server_info->mode==STREAM_SERVER)
    {
        m_stream_server_info->external_ip=external_ip->second;
    }
}
void p2p_punch_client::handle_stream_server_register(std::map<std::string,std::string> &recv_map)
{
    if(m_stream_server_info->stream_server_id=="-1")
    {
        auto stream_server_id=recv_map.find("stream_server_id");
        if(stream_server_id==recv_map.end())return;
        m_stream_server_info->stream_server_id=stream_server_id->second;
    }
}
void p2p_punch_client::handle_restart_stream_server()
{
    std::cout<<"handle_restart_stream_server"<<std::endl;
    m_stream_server_info->stream_server_id="-1";
    if(m_restartstreamserverCB)m_restartstreamserverCB();
    send_stream_server_register();
}
void p2p_punch_client::handle_stream_server_keepalive()
{
    std::cout<<"handle_stream_server_keepalive"<<std::endl;
}
void p2p_punch_client::udp_active_connect_task(std::string session_id,std::string channel_id,std::string src_name,std::string ip,std::string port,std::string mode)
{
    int fd=get_udp_session_sock();
    if(fd<0)return;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(std::stoi(port));
    ::connect(fd,(sockaddr *)&addr,sizeof addr);
    std::string request;
    message_handle::packet_buf(request,"cmd","set_up_connection");
    message_handle::packet_buf(request,"session_id",session_id);
    message_handle::packet_buf(request,"channel_id",channel_id);
    message_handle::packet_buf(request,"src_name",src_name);
    message_handle::packet_buf(request,"ip",ip);
    message_handle::packet_buf(request,"port",port);
    message_handle::packet_buf(request,"mode",mode);
    ::send(fd,request.c_str(),request.length(),0);
    m_connectCB(std::make_shared<xop::Channel>(fd),std::stoi(session_id),std::stoi(channel_id),src_name);
}

void p2p_punch_client::tcp_active_connect_task(std::string session_id,std::string channel_id,std::string src_name,std::string ip,std::string port,std::string mode)
{
    std::cout<<"tcp_active_connect_task"<<std::endl;
    std::thread t([session_id,channel_id,src_name,ip,port,mode,this](){
        int fd=socket(AF_INET, SOCK_STREAM, 0);
        if(fd<0)return;
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(std::stoi(port));
        if(::connect(fd,(sockaddr *)&addr,sizeof addr)!=0)
        {
            std::cerr<<"ip : "<<ip<<"  port : "<<port<<std::endl;
            std::cerr<<strerror(errno)<<std::endl;
            return;
        }
        std::string request;
        message_handle::packet_buf(request,"cmd","set_up_connection");
        message_handle::packet_buf(request,"session_id",session_id);
        message_handle::packet_buf(request,"channel_id",channel_id);
        message_handle::packet_buf(request,"src_name",src_name);
        message_handle::packet_buf(request,"ip",ip);
        message_handle::packet_buf(request,"port",port);
        message_handle::packet_buf(request,"mode",mode);
        ::send(fd,request.c_str(),request.length(),0);
        m_TcpconnectCB(std::make_shared<xop::Channel>(fd),std::stoi(session_id),std::stoi(channel_id),src_name);
    });
    t.detach();
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
    if(m_alive_timer_id<=0)m_alive_timer_id=m_event_loop->addTimer([this](){this->send_alive_packet();return true;},ALIVE_TIME_INTERVAL);
    if(m_wan_ip==FIRST_STRING)
    {
        m_wan_ip=ip->second;
        m_wan_port=port->second;
    }
    else
    {
        if(m_wan_ip==ip->second&&m_wan_port==port->second)
        {
            std::cout<<"support p2p connection!"<<std::endl;
            m_p2p_flag=true;
            send_alive_packet();
        }
        else
        {
            std::cout<<"not support p2p connection!"<<std::endl;
            m_wan_ip=ip->second;
            m_wan_port=port->second;
        }
    }
}
