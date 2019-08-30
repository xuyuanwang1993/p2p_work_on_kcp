#ifndef BASE64_H
#define BASE64_H
#include <string>
namespace sensor_tool{
    std::string base64Encode(const std::string & origin_string);
    std::string base64Decode(const std::string & origin_string);
};
#endif // BASE64_H
