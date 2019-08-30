#include "sensor_task.h"
#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include <iostream>
#include <chrono>//提供计时功能
#include <random>//随机数 std::random_device
#include "Net/SocketUtil.h"
#include <stdio.h>
//#define SENSOR_TEST
CJSON *Sensor_Task::Create_Config_File(std::string device_name,bool big_endian)
{
    CJSON *root=CJSON_CreateObject();
    CJSON_AddStringToObject(root,"device_name",device_name.c_str());
    CJSON_AddBoolToObject(root,"big_endian",big_endian);
    CJSON_AddObjectToObject(root,"command_list");
    return root;
}
bool Sensor_Task::Add_Command(CJSON * root,std::string command_name,unsigned char * send_data,int send_size,unsigned char *handle_rule,int rule_size,std::string unit,std::string description)
{
    if(!root)return false;
    CJSON *command_list=CJSON_GetObjectItemCaseSensitive(root,"command_list");
    if(!command_list)return false;
    CJSON *command=CJSON_GetObjectItemCaseSensitive(command_list,command_name.c_str());
    if(!command)
    {
        command=CJSON_AddObjectToObject(command_list,command_name.c_str());
        CJSON_AddBoolToObject(command,"in_use",1);
        CJSON *array=CJSON_CreateUcharArray(send_data,send_size);
        CJSON_AddItemToObject(command,"send_data",array);
        array=CJSON_CreateUcharArray(handle_rule,rule_size);
        CJSON_AddItemToObject(command,"handle_rule",array);
        CJSON_AddStringToObject(command,"unit",unit.c_str());
        CJSON_AddStringToObject(command,"description",description.c_str());
    }
    else
    {
        CJSON *replace=CJSON_CreateObject();
        CJSON_AddBoolToObject(replace,"in_use",1);
        CJSON *array=CJSON_CreateUcharArray(send_data,send_size);
        CJSON_AddItemToObject(replace,"send_data",array);
        array=CJSON_CreateUcharArray(handle_rule,rule_size);
        CJSON_AddItemToObject(replace,"handle_rule",array);
        CJSON_AddStringToObject(replace,"unit",unit.c_str());
        CJSON_AddStringToObject(replace,"description",description.c_str());
        CJSON_ReplaceItemInObjectCaseSensitive(command_list,command_name.c_str(),replace);
    }
    return true;
}
void Sensor_Task::Disable_Command(CJSON * root,std::string command_name)
{
    if(!root)return ;
    CJSON *command_list=CJSON_GetObjectItemCaseSensitive(root,"command_list");
    if(!command_list)return ;
    CJSON *command=CJSON_GetObjectItemCaseSensitive(command_list,command_name.c_str());
    if(command)
    {
        CJSON *in_use=CJSON_GetObjectItemCaseSensitive(command,"in_use");
        in_use->type=CJSON_False;
    }
}
void Sensor_Task::Modify_Command(CJSON * root,std::string command_name,unsigned char * send_data,int send_size,unsigned char *handle_rule,int rule_size,std::string unit,std::string description)
{
    if(!root)return ;
    CJSON *command_list=CJSON_GetObjectItemCaseSensitive(root,"command_list");
    if(!command_list)return ;
    CJSON *command=CJSON_GetObjectItemCaseSensitive(command_list,command_name.c_str());
    if(command)
    {
        CJSON *replace=CJSON_CreateObject();
        CJSON_AddBoolToObject(replace,"in_use",1);
        CJSON *array=CJSON_CreateUcharArray(send_data,send_size);
        CJSON_AddItemToObject(replace,"send_data",array);
        array=CJSON_CreateUcharArray(handle_rule,rule_size);
        CJSON_AddItemToObject(replace,"handle_rule",array);
        CJSON_AddStringToObject(replace,"unit",unit.c_str());
        CJSON_AddStringToObject(replace,"description",description.c_str());
        CJSON_ReplaceItemInObjectCaseSensitive(command_list,command_name.c_str(),replace);
    }
}

