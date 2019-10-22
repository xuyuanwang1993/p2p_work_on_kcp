#ifndef PARASE_REQUEST_H
#define PARASE_REQUEST_H
#include <map>
#include <iostream>
#include <string>
class message_handle
{
public:
    static std::map<std::string,std::string> parse_buf(std::string &buf,std::string delimiter="\r\n");
    static std::map<std::string,std::string> parse_buf(const char *c_buf,std::string delimiter="\r\n");
    static void packet_buf(std::string &buf,std::string key,std::string value);
    static void packet_http_buf(std::string &buf,std::string key,std::string value);
    static void packet_append_endl(std::string &buf);
};
#endif // PARASE_REQUEST_H
