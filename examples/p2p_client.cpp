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
    //初始化事件循环
    std::shared_ptr<xop::EventLoop>event_loop(new xop::EventLoop(1));
    sensor_net::KCP_Manager::GetInstance().Config(event_loop.get());
    //添加KCP 状态处理任务
    sensor_net::KCP_Manager::GetInstance().StartUpdateLoop();
    //std::cout<<argv[1]<<":"<<argv[2]<<std::endl;
    p2p_punch_client client(argv[1],argv[2],event_loop);
    std::thread t(std::bind(&xop::EventLoop::loop,event_loop));
    t.detach();
    //激活客户端
    client.start();
    std::string mode=argv[3];
    if(mode=="send")
    {
        client.setEventCallback(kcp_test::kcp_send_test);
        client.setTcpEventCallback(kcp_test::tcp_send_callback);
//        client.setEventCallback([](std::shared_ptr<xop::Channel> channel,int session_id){
//            std::cout<<"session_id : "<<session_id<<std::endl;
//        });
    }
    else
    {
        kcp_test::tcp_listen_init(event_loop);
        client.setEventCallback(kcp_test::kcp_callback);
    }
    std::string command;
    std::string device_id;
    std::string conv_id;
    std::string src_name;
    int channel_id=0;
    while(1)
    {   std::cout<<"input command:";
        std::cin>>command;
        std::cout<<command<<std::endl;
        if(command=="close")
        {
            std::cout<<"input conv_id:";
            std::cin>>conv_id;
            std::cout<<conv_id<<std::endl;
            int id=std::stoi(conv_id);
            std::cout<<"id "<<id<<std::endl;
           sensor_net::KCP_Manager &test= sensor_net::KCP_Manager::GetInstance();
           test.CloseConnection(id);
        }
        else if(command=="start")
        {
            std::cout<<"input device_id:";
            std::cin>>device_id;
            std::cout<<device_id<<std::endl;
            std::cout<<"input src_name:";
            std::cin>>src_name;
            std::cout<<src_name<<std::endl;
            //普通kcp连接
            client.try_establish_connection(device_id,channel_id++,src_name);
            //client.try_establish_active_connection(command,kcp_test::get_active_port(event_loop,1),channel_id++,src_name+std::to_string(channel_id),1);
            //kcp反向连接
            //client.try_establish_active_connection(command,kcp_test::get_active_port(event_loop,0),channel_id++,src_name+std::to_string(channel_id),0);
        }
        else
        {
            std::cout<<"check input"<<std::endl;
        }
    }
    return 0;
}
