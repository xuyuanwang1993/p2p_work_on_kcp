#include "MD5.h"
#include "Tools.h"
#include <stdlib.h>
#include <stdio.h>
char *Get_MD5_String(const char *buf,int buf_size)
{
    MD5_CTX md5_ctx;
    unsigned char* aHash = (unsigned char *)malloc(sizeof(unsigned char)*16);
    MD5Init(&md5_ctx);
    MD5Update(&md5_ctx,(unsigned char *)buf,buf_size);
    MD5Final(aHash,&md5_ctx);
    char *md5_string=(char *)calloc(1,33);
    for(int i=0;i<16;i++)
    {
        sprintf(md5_string+i*2,"%02x",aHash[i]);
    }
    md5_string[32]='\0';
    return md5_string;
}
