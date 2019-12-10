#ifndef KCP_DOWNLAND_TOOL_H
#define KCP_DOWNLAND_TOOL_H
#include <kcp_manage.h>
#include <file_reader.h>
namespace micagent{
typedef struct  _kcp_stream_header{
    uint32_t command_type;//指令名 (无符号8位)
    uint32_t session_id;//会话id
    uint64_t sequence_id;//视频帧序列号
    uint64_t max_sequence_id;//帧总数
    uint64_t data_size;//包长度
    uint32_t cseq;//指令序列号，过期的指令序列不处理
    uint32_t check_mask;//头部校验码
    _kcp_stream_header(){
        command_type=0;
        session_id=0;
        sequence_id=0;
        max_sequence_id=0;
        data_size=0;
        cseq=0;
        check_mask=0;
    }
}kcp_stream_header;
typedef enum{
    KCP_TRANSFER_START,//传输开始
}KCP_DOWNLAND_COMMAND;
}
#endif // KCP_DOWNLAND_TOOL_H