void Sensor_Task::Save_Config_File(CJSON * root,std::string name)
{
    if(!root)return;
    if(name==CONFIG_FILE_NAME)
    {
        CJSON *device_name=CJSON_GetObjectItemCaseSensitive(root,"device_name");
        if(!device_name)return;
        else
        {
            name=device_name->valuestring;
        }
    }
    else
    {
        CJSON *device_name=CJSON_GetObjectItemCaseSensitive(root,"device_name");
        if(!device_name)return;
        CJSON *replace=CJSON_CreateString(name.c_str());
        if(!CJSON_ReplaceItemViaPointer(root,device_name,replace))return;
    }
    name+=".config";
    std::string file_name=CONFIG_FILE_PATH;
    file_name+=name;
    FILE *fp=fopen(file_name.c_str(),"w+");
    if(!fp)return;
    fprintf(fp,"%s",CJSON_Print(root));
    fclose(fp);
}

Sensor_Task *Sensor_Task::CreateNew(std::string config_name)
{
    Sensor_Task *task=new Sensor_Task(config_name);
    task->m_serial_fd=-1;
    task->m_task_brun=false;
    task->m_init_success=false;
    return task;
}
std::shared_ptr<Sensor_Task::CJSON_ptr> Sensor_Task::Get_Record_List()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::shared_ptr<CJSON_ptr> ret(new CJSON_ptr(NULL));
    if(m_record_list.empty())return ret;
    CJSON *root=CJSON_CreateObject();
    ret->_set(root);
    for(auto i : m_record_list)
    {
        struct record tmp=i.second;
        if(tmp.data_record.empty())
        {
            std::cout<<i.first<<" empty"<<std::endl;
            continue;
        }
        CJSON *node=CJSON_AddObjectToObject(root,tmp.command_name.c_str());
        CJSON_AddStringToObject(node,"unit",tmp.unit.c_str());
        CJSON_AddStringToObject(node,"location_mark",tmp.location_mark.c_str());
        CJSON * array=CJSON_AddArrayToObject(node,"data_record");
        for(auto j :tmp.data_record)
        {
            std::string value=std::to_string(j.time)+":"+std::to_string(j.data.data_int);
            CJSON *data_item=CJSON_CreateString(value.c_str());
            CJSON_AddItemToArray(array,data_item);
        }
    }
    m_record_list.clear();
    return ret;
}
bool Sensor_Task::Sensor_Task_Init()
{
#ifdef SENSOR_TEST
    m_init_success=true;
#endif
    do{
        if(m_init_success)break;
        if(serial_init()<0)break;
        m_serial_fd=serial_open();
        if(m_serial_fd<=0)break;
        xop::SocketUtil::setRecvBlockTime(m_serial_fd,1000);
        serial_setspeed(m_serial_fd,B9600);
        //若有传感器需要初始化则在此处添加
        m_init_success=true;
    }while(0);
    std::cout<<"Sensor_Task init success"<<std::endl;
    return m_init_success;
}

