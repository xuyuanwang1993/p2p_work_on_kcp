#include "kcp_test.h"
#include <thread>
#include <random>
#include <memory>
#include "h264_file_handle.h"
#include "TcpSocket.h"
#include "SocketUtil.h"
#include "parase_request.h"
void kcp_test::kcp_callback(std::shared_ptr<xop::Channel> channel,int session_id,int channel_id,std::string src_name)
{
    std::shared_ptr<sensor_net::data_ptr> data(new sensor_net::data_ptr(channel_id,src_name));
    sensor_net::KCP_Manager::GetInstance().AddConnection(channel,session_id,&kcp_test::kcp_recvcallback,data);
}
void kcp_test::kcp_send_test(std::shared_ptr<xop::Channel> channel,int session_id,int channel_id,std::string src_name)
{
    std::shared_ptr<sensor_net::data_ptr> data(new sensor_net::data_ptr(channel_id,src_name));
    std::shared_ptr<sensor_net::KCP_Interface> interface=sensor_net::KCP_Manager::GetInstance().AddConnection(channel,session_id,&kcp_test::kcp_recvcallback2,data);
    std::thread t(&kcp_test::send_thread,interface);
    t.detach();
}

void kcp_test::kcp_recvcallback(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,std::shared_ptr<sensor_net::data_ptr>data)
{
    //std::cout<<"conv_id : "<<kcp->conv<<" recv_len :"<<len<<"    info :"<<data->channel_id<<"   "<<data->src_name<<std::endl;
    ikcp_send(kcp,buf.get(),len);
}
void kcp_test::kcp_recvcallback2(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,std::shared_ptr<sensor_net::data_ptr>data)
{
    std::cout<<"conv_id : "<<kcp->conv<<" recv_len :"<<len<<"    info :"<<data->channel_id<<"   "<<data->src_name<<std::endl;
}
void kcp_test::send_thread(std::shared_ptr<sensor_net::KCP_Interface> kcp_interface)
{
    H264File h264File;
    char filename[]="../1080P.h264";
    if(!h264File.open(filename))
    {
        printf("Open %s failed.\n", filename);
        return;
    }
    std::cout<<"send_thread"<<std::endl;
#if 0
    std::shared_ptr<char> buf(new char[10]);
    char * tmp=buf.get();
    memset(tmp,'#',10);
    std::random_device rd;
#endif
    const int bufSize = 500000;
    std::shared_ptr<char> buf(new char[bufSize]);
    while(kcp_interface->GetInterfaceIsValid())
    {
        bool bEndOfFrame;
        int frameSize = h264File.readFrame((char*)buf.get(), bufSize, &bEndOfFrame);
#if 0
        tmp[0]=33+rd()%93;
        tmp[1]=33+rd()%93;
#endif
        if(frameSize>0)
        {
            kcp_interface->Send_Userdata(buf,frameSize);
            std::cout<<"send_len :"<<frameSize<<std::endl;
        }
        else
        {
            break;
        }
        xop::Timer::sleep(40);
    }
}
void kcp_test::tcp_send_thread(std::shared_ptr<xop::Channel> channel)
{
    std::cout<<"tcp_send_thread"<<std::endl;
    int fd=channel->fd();
    char buf[10]={0};
    memset(buf,'#',10);
    std::random_device rd;
    while(1)
    {
        buf[0]=33+rd()%93;
        buf[1]=33+rd()%93;
        if(send(fd,buf,10,0)<0)break;
        std::cout<<"send "<<buf<<std::endl;
        xop::Timer::sleep(40);
    }
    std::cout<<"tcp send exit"<<buf<<std::endl;
}
void kcp_test::tcp_send_callback(std::shared_ptr<xop::Channel> channel,int session_id,int channel_id,std::string src_name)
{
    std::thread t(&kcp_test::tcp_send_thread,channel);
    t.detach();
}

