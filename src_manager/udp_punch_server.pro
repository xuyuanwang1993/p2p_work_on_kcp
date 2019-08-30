INCLUDEPATH += ../ \
    ../examples \
    ../Net \
    ../p2p_punch_model \
    ../Tools \
    ../Tests
HEADERS += ../Net/Timestamp.h \
    ../Net/Timer.h \
    ../Net/ThreadSafeQueue.h \
    ../Net/TcpSocket.h \
    ../Net/TcpServer.h \
    ../Net/TcpConnection.h \
    ../Net/TaskScheduler.h \
    ../Net/SocketUtil.h \
    ../Net/Socket.h \
    ../Net/SelectTaskScheduler.h \
    ../Net/RingBuffer.h \
    ../Net/Pipe.h \
    ../Net/NetInterface.h \
    ../Net/MemoryManager.h \
    ../Net/Logger.h \
    ../Net/log.h \
    ../Net/kcp_manage.h \
    ../Net/ikcp.h \
    ../Net/http_client.h \
    ../Net/EventLoop.h \
    ../Net/EpollTaskScheduler.h \
    ../Net/dns.h \
    ../Net/Channel.h \
    ../Net/BufferWriter.h \
    ../Net/BufferReader.h \
    ../Net/Acceptor.h \
    ../Tools/Tools.h \
    ../Tools/sensor_task.h \
    ../Tools/MD5.h \
    ../Tools/cJSON.h \
    ../Tools/Base64.h \
    ../p2p_punch_model/p2p_punch_server.h \
    ../p2p_punch_model/p2p_punch_client.h \
    ../Tools/parase_request.h \
    ../sensor_manager.h \
    ../Tests/kcp_test.h
SOURCES += ../Net/Timestamp.cpp \
    ../Net/Timer.cpp \
    ../Net/TcpSocket.cpp \
    ../Net/TcpServer.cpp \
    ../Net/TcpConnection.cpp \
    ../Net/TaskScheduler.cpp \
    ../Net/SocketUtil.cpp \
    ../Net/SelectTaskScheduler.cpp \
    ../Net/Pipe.cpp \
    ../Net/NetInterface.cpp \
    ../Net/MemoryManager.cpp \
    ../Net/Logger.cpp \
    ../Net/kcp_manage.cpp \
    ../Net/ikcp.c \
    ../Net/http_client.cpp \
    ../Net/EventLoop.cpp \
    ../Net/EpollTaskScheduler.cpp \
    ../Net/dns.c \
    ../Net/BufferWriter.cpp \
    ../Net/BufferReader.cpp \
    ../Net/Acceptor.cpp \
    ../Tools/Tools.cpp \
    ../Tools/sensor_task.cpp \
    ../Tools/MD5.c \
    ../Tools/cJSON.c \
    ../Tools/Base64.cpp \
    ../p2p_punch_model/p2p_punch_server.cpp \
    ../p2p_punch_model/p2p_punch_client.cpp \
    ../Tools/parase_request.cpp \
    ../sensor_manager.cpp \
    ../examples/p2p_client.cpp \
    ../examples/p2p_server.cpp \
    ../Tests/kcp_test.cpp
OTHER_FILES += run_helper.txt