void Sensor_Task::Insert_Record_To_List(std::string &command_name,unsigned char *_data,int data_size)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it=m_command_set.find(command_name);
    if(it==m_command_set.end()) return;
    struct record &iter=m_record_list[command_name];
    if(iter.data_record.empty())
    {
        iter.command_name=command_name;
        iter.unit=m_command_map[command_name]->unit;
        iter.location_mark=m_command_map[command_name]->description;
    }
    else if(iter.data_record.size()>=MAX_DATA_ITEMS)
    {
        iter.data_record.erase(std::begin(iter.data_record));
    }
    struct sensor_data insert_data;
    struct timeval now;
    gettimeofday(&now,NULL);
    insert_data.time=now.tv_sec;
    memcpy(insert_data.data.data_char,_data,data_size);
    if(data_size==2)insert_data.data.data_int=(short)insert_data.data.data_int;
    iter.data_record.emplace_back(std::move(insert_data));
}
void Sensor_Task::Handle_Data(command_ptr cmd)
{
    std::this_thread::sleep_for(std::chrono::microseconds(20000));
    unsigned char recv_buf[16]={0};
#ifndef SENSOR_TEST
    int recv_size=serial_receive(m_serial_fd,recv_buf,16);
    if(recv_size>0)
    {
        std::cout<<"recv:";
        for(int i=0;i<recv_size;i++)printf("%02x ",recv_buf[i]);
        std::cout<<std::endl;
        unsigned char recv_data[16]={0};
        bool s_find=false;
        if(recv_size<cmd->handle_size)return;
        int key_data_count=0;
        for(int i=0;i<cmd->handle_size;i++)
        {
            if(cmd->handle_rule[i]!=recv_buf[i])
            {
                if(cmd->handle_rule[i]==KEY_DATA)
                {
                    recv_data[key_data_count++]=recv_buf[i++];
                    while(i<cmd->handle_size&&cmd->handle_rule[i]==KEY_DATA)
                    {
                        recv_data[key_data_count++]=recv_buf[i++];
                    }
                    s_find=true;
                    if(m_big_endian)
                    {
                        unsigned char tmp;
                        int offset=0;
                        while(offset<key_data_count/2)
                        {
                            tmp=recv_data[offset];
                            recv_data[offset]=recv_data[key_data_count-offset-1];
                            recv_data[key_data_count-offset-1]=tmp;
                            offset++;
                        }
                    }
                    s_find=true;
                    break;
                }
                else if(cmd->handle_rule[i]==IGNORE)
                {
                    continue;
                }
                else break;
            }
        }
        if(s_find)
        {
            Insert_Record_To_List(cmd->command_name,recv_data,key_data_count);
        }
    }
#else
    std::random_device rd;
    recv_buf[0]=rd()&0xFF;
    recv_buf[1]=rd()&0xFF;
    Insert_Record_To_List(cmd->command_name,recv_buf);
#endif

}

void Sensor_Task::start()
{
    if(!m_init_success)
    {
        throw "must call start() after call Sensor_Task_Init() success!";
    }
    if(!m_task_brun)
    {
        m_task_brun=true;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_thread.reset(new std::thread(&Sensor_Task::run,this));
        //m_thread->join();
        m_thread->detach();
    }
}

void Sensor_Task::stop()
{
    m_task_brun=false;
}

void Sensor_Task::run()
{
    std::cout<<"Sensor_Task thread run!"<<std::endl;
    auto timeBegin = std::chrono::high_resolution_clock::now();
    int64_t elapsed = 0;
    while(m_task_brun)
    {
        timeBegin = std::chrono::high_resolution_clock::now();
        task();
        elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - timeBegin).count();
        if (elapsed < 0)
            elapsed = 0;
        std::this_thread::sleep_for(std::chrono::microseconds(RUN_INTERVAL - elapsed));
    }
    std::cout<<"Sensor_Task thread exit()!"<<std::endl;
}
void Sensor_Task::task()
{
    std::unordered_map<std::string,command_ptr> tmp_map;
    {//拷贝指令map
        std::lock_guard<std::mutex> lock(m_mutex);
        tmp_map=m_command_map;
    }
    unsigned char tmp[16];
    for(auto i : tmp_map)
    {
        command_ptr command=i.second;
        if(!command->in_use||m_command_set.find(command->command_name)==m_command_set.end())continue;
#ifndef SENSOR_TEST
        //while(serial_receive(m_serial_fd,tmp,16)>0)continue;//清空缓冲区
        std::cout<<command->command_name<<" send :";
        for(int i=0;i<command->send_size;i++)printf("%02x ",command->send_data[i]);
        std::cout<<std::endl;
        if(serial_send(m_serial_fd,command->send_data,command->send_size)>0)
        {
            Handle_Data(command);
        }
#else
        Handle_Data(command);
#endif
        std::this_thread::sleep_for(std::chrono::microseconds(100000));
    }
}

