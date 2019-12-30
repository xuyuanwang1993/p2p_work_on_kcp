#include "kcp_manage.h"
#include <mutex>
#include <string>
#include <stdint.h>
#include <utility>
#include <chrono>
#include <future>
//#define TRY_QUICK_SEND
using namespace sensor_net;
static  int kcp_output(const char *buf, int len, struct IKCPCB *kcp, void *user)
{
    (void *)(kcp);
    KCP_Interface *interface=(KCP_Interface *)user;
    return interface->Send(buf,len);
}

KCP_Interface::KCP_Interface(int fd,unsigned int conv_id,xop::EventLoop *event_loop,ClearCallback clearCB,RecvDataCallback recvCB,std::shared_ptr<data_ptr>data,int window_size)
    :m_readBufferPtr(new xop::BufferReader),m_udp_channel(new xop::Channel(fd)),m_clear_CB(clearCB),m_recvCB(recvCB),m_data(data),m_fd(fd)
{
    m_taskScheduler=event_loop?event_loop->getTaskScheduler():nullptr;
    m_kcp=ikcp_create(conv_id,this);
    window_size=window_size>MAX_WINDOW_SIZE?MAX_WINDOW_SIZE:window_size;
    ikcp_wndsize(m_kcp, window_size, window_size);
    SetTransferMode(FAST_MODE);
    m_kcp->output=kcp_output;
    m_kcp->user=this;
    m_udp_channel->enableReading();
    m_have_cleared=false;
    //读到的数据输入kcp进行处理
    m_udp_channel->setReadCallback([this](){
        if(this->m_have_cleared)return;
        //接收数据
        int ret=this->m_readBufferPtr->readFd(this->m_udp_channel->fd());
        if(ret<=0)
        {
            this->clear();
            return;
        }
        this->m_last_alive_time=KCP_Manager::GetTimeNow();
        if(this->m_kcp)
        {
            int size=this->m_readBufferPtr->readableBytes();
            if(size>0)
            {
                if(ikcp_input(this->m_kcp,this->m_readBufferPtr->peek(),size)==0)
                {
                    ikcp_flush(this->m_kcp);
                    std::shared_ptr<char> frame_data(new char[MAX_FRAMESIZE]);
                    int recv_len=ikcp_recv(this->m_kcp,frame_data.get(),MAX_FRAMESIZE);
                    if(recv_len>0&&this->m_recvCB)
                    {
                        this->m_recvCB(frame_data,recv_len,this->m_kcp,this->m_data);
                    }
                }
                //清空buff
                this->m_readBufferPtr->retrieveAll();
            }
        }
    });
    m_udp_channel->setWriteCallback([this](){
        if(this->m_have_cleared)return;
        ikcp_flush(this->m_kcp);
    });
    m_udp_channel->setCloseCallback([this](){
        if(this->m_have_cleared)return;
        this->clear();
    });
    m_udp_channel->setErrorCallback([this](){
        if(this->m_have_cleared)return;
        this->clear();
    });
    m_taskScheduler->updateChannel(m_udp_channel);
}
KCP_Interface::KCP_Interface(std::shared_ptr<xop::Channel>channel,unsigned int conv_id,xop::EventLoop *event_loop,ClearCallback clearCB,RecvDataCallback recvCB,std::shared_ptr<data_ptr>data,int window_size)
    :m_readBufferPtr(new xop::BufferReader),m_udp_channel(channel),m_clear_CB(clearCB),m_recvCB(recvCB),m_data(data)
{
    m_fd=channel->fd();
    m_taskScheduler=event_loop?event_loop->getTaskScheduler():nullptr;
    m_kcp=ikcp_create(conv_id,this);
    window_size=window_size>MAX_WINDOW_SIZE?MAX_WINDOW_SIZE:window_size;
    ikcp_wndsize(m_kcp, window_size, window_size);
    SetTransferMode(FAST_MODE);
    m_kcp->output=kcp_output;
    m_udp_channel->enableReading();
    m_have_cleared=false;
    //读到的数据输入kcp进行处理
    m_udp_channel->setReadCallback([this](){
        if(this->m_have_cleared)return;
        //接收数据
        int ret=this->m_readBufferPtr->readFd(this->m_udp_channel->fd());
        if(ret<=0)
        {
            this->clear();
            return;
        }
        this->m_last_alive_time=KCP_Manager::GetTimeNow();
        if(this->m_kcp)
        {
            int size=this->m_readBufferPtr->readableBytes();
            if(size>0)
            {
                if(ikcp_input(this->m_kcp,this->m_readBufferPtr->peek(),size)==0)
                {
                    ikcp_flush(this->m_kcp);
                    std::shared_ptr<char> frame_data(new char[MAX_FRAMESIZE]);
                    int recv_len=ikcp_recv(this->m_kcp,frame_data.get(),MAX_FRAMESIZE);
                    if(recv_len>0&&this->m_recvCB)
                    {
                        this->m_recvCB(frame_data,recv_len,this->m_kcp,this->m_data);
                    }
                }
                //清空buff
                this->m_readBufferPtr->retrieveAll();
            }
        }
    });
    m_udp_channel->setWriteCallback([this](){
        if(this->m_have_cleared)return;
        ikcp_flush(this->m_kcp);
    });
    m_udp_channel->setCloseCallback([this](){
        if(this->m_have_cleared)return;
        this->clear();
    });
    m_udp_channel->setErrorCallback([this](){
        if(this->m_have_cleared)return;
        this->clear();
    });
    m_taskScheduler->updateChannel(m_udp_channel);
}

