#include"encrption_buf_cache.h"
#include"Socket.h"
using namespace micagent;
Encrption_Buf_Cache::Encrption_Buf_Cache(size_t size)
:m_buf_cache(new std::vector<uint8_t>(size)),
m_availiable_packets(0),
m_write_index(0),
m_read_index(0),
m_size_left(0),
m_head_left(0)
{
    m_buf_cache->resize(size);
}
bool Encrption_Buf_Cache::readFd(int fd,ssize_t &read_len,size_t max_read_len)
{
    read_len=0;
    do{
        size_t availiable_size=m_buf_cache->size()-m_write_index;
        if(availiable_size<max_read_len)
        {
            if(m_buf_cache->size()+max_read_len>MAX_CACHE_SIZE)
            {
                printf("%s %d write overflow !\r\n",__func__,__LINE__);
                reset();
                break;
            }
            m_buf_cache->resize(m_buf_cache->size()+max_read_len);
        }
        read_len=::recv(fd,beginWrite(),max_read_len,0);
        //无可用数据
        if(read_len<=0)break;
        m_write_index+=read_len;
        if(m_size_left>0)
        {
            if(read_len<m_size_left)m_size_left-=read_len;
            else {
                m_head_left=read_len-m_size_left;
                m_size_left=0;
                m_availiable_packets++;
            }
        }
        else {
            m_head_left+=read_len;
        }
        handle_packet_left();
    }while(0);
    return m_availiable_packets>0;
}
bool Encrption_Buf_Cache::inputBuf(void *buf,size_t len)
{
    do{
        if(len==0)break;
        size_t availiable_size=m_buf_cache->size()-m_write_index;
        if(availiable_size<len)
        {
            if(m_buf_cache->size()+len>MAX_CACHE_SIZE)
            {
                printf("%s %d write overflow !\r\n",__func__,__LINE__);
                reset();
                break;
            }
            m_buf_cache->resize(m_buf_cache->size()+len);
        }
        memcpy(begin()+m_write_index,buf,len);
        m_write_index+=len;
        if(m_size_left>0)
        {
            if(len<m_size_left)m_size_left-=len;
            else {
                m_head_left=len-m_size_left;
                m_size_left=0;
                m_availiable_packets++;
            }
        }
        else {
            m_head_left+=len;
        }
        handle_packet_left();
    }while(0);
    return m_availiable_packets>0;
}
Raw_Packet Encrption_Buf_Cache::readPacket()
{
    if(m_availiable_packets==0)return Raw_Packet();
    m_availiable_packets--;
    Encryption_Header *tmp=(Encryption_Header *)(begin()+m_read_index);
    Encryption_Packet packet;
    packet.packet_len=sizeof(Encryption_Header)+tmp->private_data_len+tmp->payload_data_len;
    packet.data.reset(new uint8_t[packet.packet_len]);
    memcpy(packet.data.get(),begin()+m_read_index,packet.packet_len);
    m_read_index+=packet.packet_len;
    if(m_read_index==m_write_index)reset();
    return Encryption_Tool::Instance().Decryption_Data(packet);
}
void Encrption_Buf_Cache::handle_packet_left()
{
    size_t head_len=sizeof(Encryption_Header);
    while(m_head_left>=head_len)
    {
        Encryption_Header *tmp=(Encryption_Header *)(begin()+m_write_index-m_head_left);
        if(!Encryption_Tool::Instance().Check_Packet_Legal(tmp,head_len))
        {
            reset();
            break;
        }
        m_head_left-=head_len;
        size_t data_size=tmp->private_data_len+tmp->payload_data_len;
        if(data_size<=m_head_left)
        {
            m_availiable_packets++;
            m_head_left-=data_size;
        }
        else {
            m_size_left=data_size-m_head_left;
            m_head_left=0;
        }
    }
}
