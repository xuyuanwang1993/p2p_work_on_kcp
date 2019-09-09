#include "kcp_test.h"
#include <thread>
#include <random>
#include <memory>
#include "h264_file_handle.h"
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
    std::cout<<"conv_id : "<<kcp->conv<<" recv_len :"<<len<<"    info :"<<data->channel_id<<"   "<<data->src_name<<std::endl;
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
