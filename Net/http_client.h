#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H
#include "dns.h"
#include <string>
#include <map>
#include <mutex>
//需要增加DNS解析服务
class http_client
{
public:
    struct url_cache{
        std::string ip;
        std::string port;
        std::string source_url;
        std::string domain;
    };
    static std::mutex m_mutex;
    static std::map<std::string,url_cache> m_url_cache;
    int Get_Http_Ack(std::string &ack);//阻塞执行 超时5秒
    //设置请求内容，在获取回复前需调用此函数
    void Set_Content(const std::string &content)
    {
        if(content.size()>MAX_CONTENT_LENGTH)return;
        m_content=content;
    }
    void Set_Network_Info(std::string dns_server_ip=std::string("114.114.114.114"),unsigned int timeout=100000,unsigned short dns_port=53)
    {
        m_dns_server_ip=dns_server_ip;
        m_dns_port=dns_port;
        m_timeout=timeout;
    }

//可重入
    static http_client *CreateNew_http_client(std::string &url,bool is_post=true,void (*callback)(void *,const char *,int)=NULL,void *data=NULL);
    static void Delete_http_client(http_client *client){client->~http_client();};
    void print_http_info();
protected:
    http_client(); 
    int connect_to_server();//返回连接套接字
    bool analyze_url(std::string &url);
    ~http_client();
private:
    void pack_http_message(std::string &return_string);//写入http包
private:
    static const unsigned int MAX_CONTENT_LENGTH=65235;
    //取url最大长度128
    static const int MAX_URL_LENGTH=128;
    static const int MAX_HEADR_LENGTH=300;
    unsigned short m_dns_port;
    unsigned int m_timeout;
    std::string m_dns_server_ip;
    void (*m_callback)(void *,const char *,int);
    void *m_data;
    bool m_is_post;
    std::string m_url;//初始url
    std::string m_domain;//域名
    std::string m_content;
    std::string m_host_ip;//原始ip
    std::string m_host_port;//原始端口
    std::string m_source_url;//资源路径
    int m_fd;
};

#endif // HTTP_CLIENT_H
