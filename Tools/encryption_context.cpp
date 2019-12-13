#include "encryption_context.h"
#include "encryption_function.h"
#include<string.h>
using namespace micagent;
void Encryption_Tool::Set_Encryption_Func(uint32_t type,const std::string description,const EncryptionFunction &func1,const EncryptionFunction &func2)
{
    std::lock_guard<std::mutex>locker(m_mutex);
    if(m_func_map.find(type)!=std::end(m_func_map))
    {
        Encryption_Info info;
        info.name=description;
        info.encryption_func=func1;
        info.decryption_func=func2;
        m_func_map.emplace(std::pair<uint32_t,Encryption_Info>(type,std::move(info)));
    }
}
void Encryption_Tool::Remove_Encryption_Func(uint32_t type)
{
    std::lock_guard<std::mutex>locker(m_mutex);
    auto iter=m_func_map.find(type);
    if(iter!=std::end(m_func_map))m_func_map.erase(iter);
}
Encryption_Packet  Encryption_Tool::Encryption_Data(uint32_t type,Raw_Packet &packet)
{
    Encryption_Header head;
    head.protocal_version=version;
    head.encryption_type=type;
    head.payload_type=packet.payload_type;
    Encryption_Info payload_encryption_info;
    Encryption_Info private_encryption_info;
    if(type!=0)
    {
        head.encryption_flag=1;
        std::lock_guard<std::mutex>locker(m_mutex);
        auto iter=m_func_map.find(type);
        if(iter!=std::end(m_func_map))payload_encryption_info=iter->second;
        else {
            head.encryption_flag=0;
        }
        auto iter2=m_func_map.find(micagent::MICAGENT_BASE64);
        if(iter2!=std::end(m_func_map))private_encryption_info=iter2->second;
        else {
            head.encryption_flag=0;
        }
    }
    else {
        head.encryption_flag=0;
    }
    Encryption_Packet ret;
    uint32_t head_len=sizeof(head);
    if(head.encryption_flag)
    {
        auto private_data=private_encryption_info.encryption_func(Handle_Packet(packet.private_data_len,packet.private_data));
        auto payload_data=payload_encryption_info.encryption_func(Handle_Packet(packet.payload_data_len,packet.payload_data));
        head.payload_data_len=payload_data.first;
        head.private_data_len=private_data.first;
        head.private_data_flag=head.private_data_len>0?1:0;
        ret.packet_len=head_len+head.payload_data_len+head.private_data_len;
        ret.data.reset(new uint8_t[ret.packet_len]);
        memcpy(ret.data.get(),&head,head_len);
        if(head.private_data_len>0)memcpy(ret.data.get()+head_len,private_data.second.get(),head.private_data_len);
        if(head.payload_data_len>0)memcpy(ret.data.get()+head_len+head.private_data_len,payload_data.second.get(),\
        head.payload_data_len);
    }
    else {
        head.payload_data_len=packet.payload_data_len;
        head.private_data_len=packet.private_data_len;
        head.private_data_flag=head.private_data_len>0?1:0;
        ret.packet_len=head_len+head.payload_data_len+head.private_data_len;
        ret.data.reset(new uint8_t[ret.packet_len]);
        memcpy(ret.data.get(),&head,head_len);
        if(head.private_data_len>0)memcpy(ret.data.get()+head_len,packet.private_data.get(),head.private_data_len);
        if(head.payload_data_len>0)memcpy(ret.data.get()+head_len+head.private_data_len,packet.payload_data.get(),\
        head.payload_data_len);
    }
    return ret;
}
Raw_Packet  Encryption_Tool::Decryption_Data(Encryption_Packet &packet)
{
    Raw_Packet ret;
    uint32_t head_len=sizeof(Encryption_Header);
    do{
        if(packet.packet_len<head_len)break;
        Encryption_Header head;
        memcpy(&head,packet.data.get(),head_len);
        if(head.private_data_len+head_len+head.payload_data_len!=packet.packet_len)break;
        ret.payload_type=head.payload_type;
        if(0==head.encryption_flag)
        {
            if(head.private_data_flag&&head.private_data_len>0)
            {
                ret.private_data_len=head.private_data_len;
                ret.private_data.reset(new uint8_t[ret.private_data_len]);
                memcpy(ret.private_data.get(),packet.data.get()+head_len,ret.private_data_len);
            }
            if(head.payload_data_len>0)
            {
                ret.payload_data_len=head.payload_data_len;
                ret.payload_data.reset(new uint8_t[ret.payload_data_len]);
                memcpy(ret.payload_data.get(),packet.data.get()+head_len+ret.private_data_len,ret.payload_data_len);
            }
        }
        else {
            //解密部分
            if(head.private_data_flag&&head.private_data_len>0)
            {
                Encryption_Info private_encryption_info;
                {
                    std::lock_guard<std::mutex>locker(m_mutex);
                    auto iter=m_func_map.find(micagent::MICAGENT_BASE64);
                    if(iter!=std::end(m_func_map))private_encryption_info=iter->second;
                    else {
                        break;
                    }
                }
                std::shared_ptr<uint8_t>tmp(new uint8_t[head.private_data_len]);
                memcpy(tmp.get(),packet.data.get()+head_len,head.private_data_len);
                uint32_t private_data_len=head.private_data_len;
                auto private_data=private_encryption_info.decryption_func(Handle_Packet(private_data_len,tmp));
                ret.private_data_len=private_data.first;
                ret.private_data.reset(new uint8_t[ret.private_data_len]);
                memcpy(ret.private_data.get(),private_data.second.get(),ret.private_data_len);
            }
            if(head.payload_data_len>0)
            {
                Encryption_Info payload_encryption_info;
                {
                    std::lock_guard<std::mutex>locker(m_mutex);
                    auto iter=m_func_map.find(head.encryption_type);
                    if(iter!=std::end(m_func_map))payload_encryption_info=iter->second;
                    else {
                        break;
                    }
                }
                std::shared_ptr<uint8_t>tmp(new uint8_t[head.payload_data_len]);
                memcpy(tmp.get(),packet.data.get()+head_len+head.private_data_len,head.payload_data_len);
                uint32_t payload_data_len=head.payload_data_len;
                auto payload_data=payload_encryption_info.decryption_func(Handle_Packet(payload_data_len,tmp));
                ret.payload_data_len=payload_data.first;
                ret.payload_data.reset(new uint8_t[ret.payload_data_len]);
                memcpy(ret.payload_data.get(),payload_data.second.get(),ret.payload_data_len);
            }
        }
    }while(0);
    return ret;
}

