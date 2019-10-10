#include "http_client.h"
#include <string.h>
#include <string>
#include <stdlib.h>
#include <errno.h>
#include "Socket.h"
#include "Logger.h"
//using std::string;
http_client::http_client()
{

}
std::mutex http_client::m_mutex;//静态成员初始化
std::map<std::string,http_client::url_cache> http_client::m_url_cache;//静态成员初始化
http_client::~http_client()
{
    if(m_fd>0)
    {
		CloseSocket(m_fd);
    }
}
int http_client::Get_Http_Ack(std::string &ack)//阻塞执行 超时5秒
{
    std::string http_packet;
    int ret=-1;
    do{
        //1.创建http packet
        pack_http_message(http_packet);
        //2.创建tcp连接
        m_fd=connect_to_server();
        if(m_fd<0)break;
        printf("%s \n",http_packet.c_str());
        if(send(m_fd,http_packet.c_str(),http_packet.size(),0)<0)
        {
            LOG_ERROR("send http packet error:%s!",strerror(errno));
            break;
        }
        //使用select阻塞5秒等待接收
        fd_set send_fd;
        FD_ZERO(&send_fd);
        FD_SET(m_fd,&send_fd);
        struct timeval time_out={20,0};
        if(select(m_fd+1,&send_fd,NULL,NULL,&time_out)>0)
        {
            if(FD_ISSET(m_fd,&send_fd))
            {
                int len=MAX_CONTENT_LENGTH;
                char buf[MAX_CONTENT_LENGTH]={0};
                if((len=recv(m_fd,buf,len,0))<=0)
                {
                    LOG_ERROR("recv http response error:%s!",strerror(errno));
                    break;
                }
                else
                {
                    ack=buf;
                    if(this->m_callback)
                    {//处理回调函数
                        m_callback(m_data,ack.c_str(),len);
                    }
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            LOG_ERROR("select error or time out:%s!",strerror(errno));
            break;
        }
        ret=0;
    }while(0);
    if(m_fd>0) {
		CloseSocket(m_fd);
        m_fd=-1;
    }
    return ret;
}

http_client *http_client::CreateNew_http_client(std::string &url,bool is_post,void (*callback)(void *,const char *,int),void *data)
{
    http_client *http_instance=new http_client();
    if(NULL==http_instance)
    {
        LOG_ERROR("New http_client error!");
        return NULL;
    }
    http_instance->Set_Network_Info();
    if(!http_instance->analyze_url(url))
    {
        LOG_ERROR("false url format!");
        delete http_instance;
        return NULL;
    }
    http_instance->m_url=url;
    http_instance->m_fd=-1;
    http_instance->m_callback=callback;
    http_instance->m_data=data;
    http_instance->m_is_post=is_post;
    http_instance->m_host_port="80";
    return http_instance;
}
int http_client::connect_to_server()//返回连接套接字
{
    int fd=-1;
    bool set=false;
    do{
        fd=socket(AF_INET,SOCK_STREAM,0);
        if(fd<0)break;
        //设置发送接收超时
        struct timeval tv={10,0};
        if(setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,(char *)&tv,sizeof(struct timeval))<0)
        {
            perror("set SO_SNDTIMEO error !");
            break;
        }
        if(setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,(char *)&tv,sizeof(struct timeval))<0)
        {
            perror("set SO_RCVTIMEO error !");
            break;
        }
        //设置发送最大缓冲
        int send_buf_size=MAX_HEADR_LENGTH+MAX_CONTENT_LENGTH;
        if(setsockopt(fd,SOL_SOCKET,SO_SNDBUF,(char *)&send_buf_size,sizeof(send_buf_size))<0)
        {
            perror("set SO_SNDBUF error !");
            break;
        }
        set=true;
    }while(0);
    if(!set)
    {
        LOG_ERROR("set socket error! %s",strerror(errno));
        if(fd>0)
        {
			CloseSocket(fd);
        }
        return -1;
    }
    sockaddr_in remote;
    remote.sin_addr.s_addr = inet_addr(m_host_ip.c_str());
    remote.sin_family = AF_INET;
    remote.sin_port = htons(atoi(m_host_port.c_str()));
    if(connect(fd,(sockaddr *)&remote,sizeof(remote))!=0)
    {
        http_client::m_url_cache.erase(m_url);
        LOG_ERROR("connect to %s %d error! %s",m_host_ip.c_str(),atoi(m_host_port.c_str()),strerror(errno));
		CloseSocket(fd);
        fd=-1;
    }
    return fd;
}
//只接受http://ip:port+source_url格式的输入,此处不检查ip合法性
bool http_client::analyze_url(std::string &url)
{
    bool ret =false;
    do{
        {
            std::lock_guard<std::mutex> locker(http_client::m_mutex);
            auto iter=http_client::m_url_cache.find(url);
            if(iter!=std::end(http_client::m_url_cache))
            {
                ret=true;
                url_cache cache=iter->second;
                m_host_ip=cache.ip;
                m_host_port=cache.port;
                m_source_url=cache.source_url;
                m_domain=cache.domain;
                break;
            }
        }
        int start_pos=url.find("//",0);
        if(start_pos<0)
        {
            start_pos=0;
        }
        int end_pos=url.find('/',start_pos+2);
        if(end_pos<0)
        {
            end_pos=url.length();
        }
        else
        {
            m_source_url=url.substr(end_pos,url.size()-end_pos);
        }
        m_domain=url.substr(start_pos+2,end_pos-start_pos-2);
        char tmp[20]={0};
        DNS_Resolve(m_domain.c_str(),tmp,m_dns_server_ip.c_str(),m_dns_port,m_timeout);
        if(strlen(tmp)>0)
        {
            m_host_ip=tmp;
        }
        else
        {
            end_pos=m_domain.find(":",0);
            if(end_pos==(-1))
            {
                m_host_ip=m_domain;
            }
            else
            {
                m_host_ip=m_domain.substr(0,end_pos);
                m_host_port=m_domain.substr(end_pos+1,m_domain.size()-end_pos-1);
            }
        }
        url_cache cache;
        cache.ip=m_host_ip;
        cache.port=m_host_port;
        cache.source_url=m_source_url;
        cache.domain=m_domain;
        std::lock_guard<std::mutex> locker(http_client::m_mutex);
        http_client::m_url_cache.emplace(url,cache);
        ret=true;
    }while(0);
    return ret;
}
void http_client::pack_http_message(std::string &return_string)
{
    if(m_is_post)
    {
        return_string="POST "+m_source_url+" HTTP/1.1\r\n";
        if(m_domain.empty())
        {
            return_string+="Host: "+m_host_ip+":"+m_host_port+"\r\n";
        }
        else
        {
            return_string+="Host: "+m_domain+"\r\n";
        }
        char len[10]={0};
        sprintf(len,"%d",m_content.length());
        return_string=return_string+"Content-Length: "+len+"\r\n";
        return_string+="Connection: keep-alive\r\n";
        return_string+="Cache-Control: no-cache\r\n";
        //application/x-www-form-urlencoded
       // return_string+="Content-Type: application/json\r\n";
        return_string+="Content-Type: application/x-www-form-urlencoded\r\n";
        return_string+="Accept: application/json\r\n\r\n";
        return_string+=m_content+"\r\n";
    }
    else
    {
        return_string="GET "+m_source_url+"?"+m_content+" HTTP/1.1\r\n";
        if(m_domain.empty())
        {
            return_string+="Host: "+m_host_ip+":"+m_host_port+"\r\n";
        }
        else
        {
            return_string+="Host: "+m_domain+"\r\n";
        }
        return_string+="Connection: keep-alive\r\n";
        return_string+="Cache-Control: no-cache\r\n";
        return_string+="Accept: application/json\r\n\r\n";
    }
}
//https://www.jianshu.com/p/f4562629c1a5
//POST source_url HTTP/1.1\r\n
//Host: ip:port\r\n  //服务器域名
//Connection: close\r\n //如果不复用套接字应将此项设为close
//Content-Length: 16\r\n //Transfer-Encoding: chunked 代替后需在数据块前+2字节的数据块长度
//Pragma: no-cache\r\n
//Cache-Control: no-cache\r\n
//Content-Type: text/plain\r\n //application/x-www-form-urlencoded
//Accept: text/plain\r\n
//device_id=name&log_file=......\r\n
//\r\n
void http_client::print_http_info()
{
    char url[256]="";
    sprintf(url,"http://%s:%s%s",this->m_host_ip.c_str(),this->m_host_port.c_str(),this->m_source_url.c_str());
    printf("request url is %s  \r\n",url);
    printf("host_ip is %s \r\n",m_host_ip.c_str());
    printf("host_port is %s \r\n",m_host_port.c_str());
    printf("source_url is %s\r\n",m_source_url.c_str());
}
