#include "p2p_punch_server.h"
int main()
{
    p2p_punch_server::Get_Instance().start_server();
    return 0;
}