void Encryption_Tool::Regsiter_All_Default_Func()
{
    if(m_init)return;
    m_init=true;
    std::lock_guard<std::mutex>locker(m_mutex);
    Encryption_Info info;
    info.name=micagent::encryption_function::get_description_by_type(micagent::MICAGENT_ORIGINAL);
    info.encryption_func=nullptr;
    info.decryption_func=nullptr;
    m_func_map.emplace(std::pair<uint32_t,Encryption_Info>(micagent::MICAGENT_ORIGINAL,info));


    info.name=micagent::encryption_function::get_description_by_type(micagent::MICAGENT_BASE64);
    info.encryption_func=micagent::encryption_function::micagent_enbase64;
    info.decryption_func=micagent::encryption_function::micagent_debase64;
    m_func_map.emplace(std::pair<uint32_t,Encryption_Info>(micagent::MICAGENT_BASE64,info));


    info.name=micagent::encryption_function::get_description_by_type(micagent::MICAGENT_BLOCK16);
    info.encryption_func=micagent::encryption_function::micagent_enblock16;
    info.decryption_func=micagent::encryption_function::micagent_deblock16;
    m_func_map.emplace(std::pair<uint32_t,Encryption_Info>(micagent::MICAGENT_BLOCK16,info));


    info.name=micagent::encryption_function::get_description_by_type(micagent::MICAGENT_BLOCK32);
    info.encryption_func=micagent::encryption_function::micagent_enblock32;
    info.decryption_func=micagent::encryption_function::micagent_deblock32;
    m_func_map.emplace(std::pair<uint32_t,Encryption_Info>(micagent::MICAGENT_BLOCK32,info));


    info.name=micagent::encryption_function::get_description_by_type(micagent::MICAGENT_MAP);
    info.encryption_func=micagent::encryption_function::micagent_enmap;
    info.decryption_func=micagent::encryption_function::micagent_demap;
    m_func_map.emplace(std::pair<uint32_t,Encryption_Info>(micagent::MICAGENT_MAP,info));


    info.name=micagent::encryption_function::get_description_by_type(micagent::MICAGENT_BASETIME);
    info.encryption_func=micagent::encryption_function::micagent_enbasetime;
    info.decryption_func=micagent::encryption_function::micagent_debasetime;
    m_func_map.emplace(std::pair<uint32_t,Encryption_Info>(micagent::MICAGENT_BASETIME,info));


    info.name=micagent::encryption_function::get_description_by_type(micagent::MICAGENT_BASEPOS);
    info.encryption_func=micagent::encryption_function::micagent_enbasepos;
    info.decryption_func=micagent::encryption_function::micagent_debasepos;
    m_func_map.emplace(std::pair<uint32_t,Encryption_Info>(micagent::MICAGENT_BASEPOS,info));
}
