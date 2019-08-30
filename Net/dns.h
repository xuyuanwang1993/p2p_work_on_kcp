#ifndef MYDNS_H_
#define MYDNS_H_
#ifdef __cplusplus
extern "C"
{
#endif
/**
  @return : 0:success -1:send error -2:recv error
  @param domain:待解析的域名
  @param return_ip:存储返回值
  @param dns_server_ip:dns服务器ip，可用本地网关
  @param dns_port:dns服务端口，默认53
  @param micro_timeout:接收等待时间，单位微秒
**/
int DNS_Resolve(const char *domain,char *return_ip,const char * dns_server_ip, unsigned short dns_port,unsigned int micro_timeout);
#ifdef __cplusplus
}
#endif
#endif /* MYDNS_H_ */
