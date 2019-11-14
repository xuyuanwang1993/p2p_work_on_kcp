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
    static int64_t GetTimeNow()
        {
            auto timePoint = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
        }
    static void kcp_callback(std::shared_ptr<xop::Channel> channel,unsigned int session_id,int channel_id,std::string src_name);
    //下面两个函数为从KCP连接中收到包后的回调操作
    static void kcp_recvcallback(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,std::shared_ptr<sensor_net::data_ptr>data);
    static void kcp_recvcallback2(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,std::shared_ptr<sensor_net::data_ptr>data);
    static void kcp_send_test(std::shared_ptr<xop::Channel> channel,unsigned int session_id,int channel_id,std::string src_name);
    static void send_thread(std::shared_ptr<sensor_net::KCP_Interface> kcp_interface);
    static void tcp_send_callback(std::shared_ptr<xop::Channel> channel,unsigned int session_id,int channel_id,std::string src_name);
    static void tcp_send_thread(std::shared_ptr<xop::Channel> channel);
    static void tcp_listen_init(std::shared_ptr<xop::EventLoop> loop);
    static int get_active_port(std::shared_ptr<xop::EventLoop> loop,int mode=0);

    //下载测试回调函数
    static void downland_eventcallback(std::shared_ptr<xop::Channel> channel,unsigned int session_id,int channel_id,std::string src_name);
    static int downland_get_active_port(std::shared_ptr<xop::EventLoop> loop);
    static void downland_kcp_recvcallback(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,std::shared_ptr<sensor_net::data_ptr>data);
    static bool downland_stream_headr_check(std::shared_ptr<char> buf);
    static unsigned char get_mask(const char *buf);
};

#endif // KCP_TEST_H
