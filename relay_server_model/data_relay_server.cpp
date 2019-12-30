#include "data_relay_server.h"
#include "assert.h"
#include "TcpSocket.h"
#include "parase_request.h"
#include "encryption_context.h"
using namespace micagent;
typedef enum{
    SUCCESS,
    UNDEFINED,
    EVENT_LOOP_BE_NULL,
    PORT_ILLEGAL,
}Error_State;
Data_Relay_Server::Data_Relay_Server(shared_ptr<EventLoop>loop,uint16_t port)
{
    m_is_exited=false;
    m_init_ok=false;
    m_error_string="success!";
    do{
        if(!loop)
        {
            m_error_string="loop is nullptr!";
            break;
        }
        m_event_loop=loop;
        if(port<1)
        {
            m_error_string="port is illegal!";
            break;
        }
        //udp 部分初始化
        int udp_fd=socket(AF_INET,SOCK_DGRAM,0);
        assert(udp_fd>0);
        xop::SocketUtil::setNonBlock(udp_fd);
        xop::SocketUtil::setReuseAddr(udp_fd);
        xop::SocketUtil::setReusePort(udp_fd);
        xop::SocketUtil::setSendBufSize(udp_fd, 32*1024);
        xop::SocketUtil::setRecvBufSize(udp_fd,32*1024);
        if(!xop::SocketUtil::bind(udp_fd,"0.0.0.0",port))
        {
            m_error_string="udp sock bind error!";
            m_error_string+="port : "+std::to_string(port);
            SocketUtil::close(udp_fd);
            break;
        }
        m_udp_channel.reset(new Channel(udp_fd));
        m_udp_channel->setReadCallback([this](){
            this->udp_handle_read();
        });
        m_udp_channel->enableReading();
        m_event_loop->updateChannel(m_udp_channel);
        //tcp部分初始化
        m_tcp_acceptor.reset(new xop::Acceptor(m_event_loop.get(),"0.0.0.0",port));
        m_tcp_acceptor->setNewConnectionCallback([this](int32_t fd){
            if(fd>0)
            {
                ChannelPtr new_channel(new xop::Channel(fd));
                new_channel->setReadCallback([new_channel,this](){
                    uint8_t first_packet[512];
                    ssize_t len=::recv(new_channel->fd(),first_packet,512,0);
                    Udp_Data_Header header;
                    this->m_tcp_tmp_set.erase(new_channel);
                    this->m_event_loop->removeChannel(const_cast<std::shared_ptr<xop::Channel>&>(new_channel));
                    if(UnpackDataHeader(first_packet,len,header))tcp_handle_new_session(header,new_channel);
                });
                new_channel->enableReading();
                m_event_loop->updateChannel(new_channel);
                m_tcp_tmp_set.insert(new_channel);
            }
        });
        m_check_timer=m_event_loop->addTimer([this](){
            this->remove_invalid_resources();
            return true;
        },CHECK_INTERVAL);
        m_init_ok=true;
    }while(0);
}
Data_Relay_Server::~Data_Relay_Server()
{
    m_is_exited=true;
    if(m_check_timer>0)m_event_loop->removeTimer(m_check_timer);
    if(m_udp_channel)m_event_loop->removeChannel(m_udp_channel);
    if(m_tcp_acceptor)m_tcp_acceptor.reset();
    for(auto i:m_session_map)
    {
        if(i.second->type==BY_TCP)
        {
            if(i.second->channel_peer1)m_event_loop->removeChannel(i.second->channel_peer1);
            if(i.second->channel_peer2)m_event_loop->removeChannel(i.second->channel_peer2);
        }
    }
    for(auto i:m_tcp_tmp_set)m_event_loop->removeChannel(i);
}
bool Data_Relay_Server::AddTunnelSession(uint32_t session_id,uint32_t call_id)
{
    if(m_is_exited||!m_init_ok)return false;
    std::lock_guard<std::mutex> locker(m_mutex);
    std::shared_ptr<Data_session> session(new Data_session(session_id,call_id,GetTimeNow()));
    m_session_map.emplace(std::pair<int,std::shared_ptr<Data_session>>(session_id,session));
    return true;
}
void Data_Relay_Server::RemoveTunnelSession(uint32_t session_id)
{
    if(m_is_exited||!m_init_ok)return ;
    std::lock_guard<std::mutex> locker(m_mutex);
    do_remove_tunnel_session(session_id);
}
const std::string Data_Relay_Server::GetErrorInfo()
{
    if(m_is_exited||!m_init_ok)return nullptr;
    std::lock_guard<std::mutex> locker(m_error_mutex);
    return m_error_string;
}
Udp_Data_Header Data_Relay_Server::PackDataHeader(uint32_t session_id,bool is_peer1,Data_Function_Code func_code)
{
    Udp_Data_Header header;
    header.session_id=session_id;
    header.function_code=func_code;
    header.peer_mask=is_peer1?0:1;
    header.check_sum=((session_id>>16)^session_id)|(header.peer_mask<<8)|(header.function_code<<10);
    return header;
}
bool Data_Relay_Server::UnpackDataHeader(void *buf,size_t len,Udp_Data_Header &save)
{
    bool ret=false;
    do{
        if(len<sizeof (Udp_Data_Header))break;
        memcpy(&save,buf,sizeof (Udp_Data_Header));
        uint32_t check_sum=((save.session_id>>16)^save.session_id)|(save.peer_mask<<8)|(save.function_code<<10);
        if(check_sum!=save.check_sum)break;
        ret=true;
    }while(0);
    return ret;
}
void Data_Relay_Server::Set_Close_State(bool is_closed)
{
    m_is_exited=is_closed;
    std::lock_guard<std::mutex> locker(m_error_mutex);
    if(is_closed)m_error_string="sever is closed!";
    else {
        m_error_string="sever is open!";
    }
}
void Data_Relay_Server::udp_handle_read()
{
    if(m_is_exited||!m_init_ok)return;
    static const size_t buf_len=32*1024;
    uint8_t udp_buf[buf_len]={0};
    struct sockaddr_in addr={0};
    socklen_t len=0;
    auto read_len=::recvfrom(m_udp_channel->fd(),udp_buf,buf_len,0,(struct sockaddr *)&addr,&len);
    if(read_len>0)
    {
        Udp_Data_Header header;
        if(UnpackDataHeader(udp_buf,read_len,header))
        {
            switch(header.function_code)
            {
            case CLOSE_TRANSFER:
                udp_handle_close(header);
                break;
            case START_TRANSFER:
                udp_handle_new_session(header,addr);
                break;
            case DATA:
                udp_handle_data(header,udp_buf,read_len);
                break;
            default:
                break;
            }
        }
    }
}
void Data_Relay_Server::udp_handle_data(Udp_Data_Header &header,const void *buf,ssize_t buf_len)
{
    std::lock_guard<std::mutex>locker(m_mutex);
    auto iter=m_session_map.find(header.session_id);
    do{
        if(iter==std::end(m_session_map)||iter->second->connection_state!=3)break;
        iter->second->last_alive_time=GetTimeNow();
        auto send_len=buf_len-sizeof(header);
        if(send_len>0)
        {
            if(header.peer_mask==0)
            {
                ::sendto(m_udp_channel->fd(),static_cast<const uint8_t *>(buf)+sizeof(header),send_len,0,(struct sockaddr *)&iter->second->addr_peer2,sizeof (struct sockaddr_in));
            }
            else {
                ::sendto(m_udp_channel->fd(),static_cast<const uint8_t *>(buf)+sizeof(header),send_len,0,(struct sockaddr *)&iter->second->addr_peer1,sizeof (struct sockaddr_in));
            }
        }
    }while(0);
}
void Data_Relay_Server::udp_handle_close(Udp_Data_Header &header)
{
    std::lock_guard<std::mutex>locker(m_mutex);
    do{
        auto session=m_session_map.find(header.session_id);
        if(session==std::end(m_session_map)||session->second->connection_state!=3)break;
        session->second->last_alive_time=GetTimeNow();
        session->second->last_alive_time=GetTimeNow();
        std::string ack;
        message_handle::packet_buf(ack,"cmd","close_tunnel_transfer");
        message_handle::packet_buf(ack,"session_Id",std::to_string(session->second->session_id));
        message_handle::packet_buf(ack,"call_id",std::to_string(session->second->call_id));
        Raw_Packet packet;
        packet.private_data_len=ack.length();
        packet.private_data.reset(new uint8_t[packet.payload_data_len]);
        memcpy(packet.private_data.get(),ack.c_str(),packet.private_data_len);
        auto buf_send=Encryption_Tool::Instance().Encryption_Data(1,packet);
        ::sendto(m_udp_channel->fd(),buf_send.data.get(),buf_send.packet_len,0,(struct sockaddr *)&session->second->addr_peer1,sizeof (struct sockaddr_in));
        ::sendto(m_udp_channel->fd(),buf_send.data.get(),buf_send.packet_len,0,(struct sockaddr *)&session->second->addr_peer2,sizeof (struct sockaddr_in));
        m_session_map.erase(session);
    }while(0);
}
void Data_Relay_Server::tcp_handle_read(int32_t fd)
{
    static const size_t buf_len=32*1024;
    if(m_is_exited||!m_init_ok)return ;
    do{
        auto session_id=m_tcp_fd_map.find(fd);
        if(session_id==std::end(m_tcp_fd_map))break;
        auto session=m_session_map.find(session_id->second);
        if(session==std::end(m_session_map)||session->second->connection_state!=3)break;
        uint8_t tcp_buf[buf_len]={0};
        auto recv_len=recv(fd,tcp_buf,buf_len,0);
        if(recv_len<=0)
        {
            tcp_handle_close_session(session->second);
        }
        else {
            session->second->last_alive_time=GetTimeNow();
            if(fd==session->second->channel_peer1->fd())
            {
                send(session->second->channel_peer2->fd(),tcp_buf,recv_len,0);
            }
            else if (fd==session->second->channel_peer2->fd()) {
                send(session->second->channel_peer1->fd(),tcp_buf,recv_len,0);
            }
            else {
                /*unknown error*/
            }
        }
    }while(0);
}
void Data_Relay_Server::do_remove_tunnel_session(uint32_t session_id)
{
    if(m_is_exited||!m_init_ok)return;
    auto session=m_session_map.find(session_id);
    if(session!=std::end(m_session_map))
    {
        if(session->second->type==BY_TCP)
        {
            if(session->second->channel_peer1)
            {
                m_tcp_fd_map.erase(session->second->channel_peer1->fd());
                m_event_loop->removeChannel(session->second->channel_peer1);
            }
            if(session->second->channel_peer2)
            {
                m_tcp_fd_map.erase(session->second->channel_peer2->fd());
                m_event_loop->removeChannel(session->second->channel_peer2);
            }
        }
        m_session_map.erase(session);
    }
}
void Data_Relay_Server::remove_invalid_resources()
{
    if(m_is_exited||!m_init_ok)return;

    int64_t time_now=GetTimeNow();
    std::lock_guard<std::mutex> locker(m_mutex);
    for(auto i=std::begin(m_session_map);i!=std::end(m_session_map);)
    {
        if(time_now-i->second->last_alive_time>MAX_SESSION_CACHE_TIME)
        {
            if(i->second->type==BY_TCP)
            {
                if(i->second->channel_peer1)m_event_loop->removeChannel(i->second->channel_peer1);
                if(i->second->channel_peer2)m_event_loop->removeChannel(i->second->channel_peer2);
            }
            m_session_map.erase(i++);
        }
        else
        {
            i++;
        }
    }
}
void Data_Relay_Server::udp_handle_new_session(Udp_Data_Header &header,struct sockaddr_in &sockaddr)
{
    std::lock_guard<std::mutex>locker(m_mutex);
    do{
        auto session=m_session_map.find(header.session_id);
        if(session==std::end(m_session_map))break;
        if(session->second->connection_state==0)session->second->type=BY_UDP;
        if(session->second->type!=BY_UDP||session->second->connection_state==3)break;
        if(header.peer_mask==0)
        {
            if(session->second->connection_state==1)break;
            session->second->addr_peer1=sockaddr;
            session->second->connection_state+=1;
        }
        else {
            if(session->second->connection_state==2)break;
            session->second->addr_peer2=sockaddr;
            session->second->connection_state+=2;
        }
        if(session->second->connection_state==3)
        {
            session->second->last_alive_time=GetTimeNow();
            std::string ack;
            message_handle::packet_buf(ack,"cmd","start_tunnel_transfer");
            message_handle::packet_buf(ack,"session_Id",std::to_string(session->second->session_id));
            message_handle::packet_buf(ack,"call_id",std::to_string(session->second->call_id));
            Raw_Packet packet;
            packet.private_data_len=ack.length();
            packet.private_data.reset(new uint8_t[packet.payload_data_len]);
            memcpy(packet.private_data.get(),ack.c_str(),packet.private_data_len);
            auto buf_send=Encryption_Tool::Instance().Encryption_Data(1,packet);
            ::sendto(m_udp_channel->fd(),buf_send.data.get(),buf_send.packet_len,0,(struct sockaddr *)&session->second->addr_peer1,sizeof (struct sockaddr_in));
            ::sendto(m_udp_channel->fd(),buf_send.data.get(),buf_send.packet_len,0,(struct sockaddr *)&session->second->addr_peer2,sizeof (struct sockaddr_in));
        }
    }while(0);
}
void Data_Relay_Server::tcp_handle_new_session(Udp_Data_Header &header,ChannelPtr channel)
{
    if(m_is_exited||!m_init_ok)return;
    std::lock_guard<std::mutex>locker(m_mutex);
    m_tcp_fd_map[channel->fd()]=header.session_id;
    do{
        auto session=m_session_map.find(header.session_id);
        if(session==std::end(m_session_map))break;
        if(session->second->connection_state==0)session->second->type=BY_TCP;
        if(session->second->type!=BY_TCP||session->second->connection_state==3)break;
        if(header.peer_mask==0)
        {
            if(session->second->connection_state==1)break;
            session->second->channel_peer1=channel;
            session->second->connection_state+=1;
        }
        else {
            if(session->second->connection_state==2)break;
            session->second->channel_peer2=channel;
            session->second->connection_state+=2;
        }
        if(session->second->connection_state==3)
        {
            int fd1=session->second->channel_peer1->fd();
            int fd2=session->second->channel_peer2->fd();
            session->second->channel_peer1->setReadCallback([fd1,this](){
                tcp_handle_read(fd1);
            });
            session->second->channel_peer2->setReadCallback([fd2,this](){
                tcp_handle_read(fd2);
            });
            session->second->channel_peer1->enableReading();
            session->second->channel_peer2->enableReading();
            m_event_loop->updateChannel(session->second->channel_peer1);
            m_event_loop->updateChannel(session->second->channel_peer2);

            session->second->last_alive_time=GetTimeNow();
            std::string ack;
            message_handle::packet_buf(ack,"cmd","start_tunnel_transfer");
            message_handle::packet_buf(ack,"session_Id",std::to_string(session->second->session_id));
            message_handle::packet_buf(ack,"call_id",std::to_string(session->second->call_id));
            Raw_Packet packet;
            packet.private_data_len=ack.length();
            packet.private_data.reset(new uint8_t[packet.payload_data_len]);
            memcpy(packet.private_data.get(),ack.c_str(),packet.private_data_len);
            auto buf_send=Encryption_Tool::Instance().Encryption_Data(1,packet);
            ::send(session->second->channel_peer1->fd(),buf_send.data.get(),buf_send.packet_len,0);
            ::send(session->second->channel_peer2->fd(),buf_send.data.get(),buf_send.packet_len,0);
        }
    }while(0);
}
void Data_Relay_Server::tcp_handle_close_session( std::shared_ptr<Data_session> session)
{
    if(!session)return;
    if(session->channel_peer1){
        m_tcp_fd_map.erase(session->channel_peer1->fd());
        m_event_loop->removeChannel(session->channel_peer1);
    }
    if(session->channel_peer2){
        m_tcp_fd_map.erase(session->channel_peer2->fd());
        m_event_loop->removeChannel(session->channel_peer2);
    }
    m_session_map.erase(session->session_id);
}
