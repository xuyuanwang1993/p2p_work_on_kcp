#ifndef ENCRYPTION_FUNCTION_H
#define ENCRYPTION_FUNCTION_H
#include <functional>
#include<map>
#include<string>
#include<cstdint>
#include<memory>
#include<chrono>
namespace micagent {
typedef enum {
    MICAGENT_ORIGINAL,//不加密
    MICAGENT_BASE64,//BASE64 加密
    MICAGENT_BLOCK16,//16字节分块映射加密
    MICAGENT_BLOCK32,//32字节分块映射加密
    MICAGENT_MAP,//字符映射
    MICAGENT_BASETIME,//基于系统时间进行加密
    MICAGENT_BASEPOS,//基于位置进行加密
}EncryptionType;
using Handle_Packet=std::pair<uint32_t,std::shared_ptr<uint8_t>>;
class encryption_function{
public:
    static Handle_Packet micagent_enbase64(Handle_Packet packet);
    static Handle_Packet micagent_debase64(Handle_Packet packet);

    static Handle_Packet micagent_enblock16(Handle_Packet packet);
    static Handle_Packet micagent_deblock16(Handle_Packet packet);

    static Handle_Packet micagent_enblock32(Handle_Packet packet);
    static Handle_Packet micagent_deblock32(Handle_Packet packet);

    static Handle_Packet micagent_enmap(Handle_Packet packet);
    static Handle_Packet micagent_demap(Handle_Packet packet);

    static Handle_Packet micagent_enbasetime(Handle_Packet packet);
    static Handle_Packet micagent_debasetime(Handle_Packet packet);

    static Handle_Packet micagent_enbasepos(Handle_Packet packet);
    static Handle_Packet micagent_debasepos(Handle_Packet packet);
    static int64_t GetTimeNow()
    {
        auto timePoint = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    }
    static bool compare_handle_packet(const Handle_Packet&packet1,const Handle_Packet&packet2);
    static void  dump_uint_8(const uint8_t *buf,uint32_t len,std::string info=std::string());
    static void diff_print_time(int64_t time1,int64_t time2);
    static std::string &get_description_by_type(EncryptionType type);
    static Handle_Packet make_handle_packet(uint32_t len);
};
}
#endif // ENCRYPTION_FUNCTION_H
