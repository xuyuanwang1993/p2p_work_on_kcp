#include "kcp_test.h"
#include <thread>
#include <random>
#include <memory>
void kcp_test::kcp_callback(std::shared_ptr<xop::Channel> channel,int session_id,int channel_id,std::string src_name)
{
    std::shared_ptr<sensor_net::data_ptr> data(new sensor_net::data_ptr(channel_id,src_name));
    sensor_net::KCP_Manager::GetInstance().AddConnection(channel,session_id,&kcp_test::kcp_recvcallback,data);
}
void kcp_test::kcp_send_test(std::shared_ptr<xop::Channel> channel,int session_id,int channel_id,std::string src_name)
{
    std::shared_ptr<sensor_net::data_ptr> data(new sensor_net::data_ptr(channel_id,src_name));
    std::shared_ptr<sensor_net::KCP_Interface> interface=sensor_net::KCP_Manager::GetInstance().AddConnection(channel,session_id,&kcp_test::kcp_recvcallback,data);
    std::thread t(&kcp_test::send_thread,interface);
    t.detach();
}

void kcp_test::kcp_recvcallback(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,std::shared_ptr<sensor_net::data_ptr>data)
{
    std::cout<<"conv_id : "<<kcp->conv<<" recv :"<<buf<<"    info :"<<data->channel_id<<"   "<<data->src_name<<std::endl;
}
void kcp_test::send_thread(std::shared_ptr<sensor_net::KCP_Interface> kcp_interface)
{
    std::cout<<"send_thread"<<std::endl;
    std::shared_ptr<char> buf(new char[10]);
    char * tmp=buf.get();
    memset(tmp,'#',10);
    std::random_device rd;
    while(kcp_interface->GetInterfaceIsValid())
    {
        tmp[0]=33+rd()%93;
        tmp[1]=33+rd()%93;
        kcp_interface->Send_Userdata(buf,10);
        std::cout<<"send :"<<tmp<<std::endl;
        xop::Timer::sleep(30);
    }
}
