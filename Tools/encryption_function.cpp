#include "encryption_function.h"
#include<cstdlib>
#include<cstring>
#include<iostream>
#include<mutex>
using namespace micagent;
Handle_Packet encryption_function::micagent_enbase64(Handle_Packet packet)
{
    static const uint8_t base64Char[] ="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t output_data_len=0;
    std::shared_ptr<uint8_t>output_data;
    do{
        if(data_len<=0)break;
        unsigned const numOrig24BitValues = data_len/3;
        bool havePadding = data_len> numOrig24BitValues*3;//判断是否有余数
        bool havePadding2 = data_len == numOrig24BitValues*3 + 2;//判断余数是否等于2
        unsigned const numResultBytes = 4*(numOrig24BitValues + havePadding);//计算最终结果的size
        output_data_len=numResultBytes;
        output_data.reset(new uint8_t[output_data_len]);//确定返回字符串大小
        uint32_t i;
        for (i = 0; i < numOrig24BitValues; ++i) {
            output_data.get()[4*i+0] = base64Char[(data[3*i]>>2)&0x3F];
            output_data.get()[4*i+1] = base64Char[(((data[3*i]&0x3)<<4) | (data[3*i+1]>>4))&0x3F];
            output_data.get()[4*i+2] = base64Char[((data[3*i+1]<<2) | (data[3*i+2]>>6))&0x3F];
            output_data.get()[4*i+3] = base64Char[data[3*i+2]&0x3F];
        }
        //处理不足3位的情况
        //余1位需在后面补齐2个'='
        //余2位需补齐一个'='
        if (havePadding) {
            output_data.get()[4*i+0] = base64Char[(data[3*i]>>2)&0x3F];
            if (havePadding2) {
                output_data.get()[4*i+1] = base64Char[(((data[3*i]&0x3)<<4) | (data[3*i+1]>>4))&0x3F];
                output_data.get()[4*i+2] = base64Char[(data[3*i+1]<<2)&0x3F];
            } else {
                output_data.get()[4*i+1] = base64Char[((data[3*i]&0x3)<<4)&0x3F];
                output_data.get()[4*i+2] = '=';
            }
            output_data.get()[4*i+3] = '=';
        }
    }while(0);
    return Handle_Packet(output_data_len,output_data);
}
Handle_Packet encryption_function::micagent_debase64(Handle_Packet packet)
{
    static uint8_t base64DecodeTable[256];
    static std::once_flag oc_init;
    std::call_once(oc_init,[&](){
        int i;//初始化映射表
        for (i = 0; i < 256; ++i) base64DecodeTable[i] = 0x80;
        for (i = 'A'; i <= 'Z'; ++i) base64DecodeTable[i] = static_cast<uint8_t>(0 + (i - 'A'));
        for (i = 'a'; i <= 'z'; ++i) base64DecodeTable[i] = static_cast<uint8_t>(26 + (i - 'a'));
        for (i = '0'; i <= '9'; ++i) base64DecodeTable[i] = static_cast<uint8_t>(52 + (i - '0'));
        base64DecodeTable[static_cast<uint8_t>('+')] = 62;
        base64DecodeTable[static_cast<uint8_t>('/')] = 63;
        base64DecodeTable[static_cast<uint8_t>('=')] = 0;
    });
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t output_data_len=0;
    std::shared_ptr<uint8_t>output_data;
    do{
        if(data_len%4!=0)break;
        uint32_t blocks=data_len/4;
        output_data_len=blocks*3;
        output_data.reset(new uint8_t[output_data_len]);
        if(data[data_len-1]=='=')output_data_len--;
        if(data[data_len-2]=='=')output_data_len--;
        uint32_t wirte_inex=0;
        for(uint32_t i=0;i<blocks;i++)
        {
            uint32_t begin=i*4;
            uint8_t tmp[4];
            for(uint32_t j=0;j<4;j++)
            {
                tmp[j]=base64DecodeTable[data[j+begin]];
                if((tmp[j]&0x80)!=0)tmp[j]=0;
            }
            output_data.get()[wirte_inex++]=(tmp[0]<<2) | (tmp[1]>>4);
            output_data.get()[wirte_inex++] = (tmp[1]<<4) | (tmp[2]>>2);
            output_data.get()[wirte_inex++] = (tmp[2]<<6) | tmp[3];
        }
    }while(0);
    return Handle_Packet(output_data_len,output_data);
}
Handle_Packet encryption_function::micagent_enblock16(Handle_Packet packet)
{
    uint32_t per_block=16;
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t blocks=data_len/per_block;
    uint32_t output_data_len=data_len;
    std::shared_ptr<uint8_t>output_data;
    if(output_data_len>0)output_data.reset(new uint8_t[output_data_len]);
    for(uint32_t i=0;i<blocks;i++)
    {
        uint32_t sum=0;
        uint32_t begin=i*per_block;
        for(uint32_t j=0;j<per_block;j++) sum+=data[j+begin];
        uint32_t offset=sum%per_block;
        for(uint32_t j=0;j<per_block;j++)output_data.get()[j+begin]=static_cast<uint8_t>((data[j+begin]+j+offset)%256);
    }
    for(uint32_t i=blocks*per_block;i<data_len;i++,data_len--)
    {
        output_data.get()[i]=data[data_len-1];
        output_data.get()[data_len-1]=data[i];
    }
    return Handle_Packet(output_data_len,output_data);
}
Handle_Packet encryption_function::micagent_deblock16(Handle_Packet packet)
{
    uint32_t per_block=16;
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t blocks=data_len/per_block;
    uint32_t output_data_len=data_len;
    std::shared_ptr<uint8_t>output_data;
    if(output_data_len>0)output_data.reset(new uint8_t[output_data_len]);
    for(uint32_t i=0;i<blocks;i++)
    {
        uint32_t sum=0;
        uint32_t begin=i*per_block;
        for(uint32_t j=0;j<per_block;j++) sum+=data[j+begin];
        uint32_t offset=(sum+per_block/2)%per_block;//per_block/2是修正量
        for(uint32_t j=0;j<per_block;j++)output_data.get()[j+begin]=static_cast<uint8_t>((data[j+begin]+256-j-offset)%256);
    }
    for(uint32_t i=blocks*per_block;i<data_len;i++,data_len--)
    {
        output_data.get()[i]=data[data_len-1];
        output_data.get()[data_len-1]=data[i];
    }
    return Handle_Packet(output_data_len,output_data);
}

