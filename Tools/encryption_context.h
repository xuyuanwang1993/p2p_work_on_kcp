#ifndef ENCRYPTION_CONTEXT_H
#define ENCRYPTION_CONTEXT_H
#include <functional>
#include<map>
#include<string>
#include<cstdint>
#include<memory>
#include<mutex>
#include<atomic>
namespace micagent {
typedef  struct _Encryption_Header{
    uint32_t protocal_version:3;//协议版本
    uint32_t encryption_flag:1;//是否加密标识位
    uint32_t encryption_type:3;//加密方式
    uint32_t private_data_flag:1;//是否携带私有数据标识
    uint32_t private_data_len:24;//私有数据长度 私有数据只进行base64加密
    uint32_t payload_type:8;//负载类型
    uint32_t payload_data_len:24;//负载长度
}Encryption_Header;

typedef  struct _Encryption_Packet{
    uint32_t packet_len;//packet长度
    std::shared_ptr<uint8_t>data;//数据部分
    _Encryption_Packet(){
        packet_len=0;
        data.reset();
    }
}Encryption_Packet;

typedef  struct _Raw_Packet{
    uint32_t private_data_len;
    std::shared_ptr<uint8_t>private_data;
    uint8_t payload_type;
    uint32_t payload_data_len;
    std::shared_ptr<uint8_t>payload_data;
    _Raw_Packet(){
        private_data_len=0;
        private_data.reset();
        payload_type=0;
        payload_data_len=0;
        payload_data.reset();
    }
}Raw_Packet;
using SharedPacket=std::pair<uint32_t,std::shared_ptr<uint8_t>>;
using EncryptionFunction=std::function<SharedPacket(SharedPacket)>;//加密解密函数原型
typedef struct _Encryption_Info{
    std::string name;
    EncryptionFunction encryption_func;
    EncryptionFunction decryption_func;
    _Encryption_Info(){
        name="default";
        encryption_func=nullptr;
        decryption_func=nullptr;
    }
}Encryption_Info;
class Encryption_Tool{
    static constexpr int version=1;
public:
    static Encryption_Tool &Instance(){static Encryption_Tool instance;return instance;}
    /**
     * @brief Set_Encryption_Func   根据type设置加密函数
     * @param type 调用加密接口的type与此处一致
     * @param description 描述此加密方法
     * @param func1 加密函数
     * @param func2 解密函数
     */
    void Set_Encryption_Func(uint32_t type,const std::string description,const EncryptionFunction &func1,const EncryptionFunction &func2);
    /**
     * @brief Remove_Encryption_Func 移除加密函数
     * @param type 加密函数类型
     */
    void Remove_Encryption_Func(uint32_t type);
    /**
     * @brief Encryption_Data 数据加密
     * @param type 加密类型
     * @param packet 原始数据包
     * @return 加密后的数据包  结构为 header+private_data+payload_data
     */
    Encryption_Packet  Encryption_Data(uint32_t type,Raw_Packet &packet);
    /**
     * @brief Decryption_Data 数据解密
     * @param packet 待解密的数据包
     * @return 解密后的数据包
     */
    Raw_Packet  Decryption_Data(Encryption_Packet &packet);
    /**
     * @brief Regsiter_All_Func 注册默认加密函数
     */
    void Regsiter_All_Default_Func();
private:
    Encryption_Tool(){}
    ~Encryption_Tool(){}
    /**
     * @brief m_func_map 存储加密函数
     */
    std::atomic<bool> m_init;
    std::map<uint32_t,Encryption_Info>m_func_map;
    std::mutex m_mutex;
};
}
#endif // ENCRYPTION_CONTEXT_H
