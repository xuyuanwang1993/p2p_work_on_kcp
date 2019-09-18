#ifndef KCP_TEST_H
#define KCP_TEST_H
#include "kcp_manage.h"
#define TCP_CONNECT_PORT 25000
class kcp_test{
    typedef struct _data_ptr{
        int channel_id;
        std::string src_name;
        _data_ptr(int _channel_id,std::string _src_name)
        {
            channel_id=_channel_id;
            src_name=_src_name;
        }
    }data_ptr;
public:
    static void kcp_callback(std::shared_ptr<xop::Channel> channel,int session_id,int channel_id,std::string src_name);
    static void kcp_recvcallback(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,std::shared_ptr<sensor_net::data_ptr>data);
    static void kcp_recvcallback2(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,std::shared_ptr<sensor_net::data_ptr>data);
    static void kcp_send_test(std::shared_ptr<xop::Channel> channel,int session_id,int channel_id,std::string src_name);
    static void send_thread(std::shared_ptr<sensor_net::KCP_Interface> kcp_interface);
    static void tcp_send_callback(std::shared_ptr<xop::Channel> channel,int session_id,int channel_id,std::string src_name);
    static void tcp_send_thread(std::shared_ptr<xop::Channel> channel);
    static void tcp_listen_init(std::shared_ptr<xop::EventLoop> loop);
    static int get_active_port(std::shared_ptr<xop::EventLoop> loop,int mode=0);
};

#endif // KCP_TEST_H