Handle_Packet encryption_function::micagent_enblock32(Handle_Packet packet)
{
    uint32_t per_block=32;
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t blocks=data_len/per_block;
    uint32_t output_data_len=data_len;
    std::shared_ptr<uint8_t>output_data;
    if(output_data_len>0)output_data.reset(new uint8_t[output_data_len]);
    for(uint32_t i=0;i<blocks;i++)
    {
        uint32_t sum=0;
        uint32_t begin=i*per_block;
        for(uint32_t j=0;j<per_block;j++) sum+=data[j+begin];
        uint32_t offset=sum%per_block;
        for(uint32_t j=0;j<per_block;j++)output_data.get()[j+begin]=static_cast<uint8_t>((data[j+begin]+j+offset)%256);
    }
    for(uint32_t i=blocks*per_block;i<data_len;i++,data_len--)
    {
        output_data.get()[i]=data[data_len-1];
        output_data.get()[data_len-1]=data[i];
    }
    return Handle_Packet(output_data_len,output_data);
}
Handle_Packet encryption_function::micagent_deblock32(Handle_Packet packet)
{
    uint32_t per_block=32;
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t blocks=data_len/per_block;
    uint32_t output_data_len=data_len;
    std::shared_ptr<uint8_t>output_data;
    if(output_data_len>0)output_data.reset(new uint8_t[output_data_len]);
    for(uint32_t i=0;i<blocks;i++)
    {
        uint32_t sum=0;
        uint32_t begin=i*per_block;
        for(uint32_t j=0;j<per_block;j++) sum+=data[j+begin];
        uint32_t offset=(sum+per_block/2)%per_block;//per_block/2是修正量
        for(uint32_t j=0;j<per_block;j++)output_data.get()[j+begin]=static_cast<uint8_t>((data[j+begin]+256-j-offset)%256);
    }
    for(uint32_t i=blocks*per_block;i<data_len;i++,data_len--)
    {
        output_data.get()[i]=data[data_len-1];
        output_data.get()[data_len-1]=data[i];
    }
    return Handle_Packet(output_data_len,output_data);
}

