#include "parase_request.h"
#include <cstring>
#include <assert.h>
std::map<std::string,std::string> message_handle::parse_buf(std::string &buf)
{
    char key[128]={0};
    char value[256]={0};
    char check_key[128]={0};
    int offset=0;
    int next_search_pos=0;
    int len=buf.size();
    std::map<std::string,std::string> ret;
    while(offset<len&&(next_search_pos=buf.find("\r\n",offset))>offset+4)
    {
        std::string tmp=buf.substr(offset,next_search_pos-offset);
        offset=next_search_pos+2;
        if(sscanf(tmp.c_str(),"<%[^>]>%[^<]<%[^>]>",key,value,check_key)!=3)goto parse_buf_fail;
        if(check_key[0]!='/'||strcmp(key,check_key+1)!=0)goto parse_buf_fail;
        ret.insert(std::pair<std::string,std::string>(std::string(key),std::string(value)));
    }
    return ret;
parse_buf_fail:
    std::cout<<"please check the input!"<<std::endl;
    std::cout<<buf;
    ret.clear();
    return ret;
}
std::map<std::string,std::string> message_handle::parse_buf(const char *c_buf)
{
    assert(c_buf!=NULL);
    std::string buf=c_buf;
    char key[128]={0};
    char value[256]={0};
    char check_key[128]={0};
    int offset=0;
    int next_search_pos=0;
    int len=buf.size();
    std::map<std::string,std::string> ret;
    while(offset<len&&(next_search_pos=buf.find("\r\n",offset))>offset+4)
    {
        std::string tmp=buf.substr(offset,next_search_pos-offset);
        offset=next_search_pos+2;
        if(sscanf(tmp.c_str(),"<%[^>]>%[^<]<%[^>]>",key,value,check_key)!=3)goto parse_buf_fail;
        if(check_key[0]!='/'||strcmp(key,check_key+1)!=0)goto parse_buf_fail;
        ret.insert(std::pair<std::string,std::string>(std::string(key),std::string(value)));
    }
    return ret;
parse_buf_fail:
    std::cout<<"please check the input!"<<std::endl;
    std::cout<<buf;
    ret.clear();
    return ret;
}
void message_handle::packet_buf(std::string &buf,std::string key,std::string value)
{
    assert(!key.empty()&&!value.empty());
    buf+="<";
    buf+=key;
    buf+=">";
    buf+=value;
    buf+="</";
    buf+=key;
    buf+=">\r\n";
}