void KCP_Interface::SetTransferMode(KCP_TRANSFER_MODE mode,int nodelay, int interval, int resend, int nc)
{
    switch(mode)
    {
    case DEFULT_MODE:
        ikcp_nodelay(m_kcp, 0, 10, 0, 0);
        break;
    case NORMAL_MODE:
        ikcp_nodelay(m_kcp, 0, 10, 0, 1);
        break;
    case FAST_MODE:
        ikcp_nodelay(m_kcp, 1, 10, 2, 1);
        break;
    case CUSTOM_MODE:
        ikcp_nodelay(m_kcp, nodelay, interval, resend, nc);
        break;
    default:
        std::cerr<<"This transmode isn't been supported!"<<std::endl;
    }
}

void KCP_Interface::Send_Userdata(std::shared_ptr<char>buf,int len)
{
#ifndef TRY_QUICK_SEND
    m_taskScheduler->addTriggerEvent([this,buf,len](){
#endif
        if(this->CheckTransWindow())
        {
            std::cout<<"kcp_send : size"<<len<<" channel_id :"<<this->m_data->channel_id<<" src_name "<<this->m_data->src_name<<\
            " conv_id "<<m_kcp->conv<<std::endl;
            ikcp_send(this->m_kcp,buf.get(),len);
        }
        else
        {
            if(m_lost_connectionCB)
            {
                int64_t time_now=KCP_Manager::GetTimeNow();
                if(time_now-m_last_alive_time>MAX_TIMEOUT_TIME)
                {//判断对端是否断开
                    m_lost_connectionCB(m_data);
                }
            }
        }
#ifndef TRY_QUICK_SEND
    });
#endif
}
void KCP_Interface::Send_Userdata(const char *buf,int len)
{
#ifndef TRY_QUICK_SEND
    m_taskScheduler->addTriggerEvent([this,buf,len](){
#endif
        if(this->CheckTransWindow())
        {
            ikcp_send(this->m_kcp,buf,len);
        }
        else
        {
            if(m_lost_connectionCB)
            {
                int64_t time_now=KCP_Manager::GetTimeNow();
                if(time_now-m_last_alive_time>MAX_TIMEOUT_TIME)
                {//判断对端是否断开
                    m_lost_connectionCB(m_data);
                }
            }
        }
#ifndef TRY_QUICK_SEND
    });
#endif
}

