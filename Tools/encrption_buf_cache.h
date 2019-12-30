#ifndef ENCRPTION_BUF_CACHE_H
#define ENCRPTION_BUF_CACHE_H
#include "encryption_context.h"
#include <vector>
namespace micagent {
using std::shared_ptr;
class Encrption_Buf_Cache{
private:
    /**
     * @brief m_buf_cache 数据缓存
     */
    shared_ptr<std::vector<uint8_t>>m_buf_cache;
    /**
     * @brief MAX_TCP_PER_SIZE TCP数据单个包最大长度
     */
    static const size_t MAX_TCP_PER_SIZE=2048;
    /**
     * @brief MAX_CACHE_SIZE 最大缓存长度
     */
    static const size_t MAX_CACHE_SIZE=1000000;
    /**
     * @brief m_availiable_packets 可读包个数
     */
    uint32_t m_availiable_packets;
    /**
     * @brief m_write_index 写入位置
     */
    size_t m_write_index;
    /**
     * @brief m_read_index 读取位置
     */
    size_t m_read_index;
    /**
     * @brief m_size_left 当前packet待读取的数据大小
     */
    size_t m_size_left;
    /**
     * @brief m_head_left 头部信息遗留字节数
     */
    size_t m_head_left;
public:
    Encrption_Buf_Cache(size_t size=10000);
    ~Encrption_Buf_Cache(){}
    /**
     * @brief readFd 从网络socket中读取数据包
     * @param fd 待读取的fd描述符
     * @param read_len 实际读取长度
     * @return 有数据packet接收完毕可读后返回true 否则返回false
     */
    bool readFd(int fd,ssize_t &read_len,size_t max_read_len=MAX_TCP_PER_SIZE);
    /**
     * @brief inputBuf 以buf形式输入
     * @param buf 数据
     * @param len 数据长度
     * @return 有packet可读时返回true 否则返回false
     */
    bool inputBuf(void *buf,size_t len);
    /**
     * @brief readPacket 从缓冲区中读取一帧
     * @return 返回解码之后的原始帧
     */
    Raw_Packet readPacket();
private:
     uint8_t* begin()
    { return &*m_buf_cache->begin(); }

    const uint8_t* begin() const
    { return &*m_buf_cache->begin(); }

    uint8_t* beginWrite()
    { return begin() + m_write_index; }

    const uint8_t* beginWrite() const
    { return begin() + m_write_index; }

    void reset()
    {
        m_availiable_packets=0;
        m_write_index=0;
        m_read_index=0;
        m_size_left=0;
        m_head_left=0;
    }
    /**
     * @brief handle_packet_left 处理剩余数据并判断下次需要读取的数据
     */
    void handle_packet_left();
};
}
#endif // ENCRPTION_BUF_CACHE_H
