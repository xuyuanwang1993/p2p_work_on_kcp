#include "p2p_punch_client.h"
#include <stdlib.h>
#include "kcp_manage.h"
#include "kcp_test.h"
int main(int argc,char*argv[])
{
    if(argc<4)
    {
        std::cout<<"check command!"<<std::endl;
        exit(-1);
    }
    std::shared_ptr<xop::EventLoop>event_loop(new xop::EventLoop(1));
    sensor_net::KCP_Manager::GetInstance().Config(event_loop.get());
    sensor_net::KCP_Manager::GetInstance().StartUpdateLoop();
    //std::cout<<argv[1]<<":"<<argv[2]<<std::endl;
    p2p_punch_client client(argv[1],argv[2],event_loop);
    std::thread t(std::bind(&xop::EventLoop::loop,event_loop));
    t.detach();
    client.start();
    std::string mode=argv[3];
    if(mode=="send")
    {
        client.setEventCallback(kcp_test::kcp_send_test);
//        client.setEventCallback([](std::shared_ptr<xop::Channel> channel,int session_id){
//            std::cout<<"session_id : "<<session_id<<std::endl;
//        });
    }
    else
    {
        client.setEventCallback(kcp_test::kcp_callback);
    }
    std::string command;
    while(1)
    {
        std::cin>>command;
        std::cout<<std::endl<<"device_id :"<<command<<std::endl;
        client.try_establish_connection(command);
    }
    return 0;
}
