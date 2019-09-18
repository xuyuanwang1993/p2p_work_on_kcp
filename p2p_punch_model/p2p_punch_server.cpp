#include "p2p_punch_server.h"
#include "SocketUtil.h"
p2p_punch_server::p2p_punch_server()
{
    m_session_id=0;
    m_event_loop.reset(new xop::EventLoop());
    std::string server_ip="0.0.0.0";
    m_server_sock[0]=sock_init();
    xop::SocketUtil::bind(m_server_sock[0],server_ip,UDP_RECV_PORT);
    m_server_channel.reset(new xop::Channel(m_server_sock[0]));
    m_server_channel->setReadCallback([this](){this->handle_read();});
    m_server_channel->enableReading();
    m_event_loop->updateChannel(m_server_channel);

    m_server_sock[1]=sock_init();
    xop::SocketUtil::bind(m_server_sock[1],server_ip,UDP_RECV_PORT+1);
    m_help_channel.reset(new xop::Channel(m_server_sock[1]));
    m_help_channel->enableReading();
    m_event_loop->updateChannel(m_help_channel);

    m_help_channel->setReadCallback([this](){this->handle_read_helper();;});

    m_event_loop->addTimer([this](){this->remove_invalid_resources();return true;},CHECK_INTERVAL);//添加超时事件
}
void p2p_punch_server::remove_invalid_resources()
{
    int64_t time_now=GetTimeNow();
    for(auto i =std::begin(m_device_map);i!=std::end(m_device_map);)
    {
        if(time_now-i->second.alive_time>OFFLINE_TIME)
        {
            m_device_map.erase(i++);
        }
        else
        {
            i++;
        }
    }
    for(auto i =std::begin(m_session_map);i!=std::end(m_session_map);)
    {
        session_info tmp=i->second;
        if(time_now-tmp.session[0].alive_time>SESSION_CACHE_TIME||time_now-tmp.session[1].alive_time>SESSION_CACHE_TIME)
        {
            m_session_map.erase(i++);
        }
        else
        {
            i++;
        }
    }
}
int p2p_punch_server::sock_init()
{
    int fd=socket(AF_INET, SOCK_DGRAM, 0);
    if(fd<0)
    {
        LOG_ERROR("create sockets error!");
        return -1;
    }
    xop::SocketUtil::setNonBlock(fd);
    xop::SocketUtil::setReuseAddr(fd);
    xop::SocketUtil::setReusePort(fd);
    xop::SocketUtil::setSendBufSize(fd, 32 * 1024);
    return fd;
}
void p2p_punch_server::handle_read()
{
    char buf[32768]={0};
    struct sockaddr_in addr = {0};
    socklen_t len=sizeof addr;
    int ret=::recvfrom(m_server_sock[0],buf,32768,0,(struct sockaddr*)&addr,&len);
    if(ret>0)
    {
        LOG_INFO("%s",buf);
        std::map<std::string,std::string> recv_map=message_handle::parse_buf(buf);
        auto cmd=recv_map.find("cmd");
        if(cmd!=std::end(recv_map))
        {
            if(cmd->second=="nat_type_probe")
            {
                handle_nat_type_probe(m_server_sock[0],addr,recv_map);
            }
            else if(cmd->second=="keep_alive")
            {
                handle_keep_alive(m_server_sock[0],addr,recv_map);
            }
            else if(cmd->second=="set_up_connection")
            {
                handle_set_up_connection(m_server_sock[0],addr,recv_map);
            }
            else if(cmd->second=="punch_hole")
            {
                handle_punch_hole(m_server_sock[0],addr,recv_map);
            }
            else if(cmd->second=="punch_hole_response")
            {
                handle_punch_hole_response(m_server_sock[0],addr,recv_map);
            }
            else if(cmd->second=="active_connection")
            {
                handle_active_connection(m_server_sock[0],addr,recv_map);
            }
            else
            {
                handle_not_supported_command(m_server_sock[0],cmd->second,addr);
            }
        }
    }
}

