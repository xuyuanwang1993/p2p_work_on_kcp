#ifndef UDP_DATA_TUNNEL_H
#define UDP_DATA_TUNNEL_H
#include <EventLoop.h>
#include <Channel.h>
#include <string>
#include<map>
#include<mutex>
#include<atomic>
#include <functional>
#include<unordered_map>
#include<ikcp.h>
namespace xop {
class udp_data_tunnel
{
public:
    typedef enum{
        PROTOCAL_UDP,
        PROTOCAL_KCP,
    }PROTOCAL_TYPE;
private:
    typedef enum{
        WAIT_CONNECT,
        HALF_CONNECT,
        ESTABLISHED,
    }CONNECT_STATE;
    using TUNNEL_DATA_HANDLER=std::function<void(struct sockaddr_in &addr, const char *buf,unsigned int size)>;
    static const int MAX_CACHE_TIME=10000;//10s
    static const int CHECK_INTERVAL=2000;//2s
    typedef struct _tunnel_session{
        unsigned int session_id;
        CONNECT_STATE  state;
        struct sockaddr_in addr1;
        struct sockaddr_in addr2;
        int64_t last_alive_time;
        std::string source_name;
        PROTOCAL_TYPE protocal_type;
        _tunnel_session()
        {
            state=WAIT_CONNECT;
            bzero(&addr1,sizeof addr1);
            addr1.sin_family=AF_INET;
            bzero(&addr2,sizeof addr2);
            addr2.sin_family=AF_INET;
            protocal_type=PROTOCAL_UDP;
        }
    }tunnel_session;
public:
    udp_data_tunnel(EventLoop *loop,int port);
    udp_data_tunnel()=delete;
    ~udp_data_tunnel();
    void run();
    bool add_tunnel_session(unsigned int session_id,std::string source_name,PROTOCAL_TYPE protocal_type=PROTOCAL_UDP);
    void remove_tunnel_session(unsigned int session_id);
private:
    void register_data_handlers();
    //前四个字节为会话id
    void handle_read();
    void handle_connection_established(tunnel_session &session);
    std::mutex m_mutex;
    void remove_invalid_tunnel()
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        int64_t time_now=GetTimeNow();
        for(auto i=std::begin(m_session_map);i!=std::end(m_session_map);)
        {
            if(time_now-i->second.last_alive_time>MAX_CACHE_TIME)
            {
                m_session_map.erase(i++);
            }
            else
            {
                i++;
            }
        }
    }
    std::atomic<bool> m_run;
    std::shared_ptr<Channel>m_udp_channel;
    EventLoop *m_event_loop;
    xop::TimerId m_cache_timer_id;
    inline unsigned int get_session_id(const char *buf)const
    {
        return ikcp_getconv(buf);
    }
    static int64_t GetTimeNow()
    {
        auto timePoint = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    }
    std::unordered_map<PROTOCAL_TYPE,TUNNEL_DATA_HANDLER>m_handler_map;
    std::map<unsigned int,tunnel_session>m_session_map;
};
}
#endif // UDP_DATA_TUNNEL_H
