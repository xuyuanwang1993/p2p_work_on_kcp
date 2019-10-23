#ifndef P2P_PUNCH_CLIENT_H
#define P2P_PUNCH_CLIENT_H
#include <string>
#include "EventLoop.h"
#include <atomic>
#include <map>
#include <Logger.h>
#include"upnpmapper.h"
class p2p_punch_client{
public:
    typedef enum{
        STREAM_SERVER,
        STREAM_CLIENT
    }STREAM_SERVER_MODEL;
    typedef std::function<void(std::shared_ptr<xop::Channel>,int,int,std::string)> P2PEventCallback;
    typedef std::function<void(std::string,int,std::string,int)>ResetStreamServerCallback;//ip,port external_ip:external_port
    typedef  std::function<void()>RestartStreamServerCallback;
private:
    const unsigned short UDP_RECV_PORT=23333;
    const int SESSION_CACHE_TIME=10000;//10s
    const int CHECK_INTERVAL=5000;//5s
    const int ALIVE_TIME_INTERVAL=30000;//30s
    typedef struct _m_p2p_session{
        int session_id;
        int channel_id;
        std::string src_name;
        std::shared_ptr<xop::Channel> channelPtr;//连接时创建的套接字
        P2PEventCallback callback;//连接成功回调函数
        int64_t alive_time;
    }m_p2p_session;
    typedef struct _stream_server_info{
        std::string stream_server_id="-1";//流媒体服务器id
        STREAM_SERVER_MODEL mode=STREAM_CLIENT;//工作模式
        std::string account_name="";//账户名
        std::string internal_ip="";//服务器内网ip
        std::string internal_port="0";//内部端口
        std::string external_ip="";//外网ip
        std::string external_port="0";//外部端口
        std::string load_size="0";//负载大小
        std::atomic<bool> check_task;//检测任务标识
    }stream_server_info;
public:
    p2p_punch_client(std::string server_ip,std::string device_id,std::shared_ptr<xop::EventLoop> event_loop);
    ~p2p_punch_client();
    void setEventCallback(const P2PEventCallback& cb)
    { m_connectCB = cb;};
    void setTcpEventCallback(const P2PEventCallback& cb)
    {m_TcpconnectCB=cb;};
    //推流客户端初始化
    void stream_client_init(std::string account_name,int load_size=4,ResetStreamServerCallback cb=nullptr);
    //推流服务器初始化
    void stream_server_init(std::string path,RestartStreamServerCallback cb=nullptr);
    //开启推流任务
    void start_stream_check_task();
    void start();
    bool check_is_p2p_able(){return m_p2p_flag;};
    bool try_establish_connection(std::string remote_device_id,int channel_id,std::string src_name);
    bool try_establish_active_connection(std::string remote_device_id,int port,int channel_id,std::string src_name,int mode=0);
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
    void send_set_up_connection_packet(std::string remote_device_id,int channel_id,std::string src_name);
    void send_active_connection(std::string remote_device_id,int channel_id,std::string src_name,int port,int mode=0);
    void send_get_stream_server_info();
    void send_get_external_ip();
    void send_stream_server_register();
    void send_stream_server_keepalive();
    void handle_punch_hole_response(std::map<std::string,std::string> &recv_map);//打洞成功回复
    void handle_punch_hole(std::map<std::string,std::string> &recv_map);//打洞
    void handle_set_up_connection(std::map<std::string,std::string> &recv_map);//请求连接
    void handle_keep_alive(std::map<std::string,std::string> &recv_map);//心跳保活
    void handle_nat_type_probe(std::map<std::string,std::string> &recv_map);//外网端口探测
    void handle_active_connection(std::map<std::string,std::string> &recv_map);//处理直连
    void handle_get_stream_server_info(std::map<std::string,std::string> &recv_map);//处理推流服务器信息
    void handle_reset_stream_state();//重置推流状态
    void handle_restart_device();//设备重启
    void handle_get_external_ip(std::map<std::string,std::string> &recv_map);//获取外网ip
    void handle_stream_server_register(std::map<std::string,std::string> &recv_map);//服务器注册
    void handle_restart_stream_server();//服务器重启
    void handle_stream_server_keepalive();
    void udp_active_connect_task(std::string session_id,std::string channel_id,std::string src_name,std::string ip,std::string port,std::string mode);
    void tcp_active_connect_task(std::string session_id,std::string channel_id,std::string src_name,std::string ip,std::string port,std::string mode);
    int64_t GetTimeNow()
    {//ms
        auto timePoint = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    }
    static std::unordered_map<std::string,int> m_command_map;
    p2p_punch_client();
    int m_client_sock;
    std::shared_ptr<stream_server_info> m_stream_server_info;
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
    P2PEventCallback m_TcpconnectCB;
    ResetStreamServerCallback m_resetstreamserverCB;
    RestartStreamServerCallback m_restartstreamserverCB;
};

#endif // P2P_PUNCH_CLIENT_H
