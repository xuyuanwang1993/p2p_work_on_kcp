// PHZ
// 2018-5-15

#ifndef XOP_NET_INTERFACE_H
#define XOP_NET_INTERFACE_H

#include <string>
//获取本地ip
namespace xop
{
    class NetInterface
    {
    public:
        static std::string getLocalIPAddress();
    };
}

#endif
