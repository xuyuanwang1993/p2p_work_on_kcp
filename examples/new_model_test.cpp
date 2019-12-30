#include "encryption_function.h"
#include <iostream>
#include<cstring>
#include<set>
#include<random>
#include<assert.h>
#include "encryption_context.h"
#include <chrono>
#include<thread>
#include"SocketUtil.h"
#include "encrption_buf_cache.h"
#define TEST_LEN 1000
using namespace std;
#define PRINT_NONE   "\033[0m"
#define PRINT_RED    "\033[0;31m"
#define PRINT_GREEN  "\033[0;32m"
#define TEST_DEBUG
#ifdef TEST_DEBUG
#define TEST_LOG(fmt,...) do{\
    printf("[LOG:%s,%s,%d]"  fmt  "\n",__FUNCTION__,__FILE__,__LINE__ , ##__VA_ARGS__);\
    }while(0);
#else
#define TEST_LOG(fmt , ...)
#endif
#define PRINT_RESULT 0
std::shared_ptr<uint8_t>get_uint8_array(uint32_t len)
{
    std::random_device rd;
    std::shared_ptr<uint8_t>ret(new uint8_t[len]);
//    for(uint32_t i=0;i<len;i++)
//    {
//        ret.get()[i]=static_cast<uint8_t>(rd()%256);
//    }
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
void test_check_header(uint32_t type)
{
    auto &instance=micagent::Encryption_Tool::Instance();
    micagent::Raw_Packet packet;
    packet.payload_type=96;
    packet.private_data_len=100;
    packet.private_data=get_uint8_array(packet.private_data_len);
    packet.payload_data_len=144;
    packet.payload_data=get_uint8_array(packet.payload_data_len);
    auto ret=instance.Encryption_Data(type,packet);
    std::cout<<instance.Check_Packet_Legal(ret.data.get(),ret.packet_len)<<std::endl;
}
static bool test_cache_exit=false;
static bool test_cache_exit2=false;
static int threads_run=0;
static int threads_run2=0;
static size_t send_size=0;
static size_t send_size2=0;
static size_t slice_size=1024;
void test_cache_buf(uint32_t buf_counts,uint32_t buf_size=20000)
{
    threads_run=1;
    auto send_func=[buf_counts,buf_size](){
        std::this_thread::sleep_for(std::chrono::seconds(2));
        struct sockaddr_in addr={0};
        xop::SocketUtil::makeSockAddr(addr,"127.0.0.1",23333);
        auto &instance=micagent::Encryption_Tool::Instance();
        int fd=::socket(AF_INET,SOCK_DGRAM,0);
        std::random_device rd;
        uint32_t type=rd()%7;
        micagent::Encrption_Buf_Cache buf;
        int count=0;
        for (uint32_t i=0;i<buf_counts;i++) {
            micagent::Raw_Packet packet;
            packet.payload_type=96;
            packet.private_data_len=rd()%32+32;
            packet.private_data=get_uint8_array(packet.private_data_len);
            packet.payload_data_len=rd()%buf_size+100;
            if(i%50==0)packet.payload_data_len=rd()%1000+400000;
            packet.payload_data=get_uint8_array(packet.payload_data_len);
            char print_buf[31]={0};
            char printf_buf2[31]={0};
            for (int j=0;j<10;j++) {
                sprintf(print_buf+j*3,"%02x ",packet.private_data.get()[j]);
                sprintf(printf_buf2+j*3,"%02x ",packet.payload_data.get()[j]);
            }
            TEST_LOG(PRINT_GREEN "%d send %d private %s payload %s!" PRINT_NONE,i,packet.private_data_len+packet.payload_data_len,print_buf,printf_buf2);
            auto ret=instance.Encryption_Data(type,packet);
            for(int j=0;j<ret.packet_len;j+=slice_size)
            {
                int send_len=j+slice_size>ret.packet_len?ret.packet_len-j:slice_size;
                //sendto(fd,ret.data.get()+j,send_len,0,(struct sockaddr *)&addr,sizeof(addr));
                if(buf.inputBuf(ret.data.get()+j,send_len))
                {
                    auto packet=buf.readPacket();
                    char print_buf[31]={0};
                    char printf_buf2[31]={0};
                    for (int k=0;k<10;k++) {
                        sprintf(print_buf+k*3,"%02x ",packet.private_data.get()[k]);
                        sprintf(printf_buf2+k*3,"%02x ",packet.payload_data.get()[k]);
                    }
                    TEST_LOG(PRINT_RED "%d recv %d private %s payload %s!" PRINT_NONE,count++,packet.private_data_len+packet.payload_data_len,print_buf,printf_buf2);
                }
            }
            send_size+=ret.packet_len;
            //模拟网络发送
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        threads_run--;
        test_cache_exit=true;
    };
    auto recv_func=[](){
        int fd=::socket(AF_INET,SOCK_DGRAM,0);
        xop::SocketUtil::setReuseAddr(fd);
        xop::SocketUtil::setReusePort(fd);
        xop::SocketUtil::setNonBlock(fd);
        xop::SocketUtil::setSendBufSize(fd,1024*1024);
        xop::SocketUtil::bind(fd,"127.0.0.1",23333);
        micagent::Encrption_Buf_Cache buf;
        ssize_t read_len;
        int count=0;
        while(!test_cache_exit)
        {
            if(buf.readFd(fd,read_len,slice_size))
            {
                auto packet=buf.readPacket();
                char print_buf[31]={0};
                char printf_buf2[31]={0};
                for (int j=0;j<10;j++) {
                    sprintf(print_buf+j*3,"%02x ",packet.private_data.get()[j]);
                    sprintf(printf_buf2+j*3,"%02x ",packet.payload_data.get()[j]);
                }
                TEST_LOG(PRINT_RED "%d recv %d private %s payload %s!" PRINT_NONE,count++,packet.private_data_len+packet.payload_data_len,print_buf,printf_buf2);
            }
        }
        threads_run--;
    };
    //    std::thread t1(recv_func);
    //    t1.detach();
    std::thread t2(send_func);
    t2.detach();
    while(threads_run!=0)std::this_thread::sleep_for(std::chrono::milliseconds(40));
}
static size_t read_total=0;
void test_cache_buf2(uint32_t buf_counts,uint32_t buf_size=20000)
{
    threads_run2=2;
    auto send_func=[buf_counts,buf_size](){
        std::this_thread::sleep_for(std::chrono::seconds(2));
        struct sockaddr_in addr={0};
        xop::SocketUtil::makeSockAddr(addr,"127.0.0.1",23334);

        auto &instance=micagent::Encryption_Tool::Instance();
        int fd=::socket(AF_INET,SOCK_DGRAM,0);
        xop::SocketUtil::setSendBufSize(fd,320*1024);
        std::random_device rd;
        uint32_t type=1;
        for (uint32_t i=0;i<buf_counts;i++) {
            micagent::Raw_Packet packet;
            packet.payload_type=96;
            packet.private_data_len=rd()%32+32;
            packet.private_data=get_uint8_array(packet.private_data_len);
            packet.payload_data_len=rd()%buf_size+100;
           if(i%50==0)packet.payload_data_len=rd()%1000+280000;
            packet.payload_data=get_uint8_array(packet.payload_data_len);
            char print_buf[31]={0};
            char printf_buf2[31]={0};
            for (int j=0;j<10;j++) {
                sprintf(print_buf+j*3,"%02x ",packet.private_data.get()[j]);
                sprintf(printf_buf2+j*3,"%02x ",packet.payload_data.get()[j]);
            }
            TEST_LOG(PRINT_GREEN "%d send %d private %s payload %s!" PRINT_NONE,i,packet.private_data_len+packet.payload_data_len,print_buf,printf_buf2);
            auto ret=instance.Encryption_Data(type,packet);
            for(int j=0;j<ret.packet_len;j+=slice_size)
            {
                int send_len=j+slice_size>ret.packet_len?ret.packet_len-j:slice_size;
                sendto(fd,ret.data.get()+j,send_len,0,(struct sockaddr *)&addr,sizeof(addr));
            }
            send_size2+=ret.packet_len;
            //模拟网络发送
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        //std::this_thread::sleep_for(std::chrono::milliseconds(25));
        threads_run2--;
        test_cache_exit2=true;
    };
    auto recv_func=[](){
        int fd=::socket(AF_INET,SOCK_DGRAM,0);
        xop::SocketUtil::setReuseAddr(fd);
        xop::SocketUtil::setReusePort(fd);
         xop::SocketUtil::setNonBlock(fd);
         xop::SocketUtil::setRecvBufSize(fd,320*1024);
        xop::SocketUtil::bind(fd,"127.0.0.1",23334);
        micagent::Encrption_Buf_Cache buf;
        ssize_t read_len;
        int count=0;
        int timeout=1000;
        while(!test_cache_exit2)
        {
            fd_set fdRead;
            FD_ZERO(&fdRead);
            FD_SET(fd, &fdRead);
            struct timeval tv = { timeout / 1000, timeout % 1000 * 1000 };
            select(fd + 1, NULL, &fdRead, NULL, &tv);
            if (!FD_ISSET(fd, &fdRead))continue;
            if(buf.readFd(fd,read_len,slice_size))
            {
                auto packet=buf.readPacket();
                char print_buf[31]={0};
                char printf_buf2[31]={0};
                for (int j=0;j<10;j++) {
                    sprintf(print_buf+j*3,"%02x ",packet.private_data.get()[j]);
                    sprintf(printf_buf2+j*3,"%02x ",packet.payload_data.get()[j]);
                }
                TEST_LOG(PRINT_RED "%d recv %d private %s payload %s!" PRINT_NONE,count++,packet.private_data_len+packet.payload_data_len,print_buf,printf_buf2);
            }
            if(read_len>0)
            {
                printf("recv %ld \r\n",read_len);
                read_total+=read_len;
            }
        }
        threads_run2--;
    };
    std::thread t1(recv_func);
    t1.detach();
    std::thread t2(send_func);
    t2.detach();
    while(threads_run2!=0)std::this_thread::sleep_for(std::chrono::milliseconds(40));
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
    //    context_test(2);
    //    test_check_header(10);
    uint32_t packet_nums=1000;
    uint32_t packet_max_size=30000;
    auto time_start=micagent::encryption_function::GetTimeNow();
    //test_cache_buf(packet_nums,packet_max_size);
    auto time_diff=micagent::encryption_function::diff_print_time(time_start,micagent::encryption_function::GetTimeNow());
    auto time_start2=micagent::encryption_function::GetTimeNow();
    test_cache_buf2(packet_nums,packet_max_size);
    auto time_diff2=micagent::encryption_function::diff_print_time(time_start2,micagent::encryption_function::GetTimeNow());
    printf("send_rate %lfMB/s\r\n",static_cast<double>(send_size*1000)/(1024*1024*time_diff));
    printf("send_rate %lfMB/s\r\n",static_cast<double>(send_size2*1000)/(1024*1024*time_diff2));
    printf("send %ld  recv %ld \r\n",send_size2,read_total);
    return 0;
}
