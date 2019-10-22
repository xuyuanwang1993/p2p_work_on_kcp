#include "p2p_punch_server.h"
int main(int argc,char *argv[])
{
    if(argc>2)
    {
        std::string mode=argv[2];
        if(mode=="-d"||mode=="-D"||mode=="--debug")
        {
            p2p_punch_server::Get_Instance().open_debug();
        }
    }
    if (argc>1) {
        p2p_punch_server::Get_Instance().init_stream_server_task(argv[1]);
    }
    p2p_punch_server::Get_Instance().start_server();
    return 0;
}
