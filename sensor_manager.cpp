#include "sensor_manager.h"
#include <iostream>
#include <cstring>
#include <stdlib.h>
void Sensor_Manager::Sensor_Task_Model()
{
    m_sensor_task.reset(Sensor_Task::CreateNew());
    do{
        m_sensor_task->Sensor_Task_Init();
        try{
            m_sensor_task->start();
        }
        catch(char const * e){
            std::cout<<e<<std::endl;
            break;
        }
        catch(...){
            break;
        }
        return;
    }while(0);
    m_sensor_task.reset();
}
void Sensor_Manager::Http_Upload_Model()
{
    static std::once_flag oc_init;
    std::call_once(oc_init,[&](){
        m_http_unpload_run=true;
        m_http_thread.reset(new std::thread([&](long long interval){
            std::cout<<"http upload thread!"<<std::endl;
            while(m_http_unpload_run)
            {
                //std::cout<<"sleep"<<std::endl;
                std::this_thread::sleep_for(std::chrono::microseconds(interval));
                //std::cout<<"time out"<<std::endl;
                CJSON *s_data=NULL;
                std::shared_ptr<Sensor_Task::CJSON_ptr> ptr;
                do{
                    std::lock_guard<std::mutex> lock(m_mutex);
                    Sensor_Task *task=m_sensor_task.get();
                    if(!task)break;
                    ptr=task->Get_Record_List();
                    s_data=ptr.get()->_get();
                }while(0);
                if(s_data)
                {
                    std::string url=s_upload_api;
                    http_client*client=http_client::CreateNew_http_client(url);
                    std::string http_packet="device_id=";
                    http_packet+=m_device_id;
                    struct timeval now;
                    gettimeofday(&now,NULL);
                    http_packet+="&now_time=";
                    http_packet+=std::to_string(now.tv_sec);
                    std::cout<<http_packet<<std::endl;
                    //http_packet+="&data=";
                    http_packet+="&log_info=";
                    char *tmp=CJSON_PrintUnformatted(s_data);
                    std::cout<<tmp<<std::endl;
                    printf("%p \n",tmp);
                    http_packet+=CJSON_PrintUnformatted(s_data);
                    std::cout<<http_packet<<std::endl;
                    client->Set_Content(http_packet);
                    std::string ack;
                    client->Get_Http_Ack(ack);
                }
            }
            std::cout<<"http upload thread exit!"<<std::endl;
        },UPLOAD_INTERVAL));
        std::cout<<"detach"<<std::endl;
        m_http_thread->detach();
    });
}
Sensor_Manager::Sensor_Manager()
{
    FILE *fp=fopen(this->s_config_file_name.c_str(),"r");
    m_device_id="*";
    if(!fp)return;
    char name[128]={0};
    char value[128]={0};
    char buf[256]={0};
    while(fgets(buf,sizeof(buf),fp))
    {
        memset(name,0,128);
        memset(value,0,128);
        sscanf(buf,"%s : %s",name,value);
        if(strcmp(name,"device_id")==0)
        {
            if(value[0]!='\0')m_device_id=value;
            break;
        }
    }
    fclose(fp);
}
