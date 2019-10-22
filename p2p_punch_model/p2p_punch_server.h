#ifndef P2P_PUNCH_SERVER_H
#define P2P_PUNCH_SERVER_H
#include "EventLoop.h"
#include <atomic>
#include "parase_request.h"
#include <unordered_map>
#include <map>
#include "Logger.h"
#include <set>
#include "cJSON.h"
#define DEFAULT_SERVER_ACCOUNT "admin"
class p2p_punch_server{
    const unsigned short UDP_RECV_PORT=23333;
    const int OFFLINE_TIME=65000;//65s
    const int CHECK_INTERVAL=5000;//5s
    const int SESSION_CACHE_TIME=10000;//10s
public:
    typedef struct _device_nat_info{
        std::string device_id;
        std::string ip;//extranet_ip外网ip
        std::string port;//extranet_port外网端口
        std::string local_ip;//本地ip
        int64_t alive_time;
        std::string stream_server_id;//流媒体服务器ID
        _device_nat_info(){stream_server_id="-1";}
    }device_nat_info;
    typedef struct _punch_session{
        std::string device_id;
        std::string ip;
        std::string port;
        std::string local_ip;//本地ip
        std::string local_port;//本地端口
        int64_t alive_time;
        bool recv_punch_packet;
        bool recv_punch_response;
    }punch_session;

    typedef struct _session_info{
        int session_id;//会话id
        punch_session session[2];
    }session_info;
    typedef struct  _stream_server_info{
        int id;//流媒体服务器序号
        int64_t last_alive_time;//上次在线时间
        std::string external_ip;//外网IP
        int external_port;//外网UDP通讯端口
        int stream_external_port;//服务器外网端口
        std::string internal_ip;//内网IP
        int stream_internal_port;//服务器内网端口
        int available_load_size;//可用负载大小   50M/512K~=100
        std::unordered_map<std::string,int>device_load_size_map;
        _stream_server_info()
        {
            id=-1;
            last_alive_time=0;
            external_ip="255.255.255.255";
            external_port=0;
            stream_external_port=0;
            internal_ip="255.255.255.255";
            stream_internal_port=0;
            available_load_size=0;
        }
    }stream_server_info;
    static p2p_punch_server &Get_Instance(){static p2p_punch_server server;return server;};
    void open_debug(){m_open_debug=true;}
    void init_stream_server_task(std::string path);
    void start_server(){m_event_loop->loop();};
protected:
    ~p2p_punch_server(){m_event_loop->quit();std::cout<<"quit"<<std::endl;};
private:
    void handle_read();
    void handle_read_helper();
    void handle_punch_hole_response(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//打洞成功回复
    void handle_punch_hole(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//打洞
    void handle_set_up_connection(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//请求连接
    void handle_active_connection(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//处理直连请求
    void handle_keep_alive(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//心跳保活
    void handle_nat_type_probe(int send_fd,struct sockaddr_in &addr,std::map<std::string,std::string> &recv_map);//外网端口探测
    void handle_get_external_ip(int send_fd,struct sockaddr_in &addr);//外网ip查询
    void handle_get_stream_server_info(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map);//可用流媒体服务器信息查询
    void handle_stream_server_register(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map);//流媒体服务器注册
     void handle_stream_server_keepalive(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map);//流媒体服务器心跳包
    void handle_reset_stream_state(){}
    void send_reset_stream_state(int send_fd,struct sockaddr_in&addr);//重置推流
    void handle_get_online_device_info(int send_fd,struct sockaddr_in&addr);//查询在线设备信息（调试指令)
    void handle_get_stream_server_state(int send_fd,struct sockaddr_in&addr);//流媒体服务器状态查询（调试模式下有效)
    void handle_restart_stream_server(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map);//指定流媒体服务器重启（调试模式下有效,不清除连接信息）
    void handle_restart_device(int send_fd,struct sockaddr_in&addr,std::map<std::string,std::string> &recv_map);//重启指定推流端（调试模式下有效）
    void handle_not_supported_command(int send_fd,std::string cmd,struct sockaddr_in &addr);
    std::string packet_error_response(std::string cmd,std::string error_info);
    int64_t GetTimeNow()
    {//ms
        auto timePoint = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    }
    int sock_init();
    void release_stream_server(std::string device_id,int stream_server_id);
    void remove_invalid_resources();
    CJSON *get_stream_server_state();
    CJSON *get_online_device_info();
    p2p_punch_server();
    std::atomic<bool> m_stream_server_task_init;
    bool m_open_debug=false;
    int m_session_id;
    int m_stream_server_id;
    int m_server_sock[2];//
     static std::unordered_map<std::string,int> m_command_map;
     std::set<std::string> m_authorized_accounts;
    std::map<int ,session_info> m_session_map;
    std::unordered_map<std::string ,device_nat_info> m_device_map;
    std::unordered_map<int,std::string>m_id_map;//server_id:account
    std::unordered_map<std::string,std::map<int,stream_server_info>>m_stream_server_map;//account :  (id:stream_server_info)
    std::shared_ptr<xop::Channel> m_server_channel;
    std::shared_ptr<xop::Channel> m_help_channel;
    std::shared_ptr<xop::EventLoop> m_event_loop;
};

#endif // P2P_PUNCH_SERVER_H
