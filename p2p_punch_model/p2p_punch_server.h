#ifndef P2P_PUNCH_SERVER_H
#define P2P_PUNCH_SERVER_H
#include "EventLoop.h"
#include <atomic>
#include "parase_request.h"
#include <unordered_map>
#include <map>
#include "Logger.h"
class p2p_punch_server{
    const unsigned short UDP_RECV_PORT=23333;
    const int OFFLINE_TIME=30000;//30s
    const int CHECK_INTERVAL=5000;//5s
    const int SESSION_CACHE_TIME=10000;//10s
public:
    typedef struct _device_nat_info{
        std::string device_id;
        std::string ip;
        std::string port;
        int64_t alive_time;
    }device_nat_info;
    typedef struct _punch_session{
        std::string device_id;
        std::string ip;
        std::string port;
        int64_t alive_time;
        bool recv_punch_packet;
        bool recv_punch_response;
    }punch_session;

    typedef struct _session_info{
        int session_id;//会话id
        punch_session session[2];
    }session_info;

    static p2p_punch_server &Get_Instance(){static p2p_punch_server server;return server;};
    void start_server(){m_event_loop->loop();};
protected:
    ~p2p_punch_server(){m_event_loop->quit();std::cout<<"quit"<<std::endl;};
private:
    void handle_read();
    void handle_read_helper();
    void handle_punch_hole_response(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//打洞成功回复
    void handle_punch_hole(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//打洞
    void handle_set_up_connection(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//请求连接
    void handle_keep_alive(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//心跳保活
    void handle_nat_type_probe(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//外网端口探测
    void handle_not_supported_command(int send_fd,std::string cmd,struct sockaddr_in &addr);
    std::string packet_error_response(std::string cmd,std::string error_info);
    int64_t GetTimeNow()
    {//ms
        auto timePoint = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    }
    int sock_init();
    void remove_invalid_resources();
    p2p_punch_server();
    int m_session_id;
    int m_server_sock[2];//
    std::map<int ,session_info> m_session_map;
    std::unordered_map<std::string ,device_nat_info> m_device_map;
    std::shared_ptr<xop::Channel> m_server_channel;
    std::shared_ptr<xop::Channel> m_help_channel;
    std::shared_ptr<xop::EventLoop> m_event_loop;
};

#endif // P2P_PUNCH_SERVER_H
