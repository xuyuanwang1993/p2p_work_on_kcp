#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H
#include "cJSON.h"
#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <map>
#include <unordered_map>
#include <time.h>
#include <unordered_set>
#include <sys/time.h>
#include <mutex>
#include <atomic>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <thread>
#ifndef IGNORE
#define IGNORE (0xF1)
#endif
#ifndef KEY_DATA
#define KEY_DATA (0xF2)
#endif
/* open the serial uart 1 */
#define SERIAL_UART1 	"/dev/ttyAMA1"
#define CONFIG_FILE_PATH  ""
#define CONFIG_FILE_NAME "device.config"
class Sensor_Task{
    static const int MAX_COMMAND=8;
    static const int MAX_DATA_ITEMS=100;//单项数据条数不超过100
    static const int RUN_INTERVAL=30*1000*1000;//传感器数据读取间隔(us)
public:
    struct command{
        std::string command_name;//指令项名
        bool in_use;//是否被禁用
        unsigned char send_data[16];//发送数据
        int send_size;//数据长度
        unsigned char handle_rule[16];//数据处理规则
        int handle_size;//数据处理规则长度
        std::string unit;//数据单位
        std::string description;//传感器描述
    };
    struct sensor_data{
        time_t time;//记录数据项的当前时间（s）
        union{
            unsigned char data_char[4];//数据项（小端字节序）
            int data_int;
        }data;
    };
    struct record{
        std::string command_name;//指令项名
        std::string unit;//数据单位
        std::string location_mark;//位置信息
        std::vector<struct sensor_data>data_record;
    };
    using command_ptr=std::shared_ptr<struct command>;
    class CJSON_ptr{
    public:
        CJSON_ptr(CJSON * _ptr=NULL){m_ptr=_ptr;};
        CJSON *_get(){return m_ptr;};
        void _set(CJSON * _ptr){CJSON_free(m_ptr);m_ptr=_ptr;};
        ~CJSON_ptr(){CJSON_free(m_ptr);};
    private:
        CJSON *m_ptr;
    };//CJSON管理类封装
    //创建配置文件
    static CJSON *Create_Config_File(std::string device_name,bool big_endian);
    //创建指令,若存在则启用并替换数据
    static bool Add_Command(CJSON * root,std::string command_name,unsigned char * send_data,int send_size,unsigned char *handle_rule,int rule_size,std::string unit,std::string description);
    //禁用指令
    static void Disable_Command(CJSON * root,std::string command_name);
    //修改数据规则
    static void Modify_Command(CJSON * root,std::string command_name,unsigned char * send_data,int send_size,unsigned char *handle_rule,int rule_size,std::string unit,std::string description);
    //保存配置文件
    static void Save_Config_File(CJSON * root,std::string name=CONFIG_FILE_NAME);
    //创建实例
    static Sensor_Task *CreateNew(std::string config_name=CONFIG_FILE_NAME);
    //获取当前缓冲区的数据指针
    std::shared_ptr<CJSON_ptr> Get_Record_List();
    bool Sensor_Task_Init();
    void Insert_Record_To_List(std::string &command_name,unsigned char *_data,int data_size=2);
    void start();
    void stop();
    ~Sensor_Task();
private:
    std::unordered_map<std::string,command_ptr> m_command_map;//可用指令全集
    std::unordered_set<std::string> m_command_set;//任务指令集
    std::unordered_map<std::string,struct record> m_record_list;//临时存储传感器数据
    std::shared_ptr<CJSON_ptr> m_CJSON;//配置文件指针
    std::mutex m_mutex;
    std::atomic<int> m_serial_fd;//串口读写句柄
    std::string m_config_name;//配置名
    bool m_big_endian;//是否为大端模式
    bool m_init_success;//初始化成功
    std::atomic<bool> m_task_brun;//线程是否正在运行
    std::shared_ptr<std::thread> m_thread;
private:
    Sensor_Task(std::string config_name=CONFIG_FILE_NAME);
    void Handle_Data(command_ptr cmd);//发送指令前应清空缓冲区
    void run();
    void task();
    int serial_open(void)
    {//打开串口
            return open(SERIAL_UART1, O_RDWR);
    };
    int serial_close(int fd)
    {//关闭串口
            return close(fd);
    };
    /**
     * the init serial process, enable the gpio 4_2
     * failed: return -1; sucess: return 0;
     */
    int serial_init(void)
    {//检查串口读写特性
            if (access("/sys/class/gpio/gpio34", F_OK) == 0) {
                    system("echo 34 > /sys/class/gpio/unexport");
                    sleep(1);
            }
            if (system("echo 34 > /sys/class/gpio/export") == -1) {
                    return -1;
            }
            if (system("echo out > /sys/class/gpio/gpio34/direction") == -1) {
                    return -1;
            }
            return 0;
    };
    /**
     * set the in and out bandrate speed
     * input: fd: the serial file
     * 		  band: the bandrate speed, before speed there "B", like: B9600, B115200, etc
     */
    void serial_setspeed(int fd, uint32_t band)
    {//设置串口波特率
            struct termios ser;
            tcgetattr(fd, &ser);
            cfsetispeed(&ser, band);
            cfsetospeed(&ser, band);

            tcflush(fd, TCIOFLUSH);
            tcsetattr(fd, TCSANOW, &ser);
    };
    int serial_send(int fd, uint8_t *data,  uint32_t len)
    {//串口发送
            int ret;
            int gf = open("/sys/class/gpio/gpio34/value", O_WRONLY);
            if (gf == -1) {
                    printf("Failed to open gpio value.\n");
                    return -1;
            }
            write(gf, "1", sizeof("1"));
            usleep(100000);
            ret = write(fd, data, len);
            usleep(15000);
            write(gf, "0", sizeof("0"));
            close(gf);
            return ret;
    };
    int serial_receive(int fd, uint8_t *data, uint32_t len)
    {//读取串口数据
            int ret = read(fd, data, len);
            usleep(10000);
            return ret;
    };
};

#endif // SENSOR_TASK_H
