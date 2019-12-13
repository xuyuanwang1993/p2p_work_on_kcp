#include "encryption_function.h"
#include <iostream>
#include<cstring>
#include<set>
#include<random>
#include<assert.h>
#include "encryption_context.h"
#define TEST_LEN 1000
using namespace std;
#define PRINT_RESULT 0
std::shared_ptr<uint8_t>get_uint8_array(uint32_t len)
{
    std::random_device rd;
    std::shared_ptr<uint8_t>ret(new uint8_t[len]);
    for(uint32_t i=0;i<len;i++)
    {
        ret.get()[i]=static_cast<uint8_t>(rd()%256);
    }
    return ret;
}
void base64test()
{
    auto time_start=micagent::encryption_function::GetTimeNow();
    uint32_t test_len=TEST_LEN;
    auto test_string=get_uint8_array(test_len);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(test_string.get(),test_len,"input ");
#endif
    micagent::Handle_Packet input(test_len,test_string);
    auto ret=micagent::encryption_function::micagent_enbase64(input);
#if PRINT_RESULT
    std::string output((char *)ret.second.get(),ret.first);
    std::cout<<"output :"<<output<<std::endl;
#endif
    auto dec=micagent::encryption_function::micagent_debase64(ret);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(dec.second.get(),dec.first,"dec   ");
#endif
    if(micagent::encryption_function::compare_handle_packet(input,dec))std::cout<<"base64test match"<<std::endl;
    micagent::encryption_function::diff_print_time(time_start,micagent::encryption_function::GetTimeNow());
}
void block16test()
{
    auto time_start=micagent::encryption_function::GetTimeNow();
    uint32_t test_len=TEST_LEN;
    auto test_string=get_uint8_array(test_len);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(test_string.get(),test_len,"input ");
#endif
    micagent::Handle_Packet input(test_len,test_string);
    auto ret=micagent::encryption_function::micagent_enblock16(input);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(ret.second.get(),ret.first,"output");
#endif
    auto dec=micagent::encryption_function::micagent_deblock16(ret);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(dec.second.get(),dec.first,"dec   ");
#endif
    if(micagent::encryption_function::compare_handle_packet(input,dec))std::cout<<"block16test match"<<std::endl;
    micagent::encryption_function::diff_print_time(time_start,micagent::encryption_function::GetTimeNow());
}
void block32test()
{
    auto time_start=micagent::encryption_function::GetTimeNow();
    uint32_t test_len=TEST_LEN;
    auto test_string=get_uint8_array(test_len);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(test_string.get(),test_len,"input ");
#endif
    micagent::Handle_Packet input(test_len,test_string);
    auto ret=micagent::encryption_function::micagent_enblock32(input);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(ret.second.get(),ret.first,"output");
#endif
    auto dec=micagent::encryption_function::micagent_deblock32(ret);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(dec.second.get(),dec.first,"dec   ");
#endif
    if(micagent::encryption_function::compare_handle_packet(input,dec))std::cout<<"block32test match"<<std::endl;
    micagent::encryption_function::diff_print_time(time_start,micagent::encryption_function::GetTimeNow());
}
uint8_t get_a_num_or_char()
{
    std::random_device rd;
    uint8_t ret;
    while(1)
    {
        ret=rd()%256;
        if((ret>='0'&&ret<='9')||(ret>='a'&&ret<='z')||(ret>='A'&&ret<='Z'))break;
    }
    return ret;
}
void test_initialize_map()
{
    std::set<uint8_t>first;
    std::set<uint8_t>second;
    for(int i='0';i<='9';i++)
    {
        first.insert(static_cast<uint8_t>(i));
        second.insert(static_cast<uint8_t>(i));
    }
    for(int i='a';i<='z';i++)
    {
        first.insert(static_cast<uint8_t>(i));
        second.insert(static_cast<uint8_t>(i));
    }
    for(int i='A';i<='Z';i++)
    {
        first.insert(static_cast<uint8_t>(i));
        second.insert(static_cast<uint8_t>(i));
    }
    uint8_t encode_char[256];
    uint8_t decode_char[256];
    for(int i=0;i<256;i++)
    {
        encode_char[i]=static_cast<uint8_t>(i);
        decode_char[i]=static_cast<uint8_t>(i);
    }
    while (1) {
        if(first.empty())break;
        uint8_t first_num=*first.begin();
        first.erase(first_num);
        uint8_t second_num;
        while(1)
        {
            uint8_t tmp=get_a_num_or_char();
            if(second.find(tmp)!=std::end(second))
            {
                second_num=tmp;
                second.erase(second_num);
                break;
            }
        }
        encode_char[first_num]=second_num;
        decode_char[second_num]=first_num;
    }

    for(int i='0';i<='9';i++)
    {
        std::cout<<static_cast<char>(i)<<" : "<<static_cast<char>(encode_char[i])<<"   "<<static_cast<char>(decode_char[i])<<std::endl;
    }
    for(int i='a';i<='z';i++)
    {
        std::cout<<static_cast<char>(i)<<" : "<<static_cast<char>(encode_char[i])<<"   "<<static_cast<char>(decode_char[i])<<std::endl;
    }
    for(int i='A';i<='Z';i++)
    {
        std::cout<<static_cast<char>(i)<<" : "<<static_cast<char>(encode_char[i])<<"   "<<static_cast<char>(decode_char[i])<<std::endl;
    }
}
void maptest()
{
    auto time_start=micagent::encryption_function::GetTimeNow();
    uint32_t test_len=TEST_LEN;
    auto test_string=get_uint8_array(test_len);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(test_string.get(),test_len,"input ");
#endif
    micagent::Handle_Packet input(test_len,test_string);
    auto ret=micagent::encryption_function::micagent_enmap(input);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(ret.second.get(),ret.first,"output");
#endif
    auto dec=micagent::encryption_function::micagent_demap(ret);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(dec.second.get(),dec.first,"dec   ");
#endif
    if(micagent::encryption_function::compare_handle_packet(input,dec))std::cout<<"maptest match"<<std::endl;
    micagent::encryption_function::diff_print_time(time_start,micagent::encryption_function::GetTimeNow());
}