Sensor_Task::~Sensor_Task()
{
    if(m_serial_fd>0)serial_close(m_serial_fd);
    stop();
    if(m_thread.get())m_thread->join();
}

Sensor_Task::Sensor_Task(std::string config_name)
{
    m_config_name=CONFIG_FILE_PATH;
    m_config_name+=config_name;
    FILE *fp=fopen(m_config_name.c_str(),"r");
    if(!fp)return;
    if (fseek(fp, 0, SEEK_END) != 0)return;
    int length=ftell(fp);
    if(length<0)return;
    if (fseek(fp, 0, SEEK_SET) != 0)return;
    char *read_buf=(char *)calloc(sizeof(char),length+1);
    int read_len=fread(read_buf,1,length,fp);
    if(read_len!=length)
    {
        free(read_buf);
        return;
    }
    m_CJSON.reset(new CJSON_ptr(CJSON_Parse(read_buf)));
    CJSON *config=m_CJSON->_get();
    CJSON *device_name=CJSON_GetObjectItemCaseSensitive(config,"device_name");
    if(!device_name)
    {
        std::cout<<"device_name can't find!"<<std::endl;
        return;
    }
    m_config_name=device_name->valuestring;
    CJSON *big_endian=CJSON_GetObjectItemCaseSensitive(config,"big_endian");
    if(!big_endian)
    {
        std::cout<<"big_endian can't find!"<<std::endl;
        return;
    }
    m_big_endian=big_endian->type==CJSON_True?true:false;
    CJSON *command_list=CJSON_GetObjectItemCaseSensitive(config,"command_list");
    if(!command_list)
    {
        std::cout<<"command_list can't find!"<<std::endl;
        return;
    }
    const char s_command_list[Sensor_Task::MAX_COMMAND][20]={
        "temperature",//温度
        "humidity",//湿度
        "pm2.5",
        "co2",
        "Illumination",//光照度
        "pm10",
        "pressure",//大气压
        "noise",//噪声
    };
    for(int i=0;i<Sensor_Task::MAX_COMMAND;i++)
    {
        m_command_set.insert(s_command_list[i]);
    }
    CJSON *child=command_list->child;
    while(child)
    {
        std::string command_name=child->string;
        command_ptr tmp(new(struct command));
        struct command *tmp_command=tmp.get();
        tmp_command->command_name=command_name;
        do{
            CJSON *in_use=CJSON_GetObjectItemCaseSensitive(child,"in_use");
            if(!in_use)break;
            tmp_command->in_use=in_use->type==CJSON_True?true:false;
            CJSON *send_data=CJSON_GetObjectItemCaseSensitive(child,"send_data");
            if(!send_data)break;
            tmp_command->send_size=0;
            CJSON *array_item=send_data->child;
            while(array_item)
            {
                tmp_command->send_data[tmp_command->send_size]=array_item->valuedouble;
                tmp_command->send_size++;
                array_item=array_item->next;
            }
            CJSON *handle_rule=CJSON_GetObjectItemCaseSensitive(child,"handle_rule");
            if(!handle_rule)break;
            tmp_command->handle_size=0;
            array_item=handle_rule->child;
            while(array_item)
            {
                tmp_command->handle_rule[tmp_command->handle_size]=array_item->valuedouble;
                tmp_command->handle_size++;
                array_item=array_item->next;
            }
            CJSON *unit=CJSON_GetObjectItemCaseSensitive(child,"unit");
            if(!unit)break;
            tmp_command->unit=unit->valuestring;
            CJSON *description=CJSON_GetObjectItemCaseSensitive(child,"description");
            if(!description)break;
            tmp_command->description=description->valuestring;
            m_command_map[command_name]=tmp;
        }while(0);
        child=child->next;
    }
}
