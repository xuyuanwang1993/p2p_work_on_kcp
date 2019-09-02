#ifndef KCP_MANAGE_H
#define KCP_MANAGE_H
#include <functional>
#include <memory>
#include "EventLoop.h"
#include "ikcp.h"
#include <atomic>
#include "BufferReader.h"
#include "BufferWriter.h"
#include <unordered_map>
namespace sensor_net{
    typedef struct _data_ptr{
        int channel_id;
        std::string src_name;
        _data_ptr(int _channel_id,std::string _src_name)
        {
            channel_id=_channel_id;
            src_name=_src_name;
        }
    }data_ptr;
    typedef std::function<void(std::shared_ptr<char> buf,int ,struct IKCPCB *kcp,std::shared_ptr<sensor_net::data_ptr>)> RecvDataCallback;
    typedef std::function<void()>ClearCallback;
    typedef std::function<void(std::shared_ptr<sensor_net::data_ptr>)>LostConnectionCallback;
    enum KCP_TRANSFER_MODE{
        DEFULT_MODE,//默认模式,不启用加速，内部时钟10ms 禁止快速重传，启用流控
        NORMAL_MODE,//常规模式，不启用加速，内部时钟10ms 禁止快速重传，关闭流控
        FAST_MODE,//快速模式,启用加速,内部时钟10ms,快速重传,关闭流控
        CUSTOM_MODE,//自定义模式，时钟不要小于5ms 不要大于帧间隔 接收端不要更改传输模式
    };
    class KCP_Interface{
        const int MAX_WINDOW_SIZE=2048;
        const int MAX_TIMEOUT_TIME=5*1000;//5s
        const int MAX_FRAMESIZE=204800;//200*1024
    public:
        KCP_Interface(int fd,unsigned int conv_id,xop::EventLoop *event_loop,ClearCallback clearCB,RecvDataCallback recvCB,std::shared_ptr<data_ptr>data,int window_size=32);
        KCP_Interface(std::shared_ptr<xop::Channel>channel,unsigned int conv_id,xop::EventLoop *event_loop,ClearCallback clearCB,RecvDataCallback recvCB,std::shared_ptr<data_ptr>data,int window_size=32);
        //发送用户数据
        void Send_Userdata(std::shared_ptr<char>buf,int len);
        int Send(const char *buf,int len);
        void Update(int64_t timenow)
        {
            ikcp_update(m_kcp,(IUINT32)timenow);
            if(m_last_alive_time==0)m_last_alive_time=timenow;
            if(m_last_alive_time!=0&&timenow-m_last_alive_time>MAX_TIMEOUT_TIME)
            {
                if(m_lost_connectionCB)
                {
                    m_lost_connectionCB(m_data);
                }
                else
                {
                    clear();
                }
            }
        };
        void SetTransferMode(KCP_TRANSFER_MODE mode,int nodelay=0, int interval=10, int resend=0, int nc=0);
        void SetLostConnectionCallback(const LostConnectionCallback &cb){m_lost_connectionCB=cb;};
        ~KCP_Interface();
         bool GetInterfaceIsValid(){return m_have_cleared?false:true;};
    private:
        bool CheckTransWindow();
        void clear();
        std::shared_ptr<xop::BufferReader> m_readBufferPtr;
        std::shared_ptr<xop::Channel> m_udp_channel;
        struct IKCPCB *m_kcp;
        std::atomic<bool> m_have_cleared;//连接失效标识
        std::shared_ptr<xop::TaskScheduler> m_taskScheduler;
        ClearCallback m_clear_CB;
        RecvDataCallback m_recvCB;
        LostConnectionCallback m_lost_connectionCB;//连接中断回调处理函数
        std::shared_ptr<data_ptr> m_data;//用户数据
        int m_fd;
        int64_t m_last_alive_time=0;
    };

    class KCP_Manager{
    public:
        static int64_t GetTimeNow()
        {
            auto timePoint = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
        }

        static KCP_Manager &GetInstance()
        {//单例模式
            static KCP_Manager manager;
            return manager;
        }
        void Config(xop::EventLoop *event_loop)
        {
            if(!m_init&&event_loop)
            {
                m_init=true;
                m_event_loop=event_loop;
            }
        }
        std::shared_ptr<KCP_Interface> AddConnection(int fd,std::string ip,int port,unsigned int conv_id,RecvDataCallback recvCB,std::shared_ptr<data_ptr>data,int window_size=32);
        std::shared_ptr<KCP_Interface> AddConnection(std::shared_ptr<xop::Channel> channel,unsigned int conv_id,RecvDataCallback recvCB,std::shared_ptr<data_ptr>data,int window_size=32);
        std::shared_ptr<KCP_Interface>GetConnectionHandle(int conv_id)
        {
            if(!m_init)return nullptr;
            std::lock_guard<std::mutex> locker(m_mutex);
            auto iter=m_kcp_map.find(conv_id);
            if(iter!=std::end(m_kcp_map))return iter->second;
            return nullptr;
        }
        void CloseConnection(int conv_id)
        {//移除连接
            if(!m_init)return ;
            std::lock_guard<std::mutex> locker(m_mutex);
            if(m_kcp_map.find(conv_id)!=std::end(m_kcp_map))
            {
                m_kcp_map.erase(conv_id);
            }
        }
        void StartUpdateLoop();
    private:
        bool UpdateLoop();
        std::atomic<bool> m_init;
        KCP_Manager(){};
        ~KCP_Manager(){};
        std::mutex m_mutex;
        xop::EventLoop *m_event_loop;
        std::unordered_map<int,std::shared_ptr<KCP_Interface>> m_kcp_map;
    };
};
#endif // KCP_MANAGE_H