void basetimetest()
{
    auto time_start=micagent::encryption_function::GetTimeNow();
    uint32_t test_len=TEST_LEN;
    auto test_string=get_uint8_array(test_len);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(test_string.get(),test_len,"input ");
#endif
    micagent::Handle_Packet input(test_len,test_string);
    auto ret=micagent::encryption_function::micagent_enbasetime(input);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(ret.second.get(),ret.first,"output");
#endif
    auto dec=micagent::encryption_function::micagent_debasetime(ret);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(dec.second.get(),dec.first,"dec   ");
#endif
    if(micagent::encryption_function::compare_handle_packet(input,dec))std::cout<<"basetimetest match"<<std::endl;
    micagent::encryption_function::diff_print_time(time_start,micagent::encryption_function::GetTimeNow());
}
void basepostest()
{
    auto time_start=micagent::encryption_function::GetTimeNow();
    uint32_t test_len=TEST_LEN;
    auto test_string=get_uint8_array(test_len);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(test_string.get(),test_len,"input ");
#endif
    micagent::Handle_Packet input(test_len,test_string);
    auto ret=micagent::encryption_function::micagent_enbasepos(input);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(ret.second.get(),ret.first,"output");
#endif
    auto dec=micagent::encryption_function::micagent_debasepos(ret);
#if PRINT_RESULT
    micagent::encryption_function::dump_uint_8(dec.second.get(),dec.first,"dec   ");
#endif
    if(micagent::encryption_function::compare_handle_packet(input,dec))std::cout<<"basepostest match"<<std::endl;
    micagent::encryption_function::diff_print_time(time_start,micagent::encryption_function::GetTimeNow());
}
void context_test(uint32_t type)
{
    //assert(type>=0&&type<=6);
    auto &instance=micagent::Encryption_Tool::Instance();
    instance.Regsiter_All_Default_Func();
    micagent::Raw_Packet packet;
    packet.payload_type=96;
    packet.private_data_len=100;
    packet.private_data=get_uint8_array(packet.private_data_len);
    packet.payload_data_len=144;
    packet.payload_data=get_uint8_array(packet.payload_data_len);
    auto ret=instance.Encryption_Data(type,packet);
    auto packet2=instance.Decryption_Data(ret);
    bool match=false;
    do{
        if(packet.payload_type!=packet2.payload_type)break;
        if(packet.payload_data_len!=packet2.payload_data_len)break;
        if(packet.private_data_len!=packet2.private_data_len)break;
        micagent::Handle_Packet tmp1(packet.private_data_len,packet.private_data);
        micagent::Handle_Packet tmp2(packet2.private_data_len,packet2.private_data);
        if(!micagent::encryption_function::compare_handle_packet(tmp1,tmp2))break;
        micagent::Handle_Packet tmp3(packet.payload_data_len,packet.payload_data);
        micagent::Handle_Packet tmp4(packet2.payload_data_len,packet2.payload_data);
        if(!micagent::encryption_function::compare_handle_packet(tmp3,tmp4))break;
        match=true;
    }while(0);
    if(match)
    {
        std::cout<<"context_test match for type "<<micagent::encryption_function::get_description_by_type(static_cast<micagent::EncryptionType>(type))<<std::endl;
    }
}
int main()
{
//    base64test();
//    block16test();
//    block32test();
//    test_initialize_map();
//    maptest();
//    basetimetest();
//    basepostest();
    context_test(10);
    return 0;
}
