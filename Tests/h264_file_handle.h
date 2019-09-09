#ifndef H264_FILE_HANDLE_H
#define H264_FILE_HANDLE_H
#include <cstdio>
#include <cstring>
class H264File
{
public:
    H264File(int bufSize=500000);
    ~H264File();

    bool open(const char *path);
    void close();

    bool isOpened() const
    {
        return (m_file != NULL);
    }

    int readFrame(char *inBuf, int inBufSize, bool *bEndOfFrame);

private:
    FILE *m_file = NULL;
    char *m_buf = NULL;
    int m_bufSize = 0;
    int m_bytesUsed = 0;
    int m_count = 0;
};
#endif // H264_FILE_HANDLE_H