Handle_Packet encryption_function::micagent_enmap(Handle_Packet packet)
{
    static uint8_t map_char[256];
    static std::once_flag oc_init;
    std::call_once(oc_init,[&](){
        //初始化映射表
        map_char[255]=255;
        for(uint8_t i=0;i<255;i++)map_char[i]=i;
        uint8_t nums[10]={'o','v','J','G','g','K','I','7','b','M'};
        uint8_t lowwer_char_array[26]={'l','6','t','L','n','x','p','Y','U','T','N','5','a','c','A','R','i','d','P','y','h','V','u','W','9','s'};
        uint8_t unpper_char_array[26]={'Z','z','E','S','B','X','C','e','j','Q','1','H','k','4','3','r','8','q','m','D','w','O','F','f','0','2'};
        memcpy(map_char+static_cast<int>('0'),nums,10);
        memcpy(map_char+static_cast<int>('a'),lowwer_char_array,26);
        memcpy(map_char+static_cast<int>('A'),unpper_char_array,26);
    });
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t output_data_len=data_len;
    std::shared_ptr<uint8_t>output_data;
    if(output_data_len>0)output_data.reset(new uint8_t[output_data_len]);
    for(uint32_t i=0;i<data_len;i++)
    {
        output_data.get()[i]=map_char[data[i]];
    }
    return Handle_Packet(output_data_len,output_data);
}
Handle_Packet encryption_function::micagent_demap(Handle_Packet packet)
{
    static uint8_t map_char[256];
    static std::once_flag oc_init;
    std::call_once(oc_init,[&](){
        //初始化映射表
        map_char[255]=255;
        for(uint8_t i=0;i<255;i++)map_char[i]=i;
        uint8_t nums[10]={'Y','K','Z','O','N','l','b','7','Q','y'};
        uint8_t lowwer_char_array[26]={'m','8','n','r','H','X','4','u','q','I','M','a','S','e','0','g','R','P','z','c','w','1','U','f','t','B'};
        uint8_t unpper_char_array[26]={'o','E','G','T','C','W','3','L','6','2','5','d','9','k','V','s','J','p','D','j','i','v','x','F','h','A'};
        memcpy(map_char+static_cast<int>('0'),nums,10);
        memcpy(map_char+static_cast<int>('a'),lowwer_char_array,26);
        memcpy(map_char+static_cast<int>('A'),unpper_char_array,26);
    });
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t output_data_len=data_len;
    std::shared_ptr<uint8_t>output_data;
    if(output_data_len>0)output_data.reset(new uint8_t[output_data_len]);
    for(uint32_t i=0;i<data_len;i++)
    {
        output_data.get()[i]=map_char[data[i]];
    }
    return Handle_Packet(output_data_len,output_data);
}

Handle_Packet encryption_function::micagent_enbasetime(Handle_Packet packet)
{
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    int64_t time_now=GetTimeNow();
    uint8_t high_offset=static_cast<uint8_t>((time_now/1000)%16==0?1:(time_now/1000)%16);
    uint8_t low_offset=static_cast<uint8_t>(((time_now%1000)*1000)%16==0?1:((time_now%1000)*1000)%16);
    uint8_t encryption_byte=0;
    encryption_byte|=low_offset;
    encryption_byte|=(high_offset<<4);
    uint32_t output_data_len=data_len+1;
    std::shared_ptr<uint8_t>output_data;
    output_data.reset(new uint8_t[output_data_len]);
    output_data.get()[data_len]=encryption_byte;
    for(uint32_t i=0;i<data_len;i++)
    {
        uint8_t high_4bit=((data[i]>>4)+high_offset)%16;
        uint8_t low_4bit=((data[i]&0x0f)+low_offset)%16;
        output_data.get()[i]=static_cast<uint8_t>(low_4bit|(high_4bit<<4));
    }
    return Handle_Packet(output_data_len,output_data);
}
Handle_Packet encryption_function::micagent_debasetime(Handle_Packet packet)
{

    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t output_data_len=0;
    std::shared_ptr<uint8_t>output_data;
    do{
        if(data_len<=1)break;
        output_data_len=data_len-1;
        uint8_t encryption_byte=data[output_data_len];
        uint8_t high_offset=encryption_byte>>4;
        uint8_t low_offset=encryption_byte&0x0f;
        output_data.reset(new uint8_t[output_data_len]);
        for(uint32_t i=0;i<output_data_len;i++)
        {
            uint8_t high_4bit=((data[i]>>4)+16-high_offset)%16;
            uint8_t low_4bit=((data[i]&0x0f)+16-low_offset)%16;
            output_data.get()[i]=static_cast<uint8_t>(low_4bit|(high_4bit<<4));
        }
    }while(0);
    return Handle_Packet(output_data_len,output_data);
}

