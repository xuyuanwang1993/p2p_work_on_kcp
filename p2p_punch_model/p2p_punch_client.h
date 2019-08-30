#ifndef P2P_PUNCH_CLIENT_H
#define P2P_PUNCH_CLIENT_H
#include <string>
#include "EventLoop.h"
#include <atomic>
#include <map>
#include <Logger.h>
class p2p_punch_client{
public:
    typedef std::function<void(std::shared_ptr<xop::Channel>,int)> P2PEventCallback;
private:
    const unsigned short UDP_RECV_PORT=23333;
    const int SESSION_CACHE_TIME=10000;//10s
    const int CHECK_INTERVAL=5000;//5s
    const int ALIVE_TIME_INTERVAL=30000;//30s
    typedef struct _m_p2p_session{
        int session_id;
        std::shared_ptr<xop::Channel> channelPtr;//连接时创建的套接字
        P2PEventCallback callback;//连接成功回调函数
        int64_t alive_time;
    }m_p2p_session;
public:
    p2p_punch_client(std::string server_ip,std::string device_id,std::shared_ptr<xop::EventLoop> event_loop);
    ~p2p_punch_client();
    void setEventCallback(const P2PEventCallback& cb)
    { m_connectCB = cb; };
    void start(){if(!m_p2p_flag){send_nat_type_probe_packet();};};
    bool check_is_p2p_able(){return m_p2p_flag;};
    bool try_establish_connection(std::string remote_device_id);
private:
    int get_udp_session_sock();
    void remove_invalid_resources();
    void handle_read();
    void session_handle_read(int fd);

    void send_alive_packet();
    void send_nat_type_probe_packet();
    void send_punch_hole_packet(m_p2p_session &session);
    void send_low_ttl_packet(m_p2p_session &session,std::string ip,std::string port);//进行打洞操作，并connect
    void send_punch_hole_response_packet(m_p2p_session &session);
    void send_set_up_connection_packet(std::string remote_device_id);

    void handle_punch_hole_response(std::map<std::string,std::string> &recv_map);//打洞成功回复
    void handle_punch_hole(std::map<std::string,std::string> &recv_map);//打洞
    void handle_set_up_connection(std::map<std::string,std::string> &recv_map);//请求连接
    void handle_keep_alive(std::map<std::string,std::string> &recv_map);//心跳保活
    void handle_nat_type_probe(std::map<std::string,std::string> &recv_map);//外网端口探测
    int64_t GetTimeNow()
    {//ms
        auto timePoint = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    }
    p2p_punch_client();
    int m_client_sock;
    std::shared_ptr<xop::Channel> m_client_channel;
    std::string m_server_ip;
    std::string m_device_id;
    std::string m_wan_ip;
    std::string m_wan_port;
    xop::TimerId m_cache_timer_id;//
    xop::TimerId m_alive_timer_id;//
    std::shared_ptr<xop::EventLoop> m_event_loop;
    std::atomic<bool> m_p2p_flag;//判断是否支持穿透
    std::map<int,m_p2p_session> m_session_map;
    P2PEventCallback m_connectCB;
};

#endif // P2P_PUNCH_CLIENT_H
