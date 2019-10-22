#include "upnpmapper.h"
#include "TcpSocket.h"
#include "NetInterface.h"
using  namespace upnp;
using  std::cout;
using  std::endl;
Upnp_Connection::Upnp_Connection(UpnpMapper * server,xop::TaskScheduler *taskScheduler, int sockfd,UPNP_COMMAND mode)
    :TcpConnection(taskScheduler, sockfd),m_upnp_mapper(server),m_taskscheduler(taskScheduler),m_mode(mode)
{
    this->setReadCallback([this](std::shared_ptr<TcpConnection> conn, xop::BufferReader& buffer) {
        return this->onRead(buffer);
    });
    this->setCloseCallback([this](std::shared_ptr<TcpConnection> conn) {
        this->onClose();
    });
}
Upnp_Connection::~Upnp_Connection()
{

}
#define SOAPPREFIX "s"
#define SERVICEPREFIX "u"
string Upnp_Connection::build_xml_packet(string action,std::vector<std::pair<string ,string>>args)
{

    string packet="<?xml version=\"1.0\"?>\r\n";
    packet+="<" SOAPPREFIX ":Envelope\r\n";
    packet+="    xmlns:" SOAPPREFIX "=\"http://schemas.xmlsoap.org/soap/envelope/\"\r\n    ";
    packet+=SOAPPREFIX ":encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n  ";
    packet+="<" SOAPPREFIX ":Body>\r\n    ";
    packet+="<" SERVICEPREFIX ":"+action+" xmlns:" SERVICEPREFIX "=\""+m_upnp_mapper->m_control_string+"\">\r\n";
    if(!args.empty())
        for(auto i : args)
        {
            packet+="      <"+i.first+">"+i.second+"</"+i.first+">\r\n";
        }
    packet+="    </" SERVICEPREFIX ":"+action+">\r\n";
    packet+="  </" SOAPPREFIX ":Body>\r\n</" SOAPPREFIX ":Envelope>\r\n";
    packet+="\r\n";
    string request="POST ";
    request=request+m_upnp_mapper->m_control_url+" HTTP/1.1\r\n";
    request=request+"Host: "+m_upnp_mapper->m_lgd_ip+":"+std::to_string(m_upnp_mapper->m_lgd_port)+"\r\n";
    request=request+"SOAPAction: \""+m_upnp_mapper->m_control_string+"#"+action+"\"\r\n";
    request=request+"User-Agent: micagent_upnp_tool\r\n";
    request=request+"Content-Type: text/xml; charset=\"utf-8\"\r\n";
    request=request+"Connection: close\r\n";
    request=request+"Content-Length: "+std::to_string(packet.length())+"\r\n";
    request+="\r\n";
    request+=packet;
    return request;
}
bool Upnp_Connection::onRead(xop::BufferReader& buffer)
{
    std::string recv_data;
    int size = buffer.readAll(recv_data);
    if(size<=0)return false;
    else {
        m_buf+=recv_data;
        if(check_packet())HandleData();
    }
    return true;
}
void Upnp_Connection::onClose()
{
    cout<<"close Upnp_Connection"<<endl;
}
bool Upnp_Connection::check_packet()
{
    cout<<"############"<<endl;
    cout<<m_buf<<endl;
    cout<<"****************"<<endl;
    string key_body="\r\n\r\n";
    string key_length="Content-Length";
    bool ret=false;
    do{
        auto pos=m_buf.find(key_body);
        if(pos==string::npos)break;
        auto pos2=m_buf.find(key_length);
        if(pos2==string::npos)break;
        char len[16]={0};

        if(sscanf(m_buf.c_str()+pos2,"%*[^:]: %[^\r\n]",len)!=1)break;
        if(m_buf.length()-std::stoi(len)-pos-key_body.length()!=0)break;
        ret=true;
    }while(0);
    return ret;
}
void Upnp_Connection::send_get_wanip()
{
    std::vector<std::pair<string ,string>>args;
    std::string request=this->build_xml_packet("GetExternalIPAddress",args);
    this->send(request.c_str(),request.length());
}
void Upnp_Connection::send_add_port_mapper(SOCKET_TYPE type,string internal_ip,int internal_port,int external_port,string description)
{
    std::vector<std::pair<string ,string>>args;
    args.emplace_back(std::pair<string,string>("NewRemoteHost",string()));
    args.emplace_back(std::pair<string,string>("NewExternalPort",std::to_string(external_port)));
    args.emplace_back(std::pair<string,string>("NewProtocol",type==SOCKET_TYPE::SOCKET_TCP?"TCP":"UDP"));
    args.emplace_back(std::pair<string,string>("NewInternalPort",std::to_string(internal_port)));
    args.emplace_back(std::pair<string,string>("NewInternalClient",internal_ip));
    args.emplace_back(std::pair<string,string>("NewEnabled","1"));
    args.emplace_back(std::pair<string,string>("NewPortMappingDescription",description));
    args.emplace_back(std::pair<string,string>("NewLeaseDuration",type==SOCKET_TYPE::SOCKET_TCP?"0":"20"));
    std::string request=this->build_xml_packet("AddPortMapping",args);
    this->send(request.c_str(),request.length());
}
void Upnp_Connection::send_get_specific_port_mapping_entry(SOCKET_TYPE type,int external_port)
{
    std::vector<std::pair<string ,string>>args;
    args.emplace_back(std::pair<string,string>("NewRemoteHost",string()));
    args.emplace_back(std::pair<string,string>("NewExternalPort",std::to_string(external_port)));
    args.emplace_back(std::pair<string,string>("NewProtocol",type==SOCKET_TYPE::SOCKET_TCP?"TCP":"UDP"));
    args.emplace_back(std::pair<string,string>("NewInternalPort",string()));
    args.emplace_back(std::pair<string,string>("NewInternalClient",string()));
    args.emplace_back(std::pair<string,string>("NewEnabled",string()));
    args.emplace_back(std::pair<string,string>("NewPortMappingDescription",string()));
    args.emplace_back(std::pair<string,string>("NewLeaseDuration",string()));
    std::string request=this->build_xml_packet("GetSpecificPortMappingEntry",args);
    this->send(request.c_str(),request.length());
}
void Upnp_Connection::send_delete_port_mapper(SOCKET_TYPE type,int external_port)
{
    std::vector<std::pair<string ,string>>args;
    args.emplace_back(std::pair<string,string>("NewRemoteHost",string()));
    args.emplace_back(std::pair<string,string>("NewExternalPort",std::to_string(external_port)));
    args.emplace_back(std::pair<string,string>("NewProtocol",type==SOCKET_TYPE::SOCKET_TCP?"TCP":"UDP"));
    std::string request=this->build_xml_packet("DeletePortMapping",args);
    this->send(request.c_str(),request.length());
}
void Upnp_Connection::send_get_control_url()
{
    string request="GET ";
    request=request+m_upnp_mapper->m_location_src+" HTTP/1.1\r\n";
     request=request+"Host: "+m_upnp_mapper->m_lgd_ip+":"+std::to_string(m_upnp_mapper->m_lgd_port)+"\r\n";
    request=request+"User-Agent: micagent_upnp_tool\r\n";
    request=request+"Accept: application/xml\r\n";
    request=request+"Connection: keep-alive\r\n";
    request+="\r\n";
    this->send(request.c_str(),request.length());
}
void Upnp_Connection::handle_get_wanip()
{
    if(m_buf.empty())return;
    char status[5]={0};
    char info[128]={0};
    do{
        if(sscanf(m_buf.c_str(),"%*s %s %[^\r\n]",status,info)!=2)
        {
            cout<<"false http response! : "<<m_buf<<endl;
            break;
        }
        if(strcmp(status,"200")!=0)
        {
            cout<<"can't found externalIPAddress! errorcode :"<<status<<" info:"<<info<<endl;
            break;
        }
        string key="NewExternalIPAddress";
        auto pos=m_buf.find(key);
        if(pos==string::npos)
        {
            cout<<"false http response! : "<<m_buf<<endl;
            break;
        }
        char externalIP[32]={0};
        if(sscanf(m_buf.c_str()+pos,"%*[^>]>%[^<]",externalIP)!=1)
        {
            cout<<"false http response! : "<<m_buf<<endl;
            break;
        }
        cout<<"NewExternalIPAddress : "<<externalIP<<endl;
        this->m_upnp_mapper->m_wan_ip=externalIP;
    }while(0);
}
void Upnp_Connection::handle_add_port_mapper()
{
    if(m_buf.empty())return;
    char status[5]={0};
    char info[128]={0};
    bool b_success=false;
    do{
        if(sscanf(m_buf.c_str(),"%*s %s %[^\r\n]",status,info)!=2)
        {
            cout<<"false http response! : "<<m_buf<<endl;
            break;
        }
        if(strcmp(status,"200")!=0)
        {
            cout<<"can't addportmapping! errorcode :"<<status<<" info:"<<info<<endl;
            break;
        }
        cout<<"add port mapping success!"<<endl;
        b_success=true;
    }while(0);
    if(m_callback)m_callback(b_success);
}
void Upnp_Connection::handle_get_specific_port_mapping_entry()
{
    if(m_buf.empty())return;
    char status[5]={0};
    char info[128]={0};
    bool b_success=false;
    do{
        if(sscanf(m_buf.c_str(),"%*s %s %[^\r\n]",status,info)!=2)
        {
            cout<<"false http response! : "<<m_buf<<endl;
            break;
        }
        if(strcmp(status,"200")!=0)
        {
            cout<<"can't get port mapping entry! errorcode :"<<status<<" info:"<<info<<endl;
            break;
        }
        string key="NewInternalClient";
        auto pos=m_buf.find(key);
        if(pos==string::npos)
        {
            cout<<"false http response! : "<<m_buf<<endl;
            break;
        }
        char internalIP[32]={0};
        if(sscanf(m_buf.c_str()+pos,"%*[^>]>%[^<]",internalIP)!=1)
        {
            cout<<"false http response! : "<<m_buf<<endl;
            break;
        }
        if(xop::NetInterface::getLocalIPAddress()!=internalIP)break;
        cout<<"get port mapping entry success!"<<endl;
        b_success=true;
    }while(0);
    if(m_callback)m_callback(b_success);
}
void Upnp_Connection:: handle_delete_port_mapper()
{
    if(m_buf.empty())return;
    char status[5]={0};
    char info[128]={0};
    bool b_success=false;
    do{
        if(sscanf(m_buf.c_str(),"%*s %s %[^\r\n]",status,info)!=2)
        {
            cout<<"false http response! : "<<m_buf<<endl;
            break;
        }
        char errorinfo[128]="OK";
        if(strcmp(status,"200")!=0)
        {
            cout<<"can't deleteportmapping! errorcode :"<<status<<" info:"<<info<<endl;
            string key="errorDescription";
            auto pos=m_buf.find(key);
            if(pos==string::npos)
            {
                cout<<"false http response! : "<<m_buf<<endl;
                break;
            }
            if(sscanf(m_buf.c_str()+pos,"%*[^>]>%[^<]",errorinfo)!=1)
            {
                cout<<"false http response! : "<<m_buf<<endl;
                break;
            }
        }
        cout<<"delete port mapping success!"<<"(error info :"<<errorinfo<<")"<<endl;
        b_success=true;
    }while(0);
    if(m_callback)m_callback(b_success);
}
void Upnp_Connection::handle_get_control_url()
{
    string key_string1="<service>";
    string key_string2="</service>";
    string key_string3="urn:schemas-upnp-org:service:WANPPPConnection:1";
    string key_string4="urn:schemas-upnp-org:service:WANIPConnection:1";
    string key_string5="<controlURL>";
    int start_pos=0;
    while(start_pos<m_buf.length())
    {
        //find service
        int pos1=m_buf.find(key_string1,start_pos);
        if(pos1==string::npos)break;
        //find service's end;
        int pos2=m_buf.find(key_string2,pos1);
        if(pos2==string::npos)break;
        string tmp=m_buf.substr(pos1,pos2-pos1);
        if(tmp.find(key_string3,0)!=string::npos)
        {
            this->m_upnp_mapper->m_control_string=key_string3;
        }
        else if(tmp.find(key_string4,0)!=string::npos)
        {
            this->m_upnp_mapper->m_control_string=key_string4;
        }
        else {
            //not match
            cout<<endl<<tmp<<endl;
            cout<<"not match"<<endl;
            start_pos=pos2+key_string2.length();
            continue;
        }
        int pos3=tmp.find(key_string5);
        if(pos3==string::npos)break;
        char t_control_url[256]={0};
        if(sscanf(tmp.c_str()+pos3,"%*[^>]>%[^<]",t_control_url)!=1)
        {
            cout<<"false xml file : "<<tmp.c_str()+pos3<<endl;
            break;
        }
        this->m_upnp_mapper->m_control_url=t_control_url;
        cout<<"control url :"<<this->m_upnp_mapper->m_control_url<<endl;
        this->m_upnp_mapper->m_init_ok=true;
        this->reset_mode(UPNP_GETEXTERNALIPADDRESS);
        this->send_get_wanip();
        break;
    }
}
void Upnp_Connection::HandleData()
{
    switch(m_mode)
    {
    case UPNP_GETCONTROLURL:
        handle_get_control_url();
    case UPNP_GETEXTERNALIPADDRESS:
        handle_get_wanip();
        break;
    case UPNP_ADDPORTMAPPING:
        handle_add_port_mapper();
        break;
    case UPNP_GETSPECIFICPORTMAPPINGENTRY:
        handle_get_specific_port_mapping_entry();
        break;
    case UPNP_DELETEPORTMAPPING:
        handle_delete_port_mapper();
        break;
    default:
        break;
    }
}
void UpnpMapper::Init(xop::EventLoop *event_loop,string lgd_ip)
{
    m_control_url=string();
    m_event_loop=event_loop;
    m_lgd_ip=lgd_ip;
    m_lgd_port=0;
    m_location_src=string();
    m_wan_ip=string();
    m_init_ok=false;
    int fd=socket(AF_INET,SOCK_DGRAM,0);
    m_udp_channel.reset(new xop::Channel(fd));
    m_udp_channel->enableReading();
    m_udp_channel->setReadCallback([this](){
        struct sockaddr_in addr = {0};
        socklen_t addr_len;
        char buf[4096]={0};
        int len=recvfrom(this->m_udp_channel->fd(),buf,4096,0,(struct sockaddr *)&addr,(socklen_t *)&addr_len);
        if(len>0)
        {
            if(m_lgd_ip==inet_ntoa(addr.sin_addr))
            {
                this->m_event_loop->removeChannel(this->m_udp_channel);
                cout<<"find upnp device!"<<endl;
                cout<<buf<<endl;
                string tmp=buf;
                int match_pos=tmp.find("LOCATION",0);
                if(match_pos==string::npos)match_pos=tmp.find("location",0);
                do{
                    if(match_pos==string::npos)
                    {
                        cout<<"can't find location url!"<<endl;
                        break;
                    }
                    char igd_port[32]={0};
                    char location_src[256]={0};
                    if(sscanf(tmp.c_str()+match_pos,"%*[^/]//%*[^:]:%[^/]%[^\r\n]",igd_port,location_src)!=2)break;
                    m_lgd_port=std::stoi(igd_port);
                    m_location_src=location_src;
                    std::thread t(std::bind(&UpnpMapper::get_control_url,this));
                    t.detach();
                    return ;
                }while(0);
                cout<<"false upnp device!"<<endl;
            }
            else {
                cout<<"recvfrom  ip:"<<inet_ntoa(addr.sin_addr)<<" port : "<<ntohs(addr.sin_port)<<endl<<buf<<endl;
            }
        }
    });
    event_loop->updateChannel(m_udp_channel);
    send_discover_packet();
    event_loop->addTimer([this](){
        if(this->m_location_src!=string()&&this->m_control_url!=string())
        {
            this->m_event_loop->removeChannel(m_udp_channel);
            m_udp_channel.reset();
            return false;
        }
        send_discover_packet();
        cout<<"no upnp suppot!"<<endl;
        return true;
    },MAX_WAIT_TIME*2);
}
void UpnpMapper:: Api_addportMapper(SOCKET_TYPE type,string internal_ip,int internal_port,int external_port,string description,UPNPCALLBACK callback)
{
    if(!m_init_ok||m_control_url==string())return;
    std::thread t(std::bind(&UpnpMapper::add_port_mapper,this,type,internal_ip,internal_port,external_port,description,callback));
    t.detach();
}
void UpnpMapper:: Api_GetSpecificPortMappingEntry(SOCKET_TYPE type,int external_port,UPNPCALLBACK callback)
{
    if(!m_init_ok||m_control_url==string())return;
    std::thread t(std::bind(&UpnpMapper::get_specific_port_mapping_entry,this,type,external_port,callback));
    t.detach();
}
void UpnpMapper::Api_deleteportMapper(SOCKET_TYPE type,int external_port,UPNPCALLBACK callback)
{
    if(!m_init_ok||m_control_url==string())return;
    std::thread t(std::bind(&UpnpMapper::delete_port_mapper,this,type,external_port,callback));
    t.detach();
}
void UpnpMapper::Api_GetNewexternalIP(UPNPCALLBACK callback)
{
if(!m_init_ok||m_control_url==string())return;
    std::thread t(std::bind(&UpnpMapper::get_wanip,this,callback));
    t.detach();
}
std::shared_ptr<Upnp_Connection> UpnpMapper::newConnection(SOCKET sockfd,UPNP_COMMAND mode)
{
    return std::make_shared<Upnp_Connection>(this, m_event_loop->getTaskScheduler().get(), sockfd,mode);
}
void UpnpMapper::addConnection(SOCKET sockfd, std::shared_ptr<Upnp_Connection> Conn)
{
    Conn->setDisconnectCallback([this] (xop::TcpConnection::Ptr conn){
        auto taskScheduler = conn->getTaskScheduler();
        int sockfd = conn->fd();
        if (!taskScheduler->addTriggerEvent([this, sockfd] {this->removeConnection(sockfd); }))
        {
            taskScheduler->addTimer([this, sockfd]() {this->removeConnection(sockfd); return false;}, 1);
        }
    });
    std::lock_guard<std::mutex> locker(m_map_mutex);
    m_connections.emplace(sockfd, Conn);
}
void UpnpMapper::removeConnection(SOCKET sockfd)
{
    std::lock_guard<std::mutex> locker(m_map_mutex);
    m_connections.erase(sockfd);
}
void UpnpMapper::addTimeoutEvent(SOCKET sockfd)
{
    //m_event_loop->addTimer([this,sockfd](){this->removeConnection(sockfd);return false;},MAX_WAIT_TIME);
}
void UpnpMapper::send_discover_packet()
{
    if(!m_udp_channel||m_init_ok)return;
    cout<<"send_discover_packet"<<endl;
    struct sockaddr_in addr = {0};
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=inet_addr("239.255.255.250");
    addr.sin_port=htons(1900);
    socklen_t  addr_len=sizeof addr ;
    string discover_packet;
    //send ppp discover
    discover_packet="M-SEARCH * HTTP/1.1\r\n";
    discover_packet+="HOST: 239.255.255.250:1900\r\n";
    discover_packet+="ST: urn:schemas-upnp-org:service:WANPPPConnection:1\r\n";
    discover_packet+="MAN: \"ssdp:discover\"\r\n";
    discover_packet+="MX: 9\r\n";
    discover_packet+="\r\n";
    sendto(m_udp_channel->fd(),discover_packet.c_str(),discover_packet.length(),0,(struct sockaddr *)&addr,addr_len);
    //send ip discover
    discover_packet="M-SEARCH * HTTP/1.1\r\n";
    discover_packet+="HOST: 239.255.255.250:1900\r\n";
    discover_packet+="ST: urn:schemas-upnp-org:service:WANIPConnection:1\r\n";
    discover_packet+="MAN: \"ssdp:discover\"\r\n";
    discover_packet+="MX: 9\r\n";
    discover_packet+="\r\n";
    sendto(m_udp_channel->fd(),discover_packet.c_str(),discover_packet.length(),0,(struct sockaddr *)&addr,addr_len);
    //send igd discover
    discover_packet="M-SEARCH * HTTP/1.1\r\n";
    discover_packet+="HOST: 239.255.255.250:1900\r\n";
    discover_packet+="ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n";
    discover_packet+="MAN: \"ssdp:discover\"\r\n";
    discover_packet+="MX: 9\r\n";
    discover_packet+="\r\n";
    sendto(m_udp_channel->fd(),discover_packet.c_str(),discover_packet.length(),0,(struct sockaddr *)&addr,addr_len);
    //send rootdevice discover
    discover_packet="M-SEARCH * HTTP/1.1\r\n";
    discover_packet+="HOST: 239.255.255.250:1900\r\n";
    discover_packet+="ST: upnp:rootdevice\r\n";
    discover_packet+="MAN: \"ssdp:discover\"\r\n";
    discover_packet+="MX: 9\r\n";
    discover_packet+="\r\n";
    sendto(m_udp_channel->fd(),discover_packet.c_str(),discover_packet.length(),0,(struct sockaddr *)&addr,addr_len);
}
void UpnpMapper::get_control_url()
{
    if(m_init_ok)return;
    xop::TcpSocket tcpsock;
    tcpsock.create();
    if(tcpsock.fd()<=0)
    {
        cout<<"failed to create socket!"<<endl;
        return;
    }
    if(!tcpsock.connect(m_lgd_ip,m_lgd_port,MAX_WAIT_TIME))
    {
        cout<<"failed to connect to the device!"<<endl;
        tcpsock.close();
        return;
    }
    auto conn=newConnection(tcpsock.fd(),UPNP_GETCONTROLURL);
    if(conn)
    {
        addConnection(tcpsock.fd(),conn);
        conn->send_get_control_url();
        //must wake_up event_loop
        m_event_loop->addTriggerEvent([](){});
    }
}
void UpnpMapper::get_wanip(UPNPCALLBACK callback)
{
    if(!m_init_ok||m_control_url==string())return;
    xop::TcpSocket tcpsock;
    tcpsock.create();
    if(tcpsock.fd()<=0)
    {
        cout<<"failed to create socket!"<<endl;
        return;
    }
    if(!tcpsock.connect(m_lgd_ip,m_lgd_port,MAX_WAIT_TIME))
    {
        cout<<"failed to connect to the device!"<<endl;
        tcpsock.close();
        return;
    }
    auto conn=newConnection(tcpsock.fd(),UPNP_GETEXTERNALIPADDRESS);
    if(conn)
    {
        addConnection(tcpsock.fd(),conn);
        if(callback)conn->setUPNPCallback(callback);
        conn->send_get_wanip();
        //must wake_up event_loop
        m_event_loop->addTriggerEvent([](){});
    }
}
void UpnpMapper::add_port_mapper(SOCKET_TYPE type,string internal_ip,int internal_port,int external_port,string description,UPNPCALLBACK callback)
{
    xop::TcpSocket tcpsock;
    tcpsock.create();
    if(tcpsock.fd()<=0)
    {
        cout<<"failed to create socket!"<<endl;
        return;
    }
    if(!tcpsock.connect(m_lgd_ip,m_lgd_port,MAX_WAIT_TIME))
    {
        cout<<"failed to connect to the device!"<<endl;
        tcpsock.close();
        return;
    }
    auto conn=newConnection(tcpsock.fd(),UPNP_ADDPORTMAPPING);
    if(conn)
    {
        addConnection(tcpsock.fd(),conn);
        if(callback)conn->setUPNPCallback(callback);
        conn->send_add_port_mapper(type,internal_ip,internal_port,external_port,description);
        //must wake_up event_loop
        m_event_loop->addTriggerEvent([](){});
    }
}
void UpnpMapper::get_specific_port_mapping_entry(SOCKET_TYPE type,int external_port,UPNPCALLBACK callback)
{
    xop::TcpSocket tcpsock;
    tcpsock.create();
    if(tcpsock.fd()<=0)
    {
        cout<<"failed to create socket!"<<endl;
        return;
    }
    if(!tcpsock.connect(m_lgd_ip,m_lgd_port,MAX_WAIT_TIME))
    {
        cout<<"failed to connect to the device!"<<endl;
        tcpsock.close();
        return;
    }
    auto conn=newConnection(tcpsock.fd(),UPNP_GETSPECIFICPORTMAPPINGENTRY);
    if(conn)
    {
        addConnection(tcpsock.fd(),conn);
        if(callback)conn->setUPNPCallback(callback);
        conn->send_get_specific_port_mapping_entry(type,external_port);
        //must wake_up event_loop
        m_event_loop->addTriggerEvent([](){});
    }
}
void UpnpMapper::delete_port_mapper(SOCKET_TYPE type,int external_port,UPNPCALLBACK callback)
{
    xop::TcpSocket tcpsock;
    tcpsock.create();
    if(tcpsock.fd()<=0)
    {
        cout<<"failed to create socket!"<<endl;
        return;
    }
    if(!tcpsock.connect(m_lgd_ip,m_lgd_port,MAX_WAIT_TIME))
    {
        cout<<"failed to connect to the device!"<<endl;
        tcpsock.close();
        return;
    }
    auto conn=newConnection(tcpsock.fd(),UPNP_DELETEPORTMAPPING);
    if(conn)
    {
        addConnection(tcpsock.fd(),conn);
        if(callback)conn->setUPNPCallback(callback);
        conn->send_delete_port_mapper(type,external_port);
        //must wake_up event_loop
        m_event_loop->addTriggerEvent([](){});
    }
}
