#include "file_reader.h"
#include <string.h>
#include<errno.h>
#include<sys/time.h>
#include<iostream>
using namespace micagent;
file_reader_base::file_reader_base(string fileName)
{
    m_file_name=fileName;
    m_fp=fopen(fileName.c_str(),"r");
    m_last_index=static_cast<size_t>(~0);
    if(m_fp)parase_stream_info();
    else {
        MICAGENT_LOG(PRINT_RED "Can't open %s!",fileName.c_str());
    }
}
size_t file_reader_base::readFrame(unsigned char *inBuf,unsigned int inBufSize,size_t index,bool jump)
{
//    struct timeval time_now;
//    gettimeofday(&time_now,nullptr);
    size_t ret=0;
    do{
        if(!m_fp||index>=m_frame_begin_poses.size())
        {
            MICAGENT_LOG(PRINT_RED "Read frame %lu error!",index);
            break;
        }
        m_last_index=m_last_index+1;
        m_last_index=m_last_index==m_frame_begin_poses.size()?0:m_last_index;
        if(jump)
        {
            if (fseek(m_fp, static_cast<long>(m_frame_begin_poses[index]), SEEK_SET) != 0)break;
            m_last_index=index;
        }
        else {
            if (fseek(m_fp, static_cast<long>(m_frame_begin_poses[m_last_index]), SEEK_SET) != 0)break;
        }
        size_t next_frame_pos=m_last_index+1==m_frame_begin_poses.size()?m_file_size:m_frame_begin_poses[static_cast<size_t>(m_last_index+1)];
        size_t frame_size=next_frame_pos-m_frame_begin_poses[static_cast<size_t>(m_last_index)];
        if(inBufSize<frame_size)
        {
            MICAGENT_LOG(PRINT_RED "inBufSize is too small! It needs %lu at least but now %u is available!",frame_size,inBufSize);
            break;
        }
        ret=fread(inBuf,1,frame_size,m_fp);
        if(ret!=frame_size)
        {
            ret=0;
            MICAGENT_LOG(PRINT_RED " IO errors! %s ",strerror(errno));
            break;
        }
//        struct timeval time_over;
//        gettimeofday(&time_over,nullptr);
//        long long us=(time_over.tv_sec-time_now.tv_sec)*1000000+(time_over.tv_usec-time_now.tv_usec);
//        MICAGENT_LOG(PRINT_GREEN "readFrame using time %lld microseconds!",us);
    }while(0);
    return  ret;
}
size_t file_reader_base::readBuf(unsigned char *inBuf,unsigned int inBufSize,size_t pos,bool jump)
{
    size_t ret=0;
    do{
        if(!m_fp)break;
        if(jump)
        {
            if(fseek(m_fp,static_cast<long>(pos),SEEK_SET)!=0)break;
        }
        ret=fread(inBuf,1,inBufSize,m_fp);
    }while(0);
}
void file_reader_base::show_a_frame_info(size_t index,size_t max_out_put_size)const
{
    size_t ret=0;
    do{
        if(!m_fp||index>=m_frame_begin_poses.size())
        {
            MICAGENT_LOG(PRINT_RED "Read frame %lu error!",index);
            break;
        }
        if (fseek(m_fp, static_cast<long>(m_frame_begin_poses[index]), SEEK_SET) != 0)break;
        size_t next_frame_pos=index+1==m_frame_begin_poses.size()?m_file_size:m_frame_begin_poses[static_cast<size_t>(index+1)];
        size_t frame_size=next_frame_pos-m_frame_begin_poses[static_cast<size_t>(index)];
        frame_size=max_out_put_size>frame_size?frame_size:max_out_put_size;
        std::shared_ptr<unsigned char>read_buf(new unsigned char[frame_size]);
        ret=fread(read_buf.get(),1,frame_size,m_fp);
        if(ret!=frame_size)
        {
            ret=0;
            MICAGENT_LOG(PRINT_RED " IO errors! %s ",strerror(errno));
            break;
        }
        printf(PRINT_PURPLE "frame : %lu ",index);
        for(size_t i=0;i<frame_size;i++)
        {
            printf("%02x ",read_buf.get()[i]);
        }
        printf("\r\n" PRINT_NONE);
    }while(0);
    if(ret==0)MICAGENT_LOG(PRINT_RED "Get %lu frame info error!",index);
}
void file_reader_base::show_all_frame_info(size_t max_out_put_size)const
{
    for(size_t i=0;i<m_frame_begin_poses.size();i++)
    {
        show_a_frame_info(i,max_out_put_size);
    }
}
void file_reader_base::dump_file_info(size_t items)const
{
    size_t limit=m_frame_begin_poses.size();
    MICAGENT_LOG(PRINT_GREEN "now is all %lu frames info!",limit);
    if(items!=0)limit=limit<items?limit:items;
    for(size_t i=0;i<limit;i++)
    {
        MICAGENT_LOG(PRINT_YELLOW "frame:%lu    begin_pos: %lu",i,m_frame_begin_poses[i]);
    }
    MICAGENT_LOG(PRINT_GREEN "now is all %lu types info!",m_frame_info_map.size());
    for(auto i : m_frame_info_map)
    {
        limit=i.second.size();
        MICAGENT_LOG(PRINT_LIGHT_RED "frame_type : %02x counts : %lu",i.first,limit);
        if(items!=0)limit=limit<items?limit:items;
        for(size_t j=0;j<limit;j++)
        {
            MICAGENT_LOG(PRINT_YELLOW "frame:%lu    begin_pos: %lu",j,i.second[j]);
        }
    }
}
file_reader_base::~file_reader_base()
{
    if(m_fp)
    {
        fclose(m_fp);
        m_fp=nullptr;
    }
}
void file_reader_base::parase_stream_info()
{
    struct timeval time_now;
    gettimeofday(&time_now,nullptr);
    do{
        if(!m_fp)
        {
            MICAGENT_LOG(PRINT_RED "%s is not opened!",m_file_name.c_str());
            break;
        }
        if(fseek(m_fp,0,SEEK_END)!=0)break;
        long read_size=ftell(m_fp);
        if(read_size<=0)
        {
            MICAGENT_LOG(PRINT_RED "%s is empty!",m_file_name.c_str());
            break;
        }
        if(fseek(m_fp,0,SEEK_SET)!=0)break;
        m_file_size=static_cast<size_t>(read_size);
        size_t bytes_used=0;
        read_size=0;
        std::shared_ptr<unsigned char>read_buf(new unsigned char[DEFAULT_STREAM_BUF_SIZE]);
        while(bytes_used!=m_file_size-5)
        {
            read_size=fread(read_buf.get(),1,DEFAULT_STREAM_BUF_SIZE,m_fp);
            if(read_size<=0)break;
            long i=0;
            for(;i<read_size-5;)
            {
                if(read_buf.get()[i]==0x00&&read_buf.get()[i+1]==0x00&&read_buf.get()[i+2]==0x00&&read_buf.get()[i+3]==0x01)
                {
                    m_frame_begin_poses.push_back(bytes_used+i);
                    m_frame_info_map[read_buf.get()[i+4]].push_back(bytes_used+i);
                    i+=4;
                }
                else if(read_buf.get()[i]==0x00&&read_buf.get()[i+1]==0x00&&read_buf.get()[i+2]==0x01)
                {
                    m_frame_begin_poses.push_back(bytes_used+i);
                    m_frame_info_map[read_buf.get()[i+3]].push_back(bytes_used+i);
                    i+=3;
                }
                else {
                    i++;
                }
            }
            bytes_used+=i;
            if(fseek(m_fp,static_cast<long>(bytes_used),SEEK_SET)!=0)break;
        }
        fseek(m_fp,0,SEEK_SET);
        struct timeval time_over;
        gettimeofday(&time_over,nullptr);
        double ms=(time_over.tv_sec-time_now.tv_sec)*1000+(time_over.tv_usec-time_now.tv_usec)/1000;
        MICAGENT_LOG(PRINT_GREEN "parase_stream_info using time %lf milliseconds!",ms);
    }while(0);

}
