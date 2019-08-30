#include "kcp_test.h"
#include <thread>
#include <random>
void kcp_test::kcp_callback(std::shared_ptr<xop::Channel> channel,int session_id)
{
    sensor_net::KCP_Manager::GetInstance().AddConnection(channel,session_id,&kcp_test::kcp_recvcallback,nullptr);
}
void kcp_test::kcp_send_test(std::shared_ptr<xop::Channel> channel,int session_id)
{
    std::shared_ptr<sensor_net::KCP_Interface> interface=sensor_net::KCP_Manager::GetInstance().AddConnection(channel,session_id,&kcp_test::kcp_recvcallback,nullptr);
    std::thread t(&kcp_test::send_thread,interface);
    t.detach();
}

void kcp_test::kcp_recvcallback(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,void *user)
{
    std::cout<<"conv_id : "<<kcp->conv<<" recv :"<<buf<<std::endl;
}
void kcp_test::send_thread(std::shared_ptr<sensor_net::KCP_Interface> kcp_interface)
{
    std::cout<<"send_thread"<<std::endl;
    std::shared_ptr<char> buf(new char[2]);
    char * tmp=buf.get();
    std::random_device rd;
    while(1)
    {
        tmp[0]=33+rd()%93;
        tmp[1]=33+rd()%93;
        kcp_interface->Send_Userdata(buf,2);
        std::cout<<"send :"<<tmp<<std::endl;
        xop::Timer::sleep(30);
    }
}
