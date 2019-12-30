#ifndef DATA_RELAY_SERVER_H
#define DATA_RELAY_SERVER_H
#include "Socket.h"
#include "EventLoop.h"
#include "Channel.h"
#include <unordered_map>
#include <atomic>
#include"Acceptor.h"
#include <unordered_set>
namespace micagent {
using namespace xop;
using std::string;
using std::shared_ptr;
class Data_Relay_Server;
typedef  enum{
    BY_TCP,
    BY_UDP,
}TRANSFER_TYPE;
typedef struct _Data_Session{
    uint32_t session_id;//服务器生成的会话id
    uint32_t call_id;//调用方创建会话时传入的会话id
    int connection_state;//<0  closed  0:wait setup 1:peer0 get in 2:peer2 get in  3:connected
    TRANSFER_TYPE type;//传输方式
    struct sockaddr_in addr_peer1;//会话发起方UDP地址
    struct sockaddr_in addr_peer2;//对端UDP地址
    ChannelPtr channel_peer1;//会话发起方TCP连接
    ChannelPtr channel_peer2;//对端TCP连接
    int64_t last_alive_time;
    _Data_Session(uint32_t _session_id,uint32_t _call_id,int64_t _time_now){
        session_id=_session_id;
        call_id=_call_id;
        connection_state=0;
        last_alive_time=_time_now;
    }
}Data_session;
typedef enum{
    DATA,//纯数据
    START_TRANSFER,//开始传输
    CLOSE_TRANSFER,//关闭传输通道
}Data_Function_Code;
typedef  struct _Udp_Data_Header{
    uint32_t session_id;//服务器生成的会话id
    uint32_t peer_mask:1;//通信标识
    uint32_t function_code:15;//功能码
    uint32_t check_sum:16;//校验位
}Udp_Data_Header;
class Data_Relay_Server{
    static const uint32_t MAX_SESSION_CACHE_TIME=5000;//5s
    static const uint32_t CHECK_INTERVAL=2000;//2s
public:
    explicit Data_Relay_Server(shared_ptr<EventLoop>loop,uint16_t port);
    ~Data_Relay_Server();
    /**
     * @brief AddTunnelSession 添加转发会话
     * @param session_id 会话id
     * @param call_id 调用方会话id
     * @return 会话存在的话会返回false
     */
    bool AddTunnelSession(uint32_t session_id,uint32_t call_id);
    /**
     * @brief RemoveTunnelSession 移除连接会话
     * @param session_id 会话id
     */
    void RemoveTunnelSession(uint32_t session_id);
    /**
     * @brief GetErrorInfo 获取最近一次错误信息
     * @return 最近一次错误信息
     */
    const std::string GetErrorInfo();
    /**
     * @brief PackDataHeader 打包数据头TCP第一个校验包为此结构体，UDP所有packet头部为此结构体
     * @param session_id 会话id
     * @param is_peer1 是否为peer1
     * @param func_code 包头功能码
     * @return 返回打包好的数据头
     */
    static Udp_Data_Header PackDataHeader(uint32_t session_id,bool is_peer1,Data_Function_Code func_code);
    /**
     * @brief UnpackDataHeader 解析数据头
     * @param buf 数据源
     * @param len 数据部分长度
     * @param save 存储解析好的header
     * @return 校验正常 返回true
     */
    static bool UnpackDataHeader(void *buf,size_t len,Udp_Data_Header &save);
    /**
     * @brief Set_Close_State 设置关闭状态 可禁用数据处理
     * @param is_closed
     */
    void Set_Close_State(bool is_closed=true);
private:
    /**
     * @brief m_is_exited 构造时置false 析构之前置true
     */
    std::atomic<bool>m_is_exited;
    /**
     * @brief m_mutex 资源同步锁
     */
    std::mutex m_mutex;
    /**
     * @brief m_error_mutex 错误信息读写锁
     */
    std::mutex m_error_mutex;
    /**
     * @brief m_tcp_fd_map fd->session_id
     */
    std::unordered_map<int,uint32_t>m_tcp_fd_map;
    /**
     * @brief m_session_map session_id->Data_session
     */
    std::unordered_map<uint32_t,std::shared_ptr<Data_session>>m_session_map;
    /**
     * @brief m_event_loop 事件循环 外部传入
     */
    std::shared_ptr<EventLoop> m_event_loop;
    /**
     * @brief m_check_timer 析构时需移除
     */
    TimerId m_check_timer;
    /**
     * @brief m_udp_channel udp数据处理fd
     */
    ChannelPtr m_udp_channel;
    /**
     * @brief m_tcp_acceptor tcp连接处理
     */
    std::shared_ptr<xop::Acceptor> m_tcp_acceptor;
    /**
     * @brief m_init_ok 判断服务器初始化
     */
    std::atomic<bool> m_init_ok;
    /**
     * @brief m_error_string 保存最近一次错误信息
     */
    std::string m_error_string;
    /**
     * @brief m_tcp_tmp_set 保存tcp临时连接
     */
    std::unordered_set<ChannelPtr> m_tcp_tmp_set;
private:
    /**
     * @brief udp_handle_read udp数据处理
     */
    void udp_handle_read();
    /**
     * @brief udp_handle_data udp 转发数据处理
     */
    void udp_handle_data(Udp_Data_Header &header,const void *buf,ssize_t buf_len);
    /**
     * @brief udp_handle_close 关闭udp连接
     * @param header 数据头部
     */
    void udp_handle_close(Udp_Data_Header &header);
    /**
     * @brief tcp_handle_read tcp数据处理
     * @param fd tcp数据接收通道
     */
    void tcp_handle_read(int32_t fd);
    /**
     * @brief do_remove_tunnel_session 移除连接会话
     * @param session_id 会话id
     */
    void do_remove_tunnel_session(uint32_t session_id);
    /**
     * @brief remove_invalid_resources 移除失效的session
     */
    void remove_invalid_resources();
    /**
     * @brief GetTimeNow 获取到1970年的ms数
     * @return 当前时间 单位ms
     */
    static int64_t GetTimeNow()
    {
        auto timePoint = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    }
    /**
     * @brief udp_handle_new_session udp新sesion接入处理
     * @param header 校验过的数据头
     * @param sockaddr 源地址
     */
    void udp_handle_new_session(Udp_Data_Header &header,struct sockaddr_in &sockaddr);
    /**
     * @brief tcp_handle_new_session tcp新sesion接入处理
     * @param header 校验过的数据头
     * @param channel 发包socket
     */
    void tcp_handle_new_session(Udp_Data_Header &header,ChannelPtr channel);
    /**
     * @brief tcp_handle_close_session 关闭tcp数据转发链路
     * @param session 连接会话
     */
    void tcp_handle_close_session(std::shared_ptr<Data_session> session);
};
}
#endif // DATA_RELAY_SERVER_H
