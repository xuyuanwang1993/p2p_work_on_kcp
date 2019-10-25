#include "p2p_punch_server.h"
#include "SocketUtil.h"
#include "NetInterface.h"
typedef enum{
    SERVER_BEGIN=0,
    SERVER_NAT_TYPE_PROBE,
    SERVER_KEEP_ALIVE,
    SERVER_SET_UP_CONNECTION,
    SERVER_PUNCH_HOLE,
    SERVER_PUCHN_HOLE_RESPONSE,
    SERVER_ACTIVE_CONNECTION,
    SERVER_GET_EXTERNAL_IP,
    SERVER_GET_STREAM_SERVER_INFO,
    SERVER_STREAM_SERVER_REGISTER,
    SERVER_STREAM_SERVER_KEEPALIVE,
    SERVER_RESET_STREAM_STATE,
    SERVER_GET_ONLINE_DEVICE_INFO,
    SERVER_GET_STREAM_SERVER_STATE,
    SERVER_RESTART_STREAM_SERVER,
    SERVER_RESTART_DEVICE,
    SERVER_END,
}SERVER_COMMAND;
static const char COMMAND_LIST[][64]={
  "begin",
  "nat_type_probe",
  "keep_alive",
  "set_up_connection",
  "punch_hole",
  "punch_hole_response",
  "active_connection",
  "get_external_ip",
  "get_stream_server_info",
  "stream_server_register",
  "stream_server_keepalive",
  "reset_stream_state",
  "get_online_device_info",
  "get_stream_server_state",
  "restart_stream_server",
  "restart_device",
  "end",
};
std::unordered_map<std::string,int> p2p_punch_server::m_command_map;
p2p_punch_server::p2p_punch_server()
{
    static std::once_flag oc_init;
    std::call_once(oc_init, [] {
        SERVER_COMMAND cmd=static_cast<SERVER_COMMAND>(SERVER_BEGIN+1);
        while(cmd!=SERVER_END)
        {
            m_command_map.insert(std::make_pair(std::string(COMMAND_LIST[cmd]),static_cast<int>(cmd)));
            cmd=static_cast<SERVER_COMMAND>(cmd+1);
        }
    });

    m_session_id=0;
    m_stream_server_id=1;
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
    m_stream_server_task_init=false;
    m_event_loop->addTimer([this](){this->remove_invalid_resources();return true;},CHECK_INTERVAL);//添加超时事件
}
void p2p_punch_server::init_stream_server_task(std::string path)
{
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
        auto stream_port=message_map.find("stream_port");
        auto external_ip=message_map.find("external_ip");
        auto authorized_list=message_map.find("authorized_list");
        if(load_size==message_map.end()||stream_port==message_map.end()||authorized_list==message_map.end()\
        ||external_ip==message_map.end())break;
        std::string key="||";
        int start_pos=0;
        while(start_pos<authorized_list->second.length())
        {
            int pos=authorized_list->second.find(key,start_pos);
            if(pos==-1)
            {
                m_authorized_accounts.insert(authorized_list->second.substr(start_pos,authorized_list->second.length()-start_pos));
                break;
            }
            m_authorized_accounts.insert(authorized_list->second.substr(start_pos,pos-start_pos));
            start_pos=pos+key.length();
        }
        m_id_map[0]=DEFAULT_SERVER_ACCOUNT;
        auto &stream_map=m_stream_server_map[DEFAULT_SERVER_ACCOUNT];
        stream_server_info server_info;
        server_info.id=0;
        server_info.external_ip=external_ip->second;
        server_info.external_port=0;
        server_info.internal_ip=external_ip->second;
        server_info.available_load_size=std::stoi(load_size->second);
        server_info.stream_external_port=std::stoi(stream_port->second);
        server_info.stream_internal_port=server_info.stream_external_port;
        stream_map.emplace(std::pair<int,stream_server_info>(0,server_info));
        m_stream_server_task_init=true;
    }while(0);
    fclose(fp);
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
    for(auto i =std::begin(m_stream_server_map);i!=std::end(m_stream_server_map);i++)
    {
        for(auto j=std::begin(i->second);j!=std::end(i->second);)
        {
            if(time_now-j->second.last_alive_time>OFFLINE_TIME&&j->second.id!=0)
            {
                for(auto k : j->second.device_load_size_map)
                {
                    auto device_info=m_device_map.find(k.first);
                    if(device_info==std::end(m_device_map))continue;
                    struct sockaddr_in addr = { 0 };
                    socklen_t addrlen = sizeof(addr);
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(std::stoi(device_info->second.port));
                    addr.sin_addr.s_addr = inet_addr(device_info->second.ip.c_str());
                    send_reset_stream_state(m_server_sock[0],addr);
                }
                m_id_map.erase(j->second.id);
                i->second.erase(j++);
            }
            else {
                for(auto k=std::begin(j->second.device_load_size_map);k!=std::end(j->second.device_load_size_map);)
                {//移除无效设备
                    if(time_now-k->second.second>OFFLINE_TIME)
                    {
                        j->second.available_load_size+=k->second.first;
                        j->second.device_load_size_map.erase(k++);
                    }
                    else {
                        k++;
                    }
                }
                j++;
            }
        }
    }
}
CJSON *p2p_punch_server::get_stream_server_state()
{
    CJSON *root=CJSON_CreateObject();
    for(auto i : m_stream_server_map)
    {
        CJSON *account=CJSON_AddObjectToObject(root,i.first.c_str());
        for(auto j : i.second)
        {
            CJSON *server=CJSON_AddObjectToObject(account,std::to_string(j.first).c_str());
            CJSON_AddNumberToObject(server,"last_alive_time",j.second.last_alive_time);
            CJSON_AddStringToObject(server,"external_ip",j.second.external_ip.c_str());
            CJSON_AddNumberToObject(server,"external_port",j.second.external_port);
            CJSON_AddNumberToObject(server,"stream_external_port",j.second.stream_external_port);
            CJSON_AddStringToObject(server,"internal_ip",j.second.internal_ip.c_str());
            CJSON_AddNumberToObject(server,"stream_internal_port",j.second.stream_internal_port);
            CJSON_AddNumberToObject(server,"available_load_size",j.second.available_load_size);
            auto tmp=CJSON_AddArrayToObject(server,"device_load_size_map");
            for(auto k : j.second.device_load_size_map)
            {
                auto device=CJSON_CreateString(k.first.c_str());
                CJSON_AddItemToArray(tmp,device);
            }
        }
    }
    return root;
}
CJSON *p2p_punch_server::get_online_device_info()
{
    CJSON *root=CJSON_CreateObject();
    for(auto i :m_device_map)
    {
        auto tmp=CJSON_AddObjectToObject(root,i.first.c_str());
        std::string device_id;
        std::string ip;//extranet_ip外网ip
        std::string port;//extranet_port外网端口
        std::string local_ip;//本地ip
        int64_t alive_time;
        std::string stream_server_id;//流媒体服务器ID
        CJSON_AddStringToObject(tmp,"ip",i.second.ip.c_str());
        CJSON_AddStringToObject(tmp,"port",i.second.port.c_str());
        CJSON_AddStringToObject(tmp,"local_ip",i.second.local_ip.c_str());
        CJSON_AddStringToObject(tmp,"alive_time",std::to_string(i.second.alive_time).c_str());
    }
    return root;
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
void p2p_punch_server::release_stream_server(std::string device_id,int stream_server_id)
{
    do{
        auto account=m_id_map.find(stream_server_id);
        if(account==m_id_map.end())break;
        auto stream_info_map=m_stream_server_map.find(account->second);
        if(stream_info_map==m_stream_server_map.end())break;
        auto stream_info=stream_info_map->second.find(stream_server_id);
        if(stream_info==stream_info_map->second.end())break;
        auto load_size=stream_info->second.device_load_size_map.find(device_id);
        if(load_size==stream_info->second.device_load_size_map.end())break;
        stream_info->second.available_load_size+=load_size->second.first;
        stream_info->second.device_load_size_map.erase(load_size);
    }while(0);
}
void p2p_punch_server::update_stream_server_id(std::string device_id,int stream_server_id)
{
        do{
        auto account=m_id_map.find(stream_server_id);
        if(account==m_id_map.end())break;
        auto stream_info_map=m_stream_server_map.find(account->second);
        if(stream_info_map==m_stream_server_map.end())break;
        auto stream_info=stream_info_map->second.find(stream_server_id);
        if(stream_info==stream_info_map->second.end())break;
        auto load_size=stream_info->second.device_load_size_map.find(device_id);
        if(load_size==stream_info->second.device_load_size_map.end())break;
        load_size->second.second=GetTimeNow();
        }while(0);
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
            auto command_name=m_command_map.find(cmd->second);
            if(command_name==std::end(m_command_map))
            {
               handle_not_supported_command(m_server_sock[0],cmd->second,addr);
            }
            else {
                switch (command_name->second) {
                case SERVER_NAT_TYPE_PROBE:
                    handle_nat_type_probe(m_server_sock[0],addr,recv_map);
                    break;
                case SERVER_KEEP_ALIVE:
                    handle_keep_alive(m_server_sock[0],addr,recv_map);
                    break;
                case SERVER_SET_UP_CONNECTION:
                    handle_set_up_connection(m_server_sock[0],addr,recv_map);
                    break;
                case SERVER_PUNCH_HOLE:
                    handle_punch_hole(m_server_sock[0],addr,recv_map);
                    break;
                case SERVER_PUCHN_HOLE_RESPONSE:
                    handle_punch_hole_response(m_server_sock[0],addr,recv_map);
                    break;
                case SERVER_ACTIVE_CONNECTION:
                    handle_active_connection(m_server_sock[0],addr,recv_map);
                    break;
                case SERVER_GET_EXTERNAL_IP:
                    if(m_stream_server_task_init)handle_get_external_ip(m_server_sock[0],addr);
                    break;
                case SERVER_GET_STREAM_SERVER_INFO:
                     if(m_stream_server_task_init)handle_get_stream_server_info(m_server_sock[0],addr,recv_map);
                    break;
                case SERVER_STREAM_SERVER_REGISTER:
                     if(m_stream_server_task_init)handle_stream_server_register(m_server_sock[0],addr,recv_map);
                    break;
                case  SERVER_STREAM_SERVER_KEEPALIVE:
                     if(m_stream_server_task_init)handle_stream_server_keepalive(m_server_sock[0],addr,recv_map);
                    break;
                case  SERVER_RESET_STREAM_STATE:
                    if(m_stream_server_task_init) handle_reset_stream_state();
                    break;
                case  SERVER_GET_ONLINE_DEVICE_INFO:
                    if(m_open_debug)handle_get_online_device_info(m_server_sock[0],addr);
                    break;
                case SERVER_GET_STREAM_SERVER_STATE:
                    if(m_open_debug&&m_stream_server_task_init)handle_get_stream_server_state(m_server_sock[0],addr);
                    break;
                case SERVER_RESTART_STREAM_SERVER:
                    if(m_open_debug&&m_stream_server_task_init)handle_restart_stream_server(m_server_sock[0],addr,recv_map);
                    break;
                case SERVER_RESTART_DEVICE:
                    if(m_open_debug)handle_restart_device(m_server_sock[0],addr,recv_map);
                    break;
                default:
                    break;
                }
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
        if(m_stream_server_task_init)
        {
            auto stream_server_id=recv_map.find("stream_server_id");
            if(stream_server_id!=std::end(recv_map))
            {
                update_stream_server_id(device_id->second,std::stoi(stream_server_id->second));
            }
        }
        m_device_map.insert(std::make_pair(device_id->second,t_nat_info));
    }
    else
    {
        device_nat_info & t_nat_info=nat_info->second;
        t_nat_info.alive_time=this->GetTimeNow();
        t_nat_info.ip=ip;
        t_nat_info.port=port;
        t_nat_info.local_ip=local_ip->second;
        if(m_stream_server_task_init)
        {
            auto stream_server_id=recv_map.find("stream_server_id");
            if(stream_server_id!=std::end(recv_map))
            {
                update_stream_server_id(device_id->second,std::stoi(stream_server_id->second));
            }
        }
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
void p2p_punch_server::handle_get_external_ip(int send_fd,struct sockaddr_in &addr)
{
    std::string response;
    std::string external_ip=inet_ntoa(addr.sin_addr);
    message_handle::packet_buf(response,"cmd","get_external_ip");
    message_handle::packet_buf(response,"external_ip",external_ip);
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_server::handle_get_stream_server_info(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map)
{
    auto device_id=recv_map.find("device_id");
    auto stream_server_id=recv_map.find("stream_server_id");
    auto account_name=recv_map.find("account_name");
    auto load_size=recv_map.find("load_size");
    auto in_use=recv_map.find("in_use");
    std::string response;
    do{
        if(device_id==std::end(recv_map)||stream_server_id==std::end(recv_map)||account_name==std::end(recv_map)\
        ||load_size==std::end(recv_map)||in_use==std::end(recv_map))
        {
            response=packet_error_response("get_stream_server_info","false packet!");
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
            break;
        }
        if(in_use->second=="true")
        {
            auto server_info=m_stream_server_map.find(account_name->second);
            if(server_info==m_stream_server_map.end()||server_info->second.find(std::stoi(stream_server_id->second))==server_info->second.end())
            {
                server_info=m_stream_server_map.find(DEFAULT_SERVER_ACCOUNT);
                if(m_authorized_accounts.find(account_name->second)==m_authorized_accounts.end()\
                ||server_info==m_stream_server_map.end())
                {
                    send_reset_stream_state(send_fd,addr);
                    break;
                }
            }
            auto s_server_info=server_info->second.find(std::stoi(stream_server_id->second));
            if(s_server_info==server_info->second.end())
            {
                send_reset_stream_state(send_fd,addr);
                break;
            }
            auto s_load_size=s_server_info->second.device_load_size_map.find(device_id->second);
            if(s_load_size==s_server_info->second.device_load_size_map.end())
            {
                int i_load_size=std::stoi(load_size->second);
                if(s_server_info->second.available_load_size>=i_load_size)
                {
                    s_server_info->second.available_load_size-=i_load_size;
                    s_server_info->second.device_load_size_map.insert(std::pair<std::string,std::pair<int,int64_t>>(device_id->second,std::pair<int,int64_t>(i_load_size,GetTimeNow())));
                }
                else {
                    send_reset_stream_state(send_fd,addr);
                    break;
                }
            }
            message_handle::packet_buf(response,"cmd","get_stream_server_info");
            message_handle::packet_buf(response,"stream_server_id",std::to_string(s_server_info->second.id));
            message_handle::packet_buf(response,"external_ip",s_server_info->second.external_ip);
            message_handle::packet_buf(response,"external_port",std::to_string(s_server_info->second.stream_external_port));
            message_handle::packet_buf(response,"internal_ip",s_server_info->second.internal_ip);
            message_handle::packet_buf(response,"internal_port",std::to_string(s_server_info->second.stream_internal_port));
            if(s_server_info->second.external_ip==inet_ntoa(addr.sin_addr))
            {
                message_handle::packet_buf(response,"lan_flag","true");
            }
            else {
                message_handle::packet_buf(response,"lan_flag","false");
            }
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
        }
        else {
            auto server_info=m_stream_server_map.find(account_name->second);
            int s_load_size=std::stoi(load_size->second);
            if(s_load_size<=0)break;
            stream_server_info  tmp;
            bool find=false;
            if(server_info!=m_stream_server_map.end())
            {
                for(auto &i : server_info->second)
                {
                    if(i.second.available_load_size>=s_load_size)
                    {
                        tmp=i.second;
                        i.second.available_load_size-=s_load_size;
                         i.second.device_load_size_map.insert(std::pair<std::string,std::pair<int,int64_t>>(device_id->second,std::pair<int,int64_t>(s_load_size,GetTimeNow())));
                        find=true;
                        break;
                    }
                }
            }
            if(!find&&account_name->second!=DEFAULT_SERVER_ACCOUNT&&m_authorized_accounts.find(account_name->second)!=std::end(m_authorized_accounts))
            {
                server_info=m_stream_server_map.find(DEFAULT_SERVER_ACCOUNT);
                if(server_info!=m_stream_server_map.end())
                {
                    for(auto &i : server_info->second)
                    {
                        if(i.second.available_load_size>=s_load_size)
                        {
                            tmp=i.second;
                            i.second.available_load_size-=s_load_size;
                            i.second.device_load_size_map.insert(std::pair<std::string,std::pair<int,int64_t>>(device_id->second,std::pair<int,int64_t>(s_load_size,GetTimeNow())));
                            find=true;
                            break;
                        }
                    }
                }
            }
            message_handle::packet_buf(response,"cmd","get_stream_server_info");
            if(!find)
            {
                message_handle::packet_buf(response,"stream_server_id","-1");
            }
            else {
                message_handle::packet_buf(response,"stream_server_id",std::to_string(tmp.id));
                message_handle::packet_buf(response,"external_ip",tmp.external_ip);
                message_handle::packet_buf(response,"external_port",std::to_string(tmp.stream_external_port));
                message_handle::packet_buf(response,"internal_ip",tmp.internal_ip);
                message_handle::packet_buf(response,"internal_port",std::to_string(tmp.stream_internal_port));
                if(tmp.external_ip==inet_ntoa(addr.sin_addr))
                {
                    message_handle::packet_buf(response,"lan_flag","true");
                }
                else {
                    message_handle::packet_buf(response,"lan_flag","false");
                }
            }
            ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
        }
    }while(0);
}
void p2p_punch_server::handle_stream_server_register(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map)
{
    int stream_server_id=-1;
    auto account_name=recv_map.find("account_name");
    auto max_load_size=recv_map.find("max_load_size");
    if(account_name==std::end(recv_map)||max_load_size==std::end(recv_map))return;
    if(m_stream_server_id<0)m_stream_server_id=1;
    while(m_stream_server_id>0)
    {
        if(m_id_map.find(m_stream_server_id)==m_id_map.end())
        {
            stream_server_id=m_stream_server_id++;
            break;
        }
        m_stream_server_id++;
    }
    std::string response;
    message_handle::packet_buf(response,"cmd","stream_server_register");
    if(stream_server_id>0)
    {
        auto &stream_server_map=m_stream_server_map[account_name->second];
        stream_server_info info=stream_server_info();
        info.id=stream_server_id;
        info.available_load_size=std::stoi(max_load_size->second);
        info.last_alive_time=GetTimeNow();
        info.external_ip=inet_ntoa(addr.sin_addr);
        info.external_port=ntohs(addr.sin_port);
        m_id_map[stream_server_id]=account_name->second;
        stream_server_map.emplace(std::pair<int,stream_server_info>(stream_server_id,info));
        message_handle::packet_buf(response,"stream_server_id",std::to_string(stream_server_id));
    }
    else {
        message_handle::packet_buf(response,"stream_server_id","-1");
    }
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_server::handle_stream_server_keepalive(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map)
{
    auto stream_server_id=recv_map.find("stream_server_id");
    auto external_port=recv_map.find("external_port");
    auto internal_port=recv_map.find("internal_port");
    auto internal_ip=recv_map.find("internal_ip");
    std::string response;
    do{
        if(stream_server_id==recv_map.end()||external_port==recv_map.end()\
        ||internal_ip==recv_map.end()||internal_port==recv_map.end())break;
        auto account=m_id_map.find(std::stoi(stream_server_id->second));
        if(account==m_id_map.end())break;
        auto stream_server_map=m_stream_server_map.find(account->second);
        if(stream_server_map==m_stream_server_map.end())break;
        auto stream_server=stream_server_map->second.find(std::stoi(stream_server_id->second));
        if(stream_server==stream_server_map->second.end())break;
        stream_server->second.external_ip=inet_ntoa(addr.sin_addr);
        stream_server->second.external_port=ntohs(addr.sin_port);
        stream_server->second.last_alive_time=GetTimeNow();
        stream_server->second.stream_external_port=std::stoi(external_port->second);
        stream_server->second.internal_ip=internal_ip->second;
        stream_server->second.stream_internal_port=std::stoi(internal_port->second);
        message_handle::packet_buf(response,"cmd","stream_server_keepalive");
        ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
        return;
    }while(0);
    message_handle::packet_buf(response,"cmd","restart_stream_server");
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_server::send_reset_stream_state(int send_fd,struct sockaddr_in&addr)
{
    std::string response;
    message_handle::packet_buf(response,"cmd","reset_stream_state");
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_server::handle_get_online_device_info(int send_fd,struct sockaddr_in&addr)
{
    std::string response;
    message_handle::packet_buf(response,"cmd","get_online_device_info");
    char *info=nullptr;
    CJSON *online_device_info=get_online_device_info();
    info=CJSON_Print(online_device_info);
    CJSON_Delete(online_device_info);
    if(info)
    {
        message_handle::packet_buf(response,"info",info);
        CJSON_free(info);
    }
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_server::handle_get_stream_server_state(int send_fd,struct sockaddr_in&addr)
{
    std::string response;
    message_handle::packet_buf(response,"cmd","get_stream_server_state");
    char *info=nullptr;
    CJSON *stream_server_state=get_stream_server_state();
    info=CJSON_Print(stream_server_state);
    CJSON_Delete(stream_server_state);
    if(info)
    {
        message_handle::packet_buf(response,"info",info);
        CJSON_free(info);
    }
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
}
void p2p_punch_server::handle_restart_stream_server(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map)
{
    std::string response;
    message_handle::packet_buf(response,"cmd","restart_stream_server");
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
    do{
        auto stream_server_id=recv_map.find("stream_server_id");
        if(stream_server_id==recv_map.end())break;
        int s_stream_server_id=std::stoi(stream_server_id->second);
        std::cout<<s_stream_server_id<<"--------------"<<std::endl;
        if(s_stream_server_id<1)break;
        auto account=m_id_map.find(s_stream_server_id);
        if(account==m_id_map.end())break;
        std::string account_name=account->second;
        m_id_map.erase(s_stream_server_id);
        auto stream_server_map=m_stream_server_map.find(account_name);
        if(stream_server_map==m_stream_server_map.end())break;
        auto stream_server_info=stream_server_map->second.find(s_stream_server_id);
        struct sockaddr_in addr = { 0 };
        socklen_t addrlen = sizeof(addr);
        addr.sin_family = AF_INET;
        for(auto i : stream_server_info->second.device_load_size_map)
        {//remove online device
            auto device_info=m_device_map.find(i.first);
            if(device_info==std::end(m_device_map))continue;
            addr.sin_port = htons(std::stoi(device_info->second.port));
            addr.sin_addr.s_addr = inet_addr(device_info->second.ip.c_str());
            send_reset_stream_state(m_server_sock[0],addr);
        }
        addr.sin_port = htons(stream_server_info->second.external_port);
        addr.sin_addr.s_addr = inet_addr(stream_server_info->second.external_ip.c_str());
        stream_server_map->second.erase(stream_server_info);
        ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
    }while(0);
}
void p2p_punch_server::handle_restart_device(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map)
{
    std::string response;
    message_handle::packet_buf(response,"cmd","restart_device");
    ::sendto(send_fd,response.c_str(),response.length(),0,(struct sockaddr*)&addr, sizeof addr);
    auto device_id=recv_map.find("device_id");
    if(device_id==std::end(recv_map))return;
    auto device_info=m_device_map.find(device_id->second);
    if(device_info==m_device_map.end())return;
    addr.sin_port = htons(std::stoi(device_info->second.port));
    addr.sin_addr.s_addr = inet_addr(device_info->second.ip.c_str());
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
