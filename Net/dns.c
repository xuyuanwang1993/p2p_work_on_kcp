#include "dns.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#define parse(__w,__p) do{*__p = *__w << 8;__w ++;*__p |= *__w;__w ++;}while(0)
#ifndef uint8_t
#define uint8_t unsigned char
#endif
#ifndef uint16_t
#define uint16_t unsigned short
#endif
#ifndef uint32_t
#define uint32_t unsigned int
#endif
#define debug_info(fmt, ... ) \
        do{if(1){Nowtime();printf("[%s][%d]: " fmt "\n",__func__,__LINE__,##__VA_ARGS__);}}while(0);
typedef struct _DNSInfo
{
        uint8_t  	Domain[30];//域名
        uint32_t 	Domainlen;//域名长度
        uint32_t 	sockfd;//通讯udp套接字
        uint16_t 	randid;
        char *          domain_ip;//返回的ip需手动传入存储地址
        unsigned int    micro_timeout;//超时时间
        char            dns_server_ip[20];//dns服务器地址
        unsigned short  dns_port;//default :53
}DNSInfo;
static void Nowtime()
{
        struct timeval tv;
        gettimeofday(&tv,NULL);
        struct tm *p;
        p = gmtime((time_t *)&tv.tv_sec);
        printf("[%02d:%02d:%02d.%06ld] ",(p->tm_hour+8)%24, p->tm_min, p->tm_sec, tv.tv_usec);
}

static uint32_t UdpOpenNoBlock()
{
        uint32_t ret = -1;
        uint32_t sockfd;
        uint32_t flags;
        sockfd = socket(AF_INET,SOCK_DGRAM,0);
        if((int)sockfd < 0)
        {
                debug_info("socker() error");
                return ret;
        }
        flags = fcntl(sockfd,F_GETFL,0);
        flags |= O_NONBLOCK;
        if(fcntl(sockfd,F_SETFL,flags) < 0)
        {
                debug_info("fcntl() error");
                return ret;
        }
        ret = sockfd;
        return ret;
}
static uint32_t UdpSend(uint32_t sockfd,uint8_t *buffer,uint32_t len,uint8_t *Domain,uint32_t Port)
{
        uint32_t sendlen = -1;
        struct sockaddr_in sockaddr;
        bzero(&sockaddr,sizeof(sockaddr));
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons(Port);
        //IP格式转换
        inet_pton(AF_INET,(char *)Domain,&sockaddr.sin_addr);

        sendlen = sendto(sockfd,buffer,len,0,(struct sockaddr *)&sockaddr,sizeof(sockaddr));
        if(sendlen <= 0)
                debug_info("sendto error");

        return sendlen;
}

/* = 0 no data to recv
 * > 0 return recvlen
 * < 0 sockfd error or timeout must reboot socket 	 */
static uint32_t UdpRecv(uint32_t sockfd,uint8_t *buffer,uint32_t len,uint32_t mstimeout)
{
        uint32_t recvlen = 0;
        struct sockaddr_in sockaddr;
        bzero(&sockaddr,sizeof(sockaddr));
        socklen_t socklen = sizeof(sockaddr);
        fd_set readfd;
        struct timeval tv;
        tv.tv_sec = mstimeout / 1000;
        tv.tv_usec = mstimeout % 1000 * 1000;

        FD_ZERO(&readfd);
        FD_SET(sockfd,&readfd);

        if(select(sockfd + 1,&readfd,NULL,NULL,&tv) > 0)
        {
                if(FD_ISSET(sockfd,&readfd))
                {
                        recvlen = recvfrom(sockfd,buffer,len,0,(struct sockaddr *)&sockaddr,&socklen);
                        if(recvlen > 0)
                                return recvlen;
                        if((int)recvlen < 0)
                                debug_info("recvfrom error");
                }
        }
        if(recvlen == 0)
                debug_info("There is no data to read");
        return recvlen;
}

static uint32_t Package_Sendbuf(uint8_t *Domain,DNSInfo *dns,uint8_t *buffer)
{
        DNSInfo *dns_info = dns;
        uint32_t i,j = 0,Position = 0;

        uint16_t *randbuff = NULL;
        randbuff = (uint16_t *)buffer;

        strcpy((char *)dns_info->Domain,(char *)Domain);
        dns_info->Domainlen = strlen((char *)dns_info->Domain);

        randbuff[0] = rand();						//ID
        dns_info->randid = randbuff[0];
//	debug_info("randid = %#x %#x",dns_info->randid,htons(dns_info->randid));

        Position ++;
        Position ++;
        buffer[Position++] = 0x01;					//Flags
        buffer[Position++] = 0x00;					//Flags 			0x0100

        buffer[Position++] = 0x00;					//Questions;
        buffer[Position++] = 0x01;					//Questions;		0x0001

        buffer[Position++] = 0x00;					//Answer RRs;
        buffer[Position++] = 0x00;					//Answer RRs;		0x0000

        buffer[Position++] = 0x00;					//Authority RRs;
        buffer[Position++] = 0x00;					//Authority RRs;	0x0000

        buffer[Position++] = 0x00;					//Additional RRs;
        buffer[Position++] = 0x00;					//Additional RRs;	0x0000

        /***************************** Queries *******************************/
        for(i = 0;i < dns_info->Domainlen; i ++)
        {
                if(dns_info->Domain[i] == '.')
                {
                        buffer[Position] = j;
                        Position += j + 1;
                        j = 0;
                }
                else
                {
                        buffer[12 + i + 1] = Domain[i];
                        j++;
                }
        }
        buffer[Position] = j;
        Position += j + 1;
        buffer[Position++] = 0x00;					//end Queries

        buffer[Position++] = 0x00;					//Type
        buffer[Position++] = 0x01;					//Type

        buffer[Position++] = 0x00;					//Class
        buffer[Position++] = 0x01;					//Class
        return Position;
}

static uint32_t DNSSend(uint8_t *Domain,DNSInfo *dns)
{
        DNSInfo *dns_info = dns;
        uint32_t ret = 0;
        uint32_t sendlen,buflen;
        uint8_t buffer[256];

        dns_info->sockfd = UdpOpenNoBlock();
        if((int)dns->sockfd < 0)
                return ret;

        buflen = Package_Sendbuf(Domain,dns_info,buffer);

        sendlen = UdpSend(dns_info->sockfd,buffer,buflen,(unsigned char *)dns_info->dns_server_ip,dns_info->dns_port);
//	debug_info("sendlen = %d",sendlen);
        if(sendlen != buflen)
        {
            return ret;
        }
        return sendlen;
}

static uint32_t parseNAME(uint8_t *buffer,uint8_t *Domain)
{
        uint32_t i = 0,num = 0,a = 0;
        uint8_t j = *buffer;
        do{
                for(i = 0;i < j;i++)
                {
                        buffer ++;num++;
                        Domain[a] = *buffer;
                        a++;
                }
                Domain[a] = '.';
                a++;
                buffer ++;num++;
                j = *buffer;
        }while(j != 0);
        num += 5;
        return num;
}

static uint32_t Parse_Rcvbuf(DNSInfo *dns,uint8_t *buffer)
{
        DNSInfo *dns_info = dns;

        uint32_t transID,ret = 0;
        parse(buffer,&transID);
//	debug_info("transID = %#x",transID);
        if(transID != ntohs(dns_info->randid))
        {
                debug_info("recvbuf error");
                return ret;
        }
        else
        {
                uint32_t Flags;
                parse(buffer,&Flags);
                //debug_info("Flags = %#x",Flags);

                uint32_t Question;
                parse(buffer,&Question);
                //debug_info("Question = %#x",Question);

                uint32_t AnserRRs;
                parse(buffer,&AnserRRs);
                //debug_info("AnserRRs = %#x",AnserRRs);

                uint32_t Authority;
                parse(buffer,&Authority);
                //debug_info("AdditionalRRs = %#x",Authority);

                uint32_t AdditionalRRs;
                parse(buffer,&AdditionalRRs);
                //debug_info("AdditionalRRs = %#x",AdditionalRRs);

                uint8_t Domain[256] = {0};
                buffer += parseNAME(buffer,Domain);
                debug_info("Domain = %s ",Domain);

                //point to name
                while(*buffer == 0xc0)
                {
                        buffer += 2; 				//->type;
                        buffer += 2;				//->Class;
                        buffer += 2;				//->TTL;
                        buffer += 4;				//->length
                        buffer += 1;				//->little
//			debug_info("%#x",*buffer);
                        if(*buffer != 4)			//not ip;
                        {
                                buffer += *buffer + 1;
                        }
                        else						//is ip
                        {
                                struct in_addr *netip;
                                buffer += 1;
                                netip = (struct in_addr *)buffer;
                                buffer += 4;
                                strcpy(dns_info->domain_ip,inet_ntoa(*netip));
                                //debug_info("ip = %s",dns_info->domain_ip);
                                break;
                        }
                }
        }
        ret = 1;
        return ret;
}

static uint32_t DNSRecv(DNSInfo *dns)
{
        uint8_t recvbuf[512] = {0};
        uint32_t recvlen,ret = 0;
        DNSInfo *dns_info = dns;
        recvlen = UdpRecv(dns_info->sockfd,recvbuf,512,100);
        if(recvlen <= 0)
                return ret;

        /**************************Parse recvbuf********************************/

        if(Parse_Rcvbuf(dns_info,recvbuf) == 0)
                return ret ;
        ret = 1;
        return ret;
}
int DNS_Resolve(const char *domain,char *return_ip,const char *dns_server_ip,unsigned short dns_port,unsigned int micro_timeout)
{
    assert(return_ip);
    assert(dns_server_ip);
    assert(domain);
    assert(dns_port!=0);
    DNSInfo *dnscheck=(DNSInfo *)calloc(1,sizeof(DNSInfo));
    assert(dnscheck);
    dnscheck->micro_timeout=micro_timeout;
    dnscheck->domain_ip=return_ip;
    dnscheck->dns_port=dns_port;
    strcpy(dnscheck->dns_server_ip,dns_server_ip);
    int ret=0;
    do{
        if(DNSSend((unsigned char *)domain,dnscheck)<=0)
        {
            printf("DNSSend() error!\n");
            ret=-1;
            break;
        }
        if(DNSRecv(dnscheck) == 0)
        {
            printf("DNSRecv() error!\n");
            ret=-2;
            break;
        }
        printf("DNS_Resolve() success!\r\n");
    }while(0);
    if(dnscheck->sockfd!=0)close(dnscheck->sockfd);
    free(dnscheck);
    return ret;
}

