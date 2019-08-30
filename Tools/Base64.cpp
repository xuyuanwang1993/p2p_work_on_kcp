#include "Base64.h"
#include <mutex>
#include <iostream>
using namespace std;
using namespace sensor_tool;
std::string sensor_tool::base64Encode(const std::string & origin_string)
{
    static const char base64Char[] ="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string ret("");
    if(origin_string.empty()) return ret;
    unsigned const numOrig24BitValues = origin_string.size()/3;
    bool havePadding = origin_string.size() > numOrig24BitValues*3;//判断是否有余数
    bool havePadding2 = origin_string.size() == numOrig24BitValues*3 + 2;//判断余数是否等于2
    unsigned const numResultBytes = 4*(numOrig24BitValues + havePadding);//计算最终结果的size
    ret.resize(numResultBytes);//确定返回字符串大小
    unsigned i;
    for (i = 0; i < numOrig24BitValues; ++i) {
      ret[4*i+0] = base64Char[(origin_string[3*i]>>2)&0x3F];
      ret[4*i+1] = base64Char[(((origin_string[3*i]&0x3)<<4) | (origin_string[3*i+1]>>4))&0x3F];
      ret[4*i+2] = base64Char[((origin_string[3*i+1]<<2) | (origin_string[3*i+2]>>6))&0x3F];
      ret[4*i+3] = base64Char[origin_string[3*i+2]&0x3F];
    }
    //处理不足3位的情况
    //余1位需在后面补齐2个'='
    //余2位需补齐一个'='
    if (havePadding) {
      ret[4*i+0] = base64Char[(origin_string[3*i]>>2)&0x3F];
      if (havePadding2) {
        ret[4*i+1] = base64Char[(((origin_string[3*i]&0x3)<<4) | (origin_string[3*i+1]>>4))&0x3F];
        ret[4*i+2] = base64Char[(origin_string[3*i+1]<<2)&0x3F];
      } else {
        ret[4*i+1] = base64Char[((origin_string[3*i]&0x3)<<4)&0x3F];
        ret[4*i+2] = '=';
      }
      ret[4*i+3] = '=';
    }
    return ret;
}

std::string sensor_tool::base64Decode(const std::string & origin_string)
{
    static char base64DecodeTable[256];
    static std::once_flag oc_init;
    std::call_once(oc_init,[&](){
        int i;//初始化映射表
        for (i = 0; i < 256; ++i) base64DecodeTable[i] = (char)0x80;
        for (i = 'A'; i <= 'Z'; ++i) base64DecodeTable[i] = 0 + (i - 'A');
        for (i = 'a'; i <= 'z'; ++i) base64DecodeTable[i] = 26 + (i - 'a');
        for (i = '0'; i <= '9'; ++i) base64DecodeTable[i] = 52 + (i - '0');
        base64DecodeTable[(unsigned char)'+'] = 62;
        base64DecodeTable[(unsigned char)'/'] = 63;
        base64DecodeTable[(unsigned char)'='] = 0;
    });
    std::string ret("");
    if(origin_string.empty()) return ret;
    int k=0;
    int paddingCount = 0;
    int const jMax = origin_string.size() - 3;
    ret.resize(3*origin_string.size()/4);
    for(int j=0;j<jMax;j+=4)
    {
        char inTmp[4], outTmp[4];
        for (int i = 0; i < 4; ++i) {
          inTmp[i] = origin_string[i+j];
          if (inTmp[i] == '=') ++paddingCount;
          outTmp[i] = base64DecodeTable[(unsigned char)inTmp[i]];
          if ((outTmp[i]&0x80) != 0) outTmp[i] = 0; // this happens only if there was an invalid character; pretend that it was 'A'
        }
        ret[k++]=(outTmp[0]<<2) | (outTmp[1]>>4);
        ret[k++] = (outTmp[1]<<4) | (outTmp[2]>>2);
        ret[k++] = (outTmp[2]<<6) | outTmp[3];
    }
    ret.assign(ret.c_str());//清除空白字符
    return ret;
}
void Base64_test()
{
    std::string qwer("Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, exceeds the short vehemence of any carnal pleasure.");
    std::string qaz=sensor_tool::base64Encode(qwer);
    std::cout<<qaz<<std::endl;
    std::cout<<qwer<<std::endl;
    std::cout<<(int)(sensor_tool::base64Decode(qaz)==qwer)<<std::endl;
}
