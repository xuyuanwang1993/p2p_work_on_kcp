#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H
#include "Tools/sensor_task.h"
#include "Net/http_client.h"
class Sensor_Manager
{
    const std::string s_config_file_name ="p2p_push_server.config";
    const std::string s_upload_api="http://www.meanning.com/api/recv_log";
    const long long UPLOAD_INTERVAL=60*1000*1000;//传感器数据上传间隔(us)
public:
    static Sensor_Manager &Get_Instance()
    {
        static std::once_flag oc_init;
        std::call_once(oc_init, [] {
        });
        static Sensor_Manager manager;
        return manager;
    }
    void main_task_start()
    {
        Sensor_Task_Model();
        Http_Upload_Model();
    }

protected:
    ~Sensor_Manager(){
        m_http_unpload_run=false;
        m_sensor_task.reset();
    };
    Sensor_Manager();
private:
    std::shared_ptr<Sensor_Task> m_sensor_task;
    std::string m_device_id;//设备id
    std::mutex m_mutex;
    std::shared_ptr<std::thread> m_http_thread;
    std::atomic<bool> m_http_unpload_run;
private:
    void Http_Upload_Model();
    void Sensor_Task_Model();
};

#endif // SENSOR_MANAGER_H
