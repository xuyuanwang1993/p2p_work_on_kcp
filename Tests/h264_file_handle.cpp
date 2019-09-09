#include "h264_file_handle.h"
H264File::H264File(int bufSize)
    : m_bufSize(bufSize)
{
    m_buf = new char[m_bufSize];
}

H264File::~H264File()
{
    delete m_buf;
}

bool H264File::open(const char *path)
{
    m_file = fopen(path, "rb");
    if(m_file == NULL)
    {
       return false;
    }

    return true;
}

void H264File::close()
{
    if(m_file)
    {
        fclose(m_file);
        m_file = NULL;
        m_count = 0;
        m_bytesUsed = 0;
    }
}

int H264File::readFrame(char *inBuf, int inBufSize, bool *bEndOfFrame)
{
    if(m_file == NULL)
    {
        return -1;
    }

    int bytesRead = fread(m_buf, 1, m_bufSize, m_file);
    if(bytesRead == 0)
    {
        fseek(m_file, 0, SEEK_SET);
        m_count = 0;
        m_bytesUsed = 0;
        bytesRead = fread(m_buf, 1, m_bufSize, m_file);
        if(bytesRead == 0)
        {
            this->close();
            return -1;
        }
    }

    bool bFindStart = false, bFindEnd = false;

    int i = 0, startCode = 3;
    *bEndOfFrame = false;
    for (i=0; i<bytesRead-5; i++)
    {
        if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 1)
        {
            startCode = 3;
        }
        else if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 0 && m_buf[i+3] == 1)
        {
            startCode = 4;
        }
        else
        {
            continue;
        }

        if (((m_buf[i+startCode]&0x1F) == 0x5 || (m_buf[i+startCode]&0x1F) == 0x1) &&
             ((m_buf[i+startCode+1]&0x80) == 0x80))
        {
            bFindStart = true;
            i += 4;
            break;
        }
    }

    for (; i<bytesRead-5; i++)
    {
        if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 1)
        {
            startCode = 3;
        }
        else if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 0 && m_buf[i+3] == 1)
        {
            startCode = 4;
        }
        else
        {
            continue;
        }

        if (((m_buf[i+startCode]&0x1F) == 0x7) || ((m_buf[i+startCode]&0x1F) == 0x8)
            || ((m_buf[i+startCode]&0x1F) == 0x6)|| (((m_buf[i+startCode]&0x1F) == 0x5
            || (m_buf[i+startCode]&0x1F) == 0x1) &&((m_buf[i+startCode+1]&0x80) == 0x80)))
        {
            bFindEnd = true;
            break;
        }
    }

    bool flag = false;
    if(bFindStart && !bFindEnd && m_count>0)
    {
        flag = bFindEnd = true;
        i = bytesRead;
        *bEndOfFrame = true;
    }

    if(!bFindStart || !bFindEnd)
    {
        this->close();
        return -1;
    }

    int size = (i<=inBufSize ? i : inBufSize);
    memcpy(inBuf, m_buf, size);

    if(!flag)
    {
        m_count += 1;
        m_bytesUsed += i;
    }
    else
    {
        m_count = 0;
        m_bytesUsed = 0;
    }

    fseek(m_file, m_bytesUsed, SEEK_SET);
    return size;
}
