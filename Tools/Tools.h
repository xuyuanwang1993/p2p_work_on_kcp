#ifndef TOOLS_H
#define TOOLS_H
#include "Net/http_client.h"
//the return buf must be delete by free()
char *Get_MD5_String(const char *buf,int buf_size);
#endif