void KCP_Interface::Send_Userdata(std::shared_ptr<uint8_t>buf,int len)
{
#ifndef TRY_QUICK_SEND
    m_taskScheduler->addTriggerEvent([this,buf,len](){
#endif
        if(this->CheckTransWindow())
        {
            ikcp_send(this->m_kcp,(const char *)buf.get(),len);
        }
        else
        {
            if(m_lost_connectionCB)
            {
                int64_t time_now=KCP_Manager::GetTimeNow();
                if(time_now-m_last_alive_time>MAX_TIMEOUT_TIME)
                {//判断对端是否断开
                    m_lost_connectionCB(m_data);
                }
            }
        }
#ifndef TRY_QUICK_SEND
    });
#endif
}
int KCP_Interface::Send(const char *buf,int len)
{
    if(m_have_cleared)return -1;
    //std::cout<<"send size "<<len<<std::endl;
    return ::send(this->m_udp_channel->fd(),buf,len,0);
}
void KCP_Interface::clear()
{
    std::cout<<"clear KCP Interface"<<std::endl;
    if(!m_have_cleared)
    {
        m_have_cleared=true;
        if (!m_taskScheduler->addTriggerEvent([this]() {
                                              if(this->m_clear_CB)
        {
                                              this->m_clear_CB();
    }
    }))
        {
            this->m_taskScheduler->addTimer([this]() {
                if(this->m_clear_CB)
                {
                    this->m_clear_CB();
                }
                ; return false;}, 1);
        }
    }
}
bool KCP_Interface::CheckTransWindow()
{
    if(m_kcp->rmt_wnd>m_kcp->snd_wnd)ikcp_wndsize(m_kcp,m_kcp->rmt_wnd,m_kcp->rcv_wnd);//更改发送窗大小
    if(ikcp_waitsnd(m_kcp)>=2*m_kcp->snd_wnd)
    {
        int tmp_wnd=2*m_kcp->snd_wnd;
        tmp_wnd=tmp_wnd>=MAX_WINDOW_SIZE?MAX_WINDOW_SIZE:tmp_wnd;
        ikcp_wndsize(m_kcp,tmp_wnd,m_kcp->rcv_wnd);
        return false;//缓存积累
    }
    return true;
}

KCP_Interface::~KCP_Interface()
{
    if(this->m_udp_channel)
    {
        this->m_taskScheduler->removeChannel(this->m_udp_channel);
    }
    ikcp_release(m_kcp);
}
std::shared_ptr<KCP_Interface> KCP_Manager::AddConnection(int fd,std::string ip,int port,unsigned int conv_id,RecvDataCallback recvCB,std::shared_ptr<data_ptr>data,int window_size)
{
    if(!m_init||fd<0)return nullptr;
    std::cout<<"AddConnection"<<std::endl;
    std::lock_guard<std::mutex> locker(m_mutex);
    struct sockaddr_in addr = { 0 };
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    ::connect(fd, (struct sockaddr*)&addr, addrlen);//绑定对端信息
    std::shared_ptr<KCP_Interface> interface(new KCP_Interface(fd,conv_id,m_event_loop,[conv_id,this](){
        this->CloseConnection(conv_id);
    },recvCB,data,window_size));
    m_kcp_map.insert(std::make_pair(conv_id,interface));
    return interface;
}
std::shared_ptr<KCP_Interface> KCP_Manager::AddConnection(std::shared_ptr<xop::Channel> channel,unsigned int conv_id,RecvDataCallback recvCB,std::shared_ptr<sensor_net::data_ptr>data,int window_size)
{
    if(!m_init)return nullptr;
    std::cout<<"AddConnection"<<std::endl;
    std::lock_guard<std::mutex> locker(m_mutex);
    int fd=channel->fd();
    std::shared_ptr<KCP_Interface> interface(new KCP_Interface(channel,conv_id,m_event_loop,[conv_id,this](){
        this->CloseConnection(conv_id);
    },recvCB,data,window_size));
    m_kcp_map.insert(std::make_pair(conv_id,interface));
    return interface;
}

void KCP_Manager::StartUpdateLoop()
{
    m_event_loop->getTaskScheduler()->addTimer(std::bind(&sensor_net::KCP_Manager::UpdateLoop,this),10);
}
void KCP_Manager::CloseConnection(int conv_id)
{//移除连接
    if(!m_init)return ;
    std::lock_guard<std::mutex> locker(m_mutex);
    if(m_kcp_map.find(conv_id)!=std::end(m_kcp_map))
    {
        m_kcp_map.erase(conv_id);
    }
}
bool KCP_Manager::UpdateLoop()
{
    std::unordered_map<int,std::shared_ptr<KCP_Interface>> tmp_map;
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        tmp_map=m_kcp_map;
    }
    for(auto i : tmp_map)
    {
        i.second->Update(KCP_Manager::GetTimeNow());
    }
    return true;
}
