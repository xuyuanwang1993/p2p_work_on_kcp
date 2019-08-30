#ifndef KCP_TEST_H
#define KCP_TEST_H
#include "kcp_manage.h"
class kcp_test{
public:
    static void kcp_callback(std::shared_ptr<xop::Channel> channel,int session_id);
    static void kcp_recvcallback(std::shared_ptr<char> buf,int len,struct IKCPCB *kcp,void *user);
    static void kcp_send_test(std::shared_ptr<xop::Channel> channel,int session_id);
    static void send_thread(std::shared_ptr<sensor_net::KCP_Interface> kcp_interface);
};

#endif // KCP_TEST_H