void p2p_punch_server::handle_read_helper()
{
    char buf[32768]={0};
    struct sockaddr_in addr = {0};
    socklen_t len=sizeof addr;
    int ret=::recvfrom(m_server_sock[1],buf,32768,0,(struct sockaddr*)&addr,&len);
    if(ret>0)
    {
        LOG_INFO("%s",buf);
        std::map<std::string,std::string> recv_map=message_handle::parse_buf(buf);
        auto cmd=recv_map.find("cmd");
        if(cmd->second=="nat_type_probe")
        {
            handle_nat_type_probe(m_server_sock[0],addr,recv_map);
        }
        else
        {
            handle_not_supported_command(m_server_sock[0],cmd->second,addr);
        }
    }
}
void p2p_punch_server::handle_punch_hole_response(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map)
{
    std::string response;
    do{
        auto session_id=recv_map.find("session_id");
        if(session_id==std::end(recv_map))
        {
            response=packet_error_response("punch_hole_response","check the command!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        auto device_id=recv_map.find("device_id");
        if(device_id==std::end(recv_map))
        {
            response=packet_error_response("punch_hole_response","check the command!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        int t_session_id=std::stoi(session_id->second);
        auto session=m_session_map.find(t_session_id);
        if(session==std::end(m_session_map))
        {
            response=packet_error_response("punch_hole_response","invalid session_id!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        session_info &tmp=session->second;
        if(device_id->second==tmp.session[0].device_id)
        {
            tmp.session[0].recv_punch_response=true;
            tmp.session[0].alive_time=this->GetTimeNow();
        }
        else if((device_id->second==tmp.session[1].device_id))
        {
            tmp.session[1].recv_punch_response=true;
            tmp.session[1].alive_time=this->GetTimeNow();
        }
        else
        {
            response=packet_error_response("punch_hole_response","check the command!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        if(tmp.session[0].recv_punch_response&&tmp.session[1].recv_punch_response)
        {//已经收到两个回复包
            auto nat_info1=m_device_map.find(tmp.session[0].device_id);
            auto nat_info2=m_device_map.find(tmp.session[1].device_id);
            if(nat_info1==std::end(m_device_map)||nat_info2==std::end(m_device_map))
            {
                response=packet_error_response("punch_hole_response","remote device if offline!");
                ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
                break;
            }
            struct sockaddr_in addr2 = {0};
            addr2.sin_family = AF_INET;
            message_handle::packet_buf(response,"cmd","punch_hole_response");
            message_handle::packet_buf(response,"session_id",session_id->second);

            addr2.sin_addr.s_addr = inet_addr(nat_info1->second.ip.c_str());
            addr2.sin_port = htons(std::stoi(nat_info1->second.port));
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr2, sizeof addr2);
            
            addr2.sin_addr.s_addr = inet_addr(nat_info2->second.ip.c_str());
            addr2.sin_port = htons(std::stoi(nat_info2->second.port));
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr2, sizeof addr2);

            m_session_map.erase(t_session_id);
        }
    }while(0);
}

void p2p_punch_server::handle_punch_hole(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map)
{
    std::string response;
    do{
        auto session_id=recv_map.find("session_id");
        if(session_id==std::end(recv_map))
        {
            response=packet_error_response("punch_hole","check the command!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        auto device_id=recv_map.find("device_id");
        if(device_id==std::end(recv_map))
        {
            response=packet_error_response("punch_hole","check the command!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        auto local_port=recv_map.find("local_port");
        if(local_port==std::end(recv_map))
        {
            response=packet_error_response("punch_hole","check the command!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        auto session=m_session_map.find(std::stoi(session_id->second));
        if(session==std::end(m_session_map))
        {
            response=packet_error_response("punch_hole_response","invalid session_id!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        session_info &tmp=session->second;
        std::string ip=inet_ntoa(addr.sin_addr);
        std::string port=std::to_string(ntohs(addr.sin_port));
        if(device_id->second==tmp.session[0].device_id)
        {
            tmp.session[0].recv_punch_packet=true;
            tmp.session[0].alive_time=this->GetTimeNow();
            tmp.session[0].ip=ip;
            tmp.session[0].port=port;
            tmp.session[0].local_port=local_port->second;
        }
        else if((device_id->second==tmp.session[1].device_id))
        {
            tmp.session[1].recv_punch_packet=true;
            tmp.session[1].alive_time=this->GetTimeNow();
            tmp.session[1].ip=ip;
            tmp.session[1].port=port;
            tmp.session[1].local_port=local_port->second;
        }
        else
        {
            response=packet_error_response("punch_hole","check the command!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        if(tmp.session[0].recv_punch_packet&&tmp.session[1].recv_punch_packet)
        {
            message_handle::packet_buf(response,"cmd","punch_hole");
            message_handle::packet_buf(response,"session_id",session_id->second);
            bool is_local_connection=tmp.session[0].ip==tmp.session[1].ip;
            struct sockaddr_in addr2 = {0};
            addr2.sin_family = AF_INET;
            std::string t_response_1=response;
            if(is_local_connection)
            {
                message_handle::packet_buf(t_response_1,"ip",tmp.session[0].local_ip);
                message_handle::packet_buf(t_response_1,"port",tmp.session[0].local_port);
            }
            else
            {
                message_handle::packet_buf(t_response_1,"ip",tmp.session[0].ip);
                message_handle::packet_buf(t_response_1,"port",tmp.session[0].port);
            }

            addr2.sin_addr.s_addr = inet_addr(tmp.session[1].ip.c_str());
            addr2.sin_port = htons(std::stoi(tmp.session[1].port));
            ::sendto(send_fd,t_response_1.c_str(),t_response_1.length(),0,(struct sockaddr*)&addr2, sizeof addr2);
            std::string t_response_2=response;

            if(is_local_connection)
            {
                message_handle::packet_buf(t_response_2,"ip",tmp.session[1].local_ip);
                message_handle::packet_buf(t_response_2,"port",tmp.session[1].local_port);
            }
            else
            {
                message_handle::packet_buf(t_response_2,"ip",tmp.session[1].ip);
                message_handle::packet_buf(t_response_2,"port",tmp.session[1].port);
            }
            addr2.sin_addr.s_addr = inet_addr(tmp.session[0].ip.c_str());
            addr2.sin_port = htons(std::stoi(tmp.session[0].port));
            ::sendto(send_fd,t_response_2.c_str(),t_response_2.length(),0,(struct sockaddr*)&addr2, sizeof addr2);
        }
    }while(0);
}

void p2p_punch_server::handle_set_up_connection(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map)
{
    std::string response;
    do{
        auto device_id=recv_map.find("device_id");
        auto remote_device_id=recv_map.find("remote_device_id");
        auto channel_id=recv_map.find("channel_id");
        auto src_name=recv_map.find("src_name");
        if(device_id==std::end(recv_map)||remote_device_id==std::end(recv_map)||src_name==std::end(recv_map)||channel_id==std::end(recv_map))
        {
            response=packet_error_response("set_up_connection","check the command!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        if(device_id->second==remote_device_id->second)
        {
            response=packet_error_response("set_up_connection","two peers locate the same machine!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        auto nat_info=m_device_map.find(device_id->second);
        if(nat_info==std::end(m_device_map))
        {
            response=packet_error_response("set_up_connection","invalid device_id!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        auto remote_nat_info=m_device_map.find(remote_device_id->second);
        if(remote_nat_info==std::end(m_device_map))
        {
            response=packet_error_response("set_up_connection","invalid device_id!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        session_info t_session;
        t_session.session_id=m_session_id++;
        t_session.session[0].alive_time=this->GetTimeNow();
        t_session.session[0].device_id=device_id->second;
        t_session.session[0].local_ip=nat_info->second.local_ip;
        t_session.session[0].recv_punch_packet=false;
        t_session.session[0].recv_punch_response=false;

        t_session.session[1].alive_time=this->GetTimeNow();
        t_session.session[1].device_id=remote_device_id->second;
        t_session.session[1].local_ip=remote_nat_info->second.local_ip;
        t_session.session[1].recv_punch_packet=false;
        t_session.session[1].recv_punch_response=false;

        m_session_map.insert(std::make_pair(t_session.session_id,t_session));
        message_handle::packet_buf(response,"cmd","set_up_connection");
        message_handle::packet_buf(response,"session_id",std::to_string(t_session.session_id));
        message_handle::packet_buf(response,"channel_id",channel_id->second);
        message_handle::packet_buf(response,"src_name",src_name->second);
        ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);

        struct sockaddr_in addr2 = {0};
        addr2.sin_family = AF_INET;
        addr2.sin_addr.s_addr = inet_addr(remote_nat_info->second.ip.c_str());
        addr2.sin_port = htons(std::stoi(remote_nat_info->second.port));
        ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr2, sizeof addr2);
    }while(0);
}
void p2p_punch_server::handle_active_connection(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map)
{
    std::string response;
    do{
        auto device_id=recv_map.find("device_id");
        auto remote_device_id=recv_map.find("remote_device_id");
        auto channel_id=recv_map.find("channel_id");
        auto src_name=recv_map.find("src_name");
        auto port=recv_map.find("port");
        auto mode=recv_map.find("mode");
        if(device_id==std::end(recv_map)||remote_device_id==std::end(recv_map)||src_name==std::end(recv_map)||channel_id==std::end(recv_map)\
           ||port==std::end(recv_map)||mode==std::end(recv_map))
        {
            response=packet_error_response("active_connection","check the command!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        if(device_id->second==remote_device_id->second)
        {
            response=packet_error_response("active_connection","two peers locate the same machine!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        auto nat_info=m_device_map.find(device_id->second);
        auto remote_nat_info=m_device_map.find(remote_device_id->second);
        if(nat_info==std::end(m_device_map)||remote_nat_info==std::end(m_device_map))
        {
            response=packet_error_response("active_connection","invalid device_id!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        message_handle::packet_buf(response,"cmd","active_connection");
        message_handle::packet_buf(response,"session_id",std::to_string(m_session_id++));
        message_handle::packet_buf(response,"channel_id",channel_id->second);
        message_handle::packet_buf(response,"src_name",src_name->second);
        if(nat_info->second.ip==remote_nat_info->second.ip)
        {
            message_handle::packet_buf(response,"ip",nat_info->second.local_ip);
        }
        else
        {
            message_handle::packet_buf(response,"ip",nat_info->second.ip);
        }
        message_handle::packet_buf(response,"port",port->second);
        message_handle::packet_buf(response,"mode",mode->second);
        struct sockaddr_in addr2 = {0};
        addr2.sin_family = AF_INET;
        addr2.sin_addr.s_addr = inet_addr(remote_nat_info->second.ip.c_str());
        addr2.sin_port = htons(std::stoi(remote_nat_info->second.port));
        ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr2, sizeof addr2);
    }while(0);
}

void p2p_punch_server::handle_keep_alive(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map)
{
    std::string response;
    std::string ip=inet_ntoa(addr.sin_addr);
    std::string port=std::to_string(ntohs(addr.sin_port));
    auto device_id=recv_map.find("device_id");
    if(device_id==std::end(recv_map))return;
    auto local_ip=recv_map.find("local_ip");
    if(local_ip==std::end(recv_map))return;
    //std::cout<<device_id->second<<std::endl;
    auto nat_info=m_device_map.find(device_id->second);
    if(nat_info==std::end(m_device_map))
    {
        device_nat_info t_nat_info;
        t_nat_info.alive_time=this->GetTimeNow();
        t_nat_info.ip=ip;
        t_nat_info.port=port;
        t_nat_info.device_id=device_id->second;
        t_nat_info.local_ip=local_ip->second;
        m_device_map.insert(std::make_pair(device_id->second,t_nat_info));
    }
    else
    {
        device_nat_info & t_nat_info=nat_info->second;
        t_nat_info.alive_time=this->GetTimeNow();
        t_nat_info.ip=ip;
        t_nat_info.port=port;
        t_nat_info.local_ip=local_ip->second;
    }
    message_handle::packet_buf(response,"cmd","keep_alive");
    message_handle::packet_buf(response,"status","ok");
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
}

void p2p_punch_server::handle_nat_type_probe(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map)
{
    std::string response;
    std::string ip=inet_ntoa(addr.sin_addr);
    std::string port=std::to_string(ntohs(addr.sin_port));
    auto device_id=recv_map.find("device_id");
    if(device_id==std::end(recv_map))
    {
        response=packet_error_response("nat_type_probe","check the command!");
        ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
        return;
    }
    message_handle::packet_buf(response,"cmd","nat_type_probe");
    message_handle::packet_buf(response,"ip",ip);
    message_handle::packet_buf(response,"port",port);
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_server::handle_not_supported_command(int send_fd,std::string cmd,struct sockaddr_in &addr)
{
    std::string response;
    message_handle::packet_buf(response,"cmd",cmd);
    message_handle::packet_buf(response,"info","Not Supported Command!");
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
std::string p2p_punch_server::packet_error_response(std::string cmd,std::string error_info)
{
    std::string response;
    message_handle::packet_buf(response,"cmd",cmd);
    message_handle::packet_buf(response,"info",error_info);
    return response;
}
