#ifndef PARASE_REQUEST_H
#define PARASE_REQUEST_H
#include <map>
#include <iostream>
#include <string>
class message_handle
{
public:
    static std::map<std::string,std::string> parse_buf(std::string &buf);
    static std::map<std::string,std::string> parse_buf(const char *c_buf);
    static void packet_buf(std::string &buf,std::string key,std::string value);
};
#endif // PARASE_REQUEST_H