Handle_Packet encryption_function::micagent_enbasepos(Handle_Packet packet)
{
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t output_data_len=data_len;
    std::shared_ptr<uint8_t>output_data;
    if(output_data_len>0)output_data.reset(new uint8_t[output_data_len]);
    uint8_t sum=0;
    for(uint32_t i=0;i<data_len;i++)
    {
        uint8_t offset=(sum+i)%8;
        offset=offset==0?1:offset;
        uint8_t low_bit=data[i]>>(8-offset);
        output_data.get()[i]=(data[i]<<offset)|low_bit;
        output_data.get()[i]^=0x80;//最高位取反
        sum+=data[i];
    }
    return Handle_Packet(output_data_len,output_data);
}
Handle_Packet encryption_function::micagent_debasepos(Handle_Packet packet)
{
    uint8_t *data=packet.second.get();
    uint32_t data_len=packet.first;
    uint32_t output_data_len=data_len;
    std::shared_ptr<uint8_t>output_data;
    if(output_data_len>0)output_data.reset(new uint8_t[output_data_len]);
    uint8_t sum=0;
    for(uint32_t i=0;i<data_len;i++)
    {
        uint8_t tartget_byte=data[i];
        tartget_byte^=0x80;
        uint8_t offset=(sum+i)%8;
        offset=offset==0?1:offset;
        offset=8-offset;
        uint8_t low_bit=tartget_byte>>(8-offset);
        output_data.get()[i]=(tartget_byte<<offset)|low_bit;
        sum+=output_data.get()[i];
    }
    return Handle_Packet(output_data_len,output_data);
}
bool encryption_function::compare_handle_packet(const Handle_Packet&packet1,const Handle_Packet&packet2)
{
    bool ret=false;
    do{
        if(packet1.first!=packet2.first)break;
        ret=true;
        for(uint32_t i=0;i<packet1.first;i++)
        {
            if(packet1.second.get()[i]!=packet2.second.get()[i])
            {
                ret=false;
                break;
            }
        }
    }while(0);
    return  ret;
}
void  encryption_function::dump_uint_8(const uint8_t *buf,uint32_t len,std::string info)
{
    if(!info.empty())std::cout<<info<<":";
    for(uint32_t i =0;i<len;i++)
    {
        if(i!=0)printf(" ");
        printf("%02x",buf[i]);
    }
    printf("\r\n");
}
void encryption_function::diff_print_time(int64_t time1,int64_t time2)
{
    int64_t sec1=time1/1000;
    int64_t microsec1=(time1%1000)*1000;
    int64_t sec2=time2/1000;
    int64_t microsec2=(time2%1000)*1000;
    if(sec1<sec2)
    {
        printf("%lds is gone!\r\n",sec2-sec1);
    }
    else if(sec1>sec2)
    {
        printf("%lds is left!\r\n",sec1-sec2);
    }
    else {
        if(microsec1<microsec2)
        {
            printf("%ldus is gone!\r\n",microsec2-microsec1);
        }
        else if(microsec1>microsec2)
        {
            printf("%ldus is left!\r\n",microsec1-microsec2);
        }
        else {
            printf("time diff is smaller than 1ms or the time is equal!\r\n");
        }
    }

}
std::string &encryption_function::get_description_by_type(EncryptionType type)
{
    static std::string unknown_string="unknown";
    static std::string description[]={
        "orignal",
        "base64",
        "block16",
        "block32",
        "map",
        "basetime",
        "basepos"
    };
    return type>=0&&type<=6?description[static_cast<uint32_t>(type)]:unknown_string;
}