void kcp_test::tcp_listen_init(std::shared_ptr<xop::EventLoop> loop)
{//创建TCP直连处理模块，端口需做UPNP映射或者手动端口映射
    xop::TcpSocket sock;
    sock.create();
    xop::SocketUtil::setReuseAddr(sock.fd());
    xop::SocketUtil::setReusePort(sock.fd());
    xop::SocketUtil::setNonBlock(sock.fd());
    sock.bind("0.0.0.0",TCP_CONNECT_PORT);
    sock.listen(10);
    std::shared_ptr<xop::Channel> channel(new xop::Channel(sock.fd()));
    channel->setReadCallback([sock,loop](){
        int fd=sock.accept();
        std::cout<<"accept : "<<fd<<std::endl;
        if(fd>0)
        {//此处获得了一个指向目的服务器的连接
            std::shared_ptr<xop::Channel> new_channel(new xop::Channel(fd));
            new_channel->setReadCallback([new_channel,loop](){
                char buf[512]={0};
                int ret=recv(new_channel->fd(),buf,512,0);
                if(ret<=0)
                {//接收失败判断连接关闭，移除连接
                    loop->removeChannel((std::shared_ptr<xop::Channel>&)new_channel);
                }
                else
                {//第一个包会携带连接信息，若对端连续发送，可能会有粘包现象
                 //可在收到连接信息后重新调用setReadCallback修改收到数据包之后的操作
                 //若用其它模块接收，则可调用removeChannel移除通道，同时需保存Channel信息,默认为rtsp
                    std::cout<<"recv : "<<buf<<std::endl;
                }
            });
            new_channel->enableReading();
            loop->updateChannel(new_channel);
        }
    });
    channel->enableReading();
    loop->updateChannel(channel);
}
int kcp_test::get_active_port(std::shared_ptr<xop::EventLoop> loop,int mode)
{
    if(mode!=0)return TCP_CONNECT_PORT;
    int fd=socket(AF_INET, SOCK_DGRAM, 0);
    //创建udp套接字并绑定随机端口
    if(fd>0&&xop::SocketUtil::random_bind(fd,100))
    {
        xop::SocketUtil::setNonBlock(fd);
        xop::SocketUtil::setSendBufSize(fd, 32*1024);
        std::shared_ptr<xop::Channel> channel(new xop::Channel(fd));
        //添加连接超时事件,超时时移除套接字
        int timer_id=loop->addTimer([loop,channel](){loop->removeChannel((std::shared_ptr<xop::Channel>&)channel);\
                                                     std::cout<<"remove udp channel!"<<std::endl;
                                                     return false;},5000);
        channel->setReadCallback([fd,timer_id,loop,channel](){
            char buf[512]={0};
            struct sockaddr_in addr = {0};
            socklen_t len=sizeof addr;
            int ret=::recvfrom(fd,buf,512,0,(struct sockaddr*)&addr,&len);
            if(ret>0)
            {//
                std::cout<<buf<<std::endl;
                std::map<std::string,std::string> recv_map=message_handle::parse_buf(buf);
                auto mode=recv_map.find("mode");
                auto session_id=recv_map.find("session_id");
                auto channel_id=recv_map.find("channel_id");
                auto src_name=recv_map.find("src_name");
                auto ip=recv_map.find("ip");
                auto port=recv_map.find("port");
                if(mode==std::end(recv_map)||session_id==std::end(recv_map)||channel_id==std::end(recv_map)||src_name==std::end(recv_map)||\
                   ip==std::end(recv_map)||port==std::end(recv_map))return;
                ::connect(fd,(sockaddr *)&addr,len);
                //将套接字变为不活跃状态
                loop->removeChannel((std::shared_ptr<xop::Channel>&)channel);
                loop->removeTimer(timer_id);
                //创建kcp连接
                kcp_test::kcp_callback(channel,std::stoi(session_id->second),std::stoi(channel_id->second),src_name->second);
            }
        });
        channel->enableReading();
        loop->updateChannel(channel);
        return xop::SocketUtil::getLocalPort(fd);
    }
    else
    {
        return 0;
    }
}

