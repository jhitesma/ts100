/********************* (C) COPYRIGHT 2015 e-Design Co.,Ltd. **********************
File Name :      CTRL.c
Version :        S100 APP Ver 2.11   
Description:
Author :         Celery
Data:            2015/08/03
History:
2015/07/07   ͳһ������
2015/08/03   �Ż��ƶ��ж�
*******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "CTRL.h"
#include "Bios.h"
#include "UI.h"
#include "HARDWARE.h"
#include "S100V0_1.h"
#include "Disk.h"
#include "MMA8652FC.h"

#define HEATINGCYCLE  30
/******************************************************************************/
DEVICE_INFO_SYS device_info;
/******************************************************************************/

u8 gCtrl_status = 1;
u16 gHt_flag = 0;
vs16 gTemp_data = 250;//25��
s16 gPrev_temp = 250; // ǰһ���¶�ֵ
u8 gIs_restartkey = 0;/*��������־*/
u8 gPre_status = 1;

const DEVICE_INFO_SYS info_def = {
    "2.11",     //Ver
    2000,       //T_Standby;    // 200��C=1800  2520,�����¶�
    3000,       // T_Work;      // 350��C=3362, �����¶�
    100,        //T_Step;
    3*60*100,   //Wait_Time;    //3*60*100   3  mintute
    6*60*100    // Idle_Time;   //6*60*100  6 minute
};
struct _pid {
    s16 settemp;        //�����趨�¶�
    s16 actualtemp;     //����ʵ���¶�
    s16 err;            //�����¶Ȳ�ֵ
    s16 err_last;       //������һ���¶Ȳ�ֵ
    s32 ht_time;        //�������ʱ��
    u16 kp,ki,kd;       //������������֡�΢��ϵ��
    s32 integral;       //�������ֵ
} pid;

/*******************************************************************************
������: Get_Ctrl_Status
��������:��ȡ��ǰ״̬
�������:��
���ز���:��ǰ״̬
*******************************************************************************/
u8 Get_CtrlStatus(void)
{
    return gCtrl_status;
}
/*******************************************************************************
������: Set_CtrlStatus
��������:���õ�ǰ״̬
�������:status ���õ�״̬
���ز���:��
*******************************************************************************/
void Set_CtrlStatus(u8 status)
{
    gCtrl_status = status;
}
/*******************************************************************************
������: Set_PrevTemp
��������:����ǰһ�¶�
�������:temp ǰһ�¶�ֵ
���ز���:�� 
*******************************************************************************/
void Set_PrevTemp(s16 temp)
{
    gPrev_temp = temp;
}

/*******************************************************************************
������: Get_HtFlag
��������:��ȡ��ǰ���ȱ�־,���ȱ�־�ɼ���ʱ�����
�������:NULL
���ز���:��ǰ���ȱ�־
*******************************************************************************/
u16 Get_HtFlag(void)
{
    return gHt_flag;
}
/*******************************************************************************
������: Get_TempVal
��������: ��ȡ��ǰ�¶ȵ�ֵ
�������:NULL
���ز���:��ǰ�¶�
*******************************************************************************/
s16 Get_TempVal(void)
{
    return gTemp_data;
}

/*******************************************************************************
������: System_Init
��������: ϵͳ��ʼ��
�������:NULL
���ز���:NULL
*******************************************************************************/
void System_Init(void)
{
    memcpy((void*)&device_info,(void*)&info_def,sizeof(device_info));
}
/*******************************************************************************
������: PID_init 
��������: PID���ݳ�ʼ��
�������:NULL
���ز���:NULL 
*******************************************************************************/
void Pid_Init(void)
{
    pid.settemp     = 0;
    pid.actualtemp  = 0;
    pid.err         = 0;
    pid.err_last    = 0;
    pid.integral    = 0;
    pid.ht_time     = 0;
    pid.kp          = 15;
    pid.ki          = 2;
    pid.kd          = 1;
}

/*******************************************************************************
������: Pid_Realize 
��������: PID����������ʱ��
�������:temp��ǰ�¶�
���ز���:�������ݵ�λ/50us 
*******************************************************************************/
u16 Pid_Realize(s16 temp)
{
    u8 index = 0,index1 = 1;
    s16 d_err = 0;

    pid.actualtemp   = temp;
    pid.err          = pid.settemp - pid.actualtemp;    //�²�

    if(pid.err >= 500)  index = 0;
    else {
        index = 1;
        pid.integral    += pid.err;//������
    }
////////////////////////////////////////////////////////////////////////////////
//����ȥ����
    if(pid.settemp < pid.actualtemp) {
      d_err = pid.actualtemp - pid.settemp;
      if(d_err > 20){
          pid.integral = 0; //����5��
          index1 = 0;
          index = 0;
      }
    }
////////////////////////////////////////////////////////////////////////////////
    if(pid.err <= 30) index1 = 0;
    else index1 = 1;
    pid.ht_time     = pid.kp * pid.err + pid.ki * index * pid.integral + pid.kd * (pid.err - pid.err_last)*index1;
    pid.err_last    = pid.err;

    if(pid.ht_time <= 0)          pid.ht_time = 0;
    else if(pid.ht_time > 30*200) pid.ht_time = 30*200;
    
    return pid.ht_time;

}

/*******************************************************************************
������: Heating_Time 
��������: ������ȱ�־�����ؼ���ʱ��
�������:temp��ǰ�¶ȣ�wk_temp �����¶�
���ز���:�������ݵ�λ/50us 
*******************************************************************************/
u32 Heating_Time(s16 temp,s16 wk_temp)
{
    u32 heat_timecnt;

    pid.settemp = wk_temp;
    if(wk_temp > temp) {
        if(wk_temp - temp >= 18)gHt_flag = 0;//����
        else gHt_flag = 2;//����
    } else {
        if(temp - wk_temp <= 18)gHt_flag = 2;//����
        else gHt_flag = 1;//����
    }

    heat_timecnt = Pid_Realize(temp);//Sub_data * 1000;

    return heat_timecnt;
}
/*******************************************************************************
������: Status_Tran 
��������: ���ݰ������¶��жϵȿ���״̬ת��
�������: NULL
���ز���: NULL
*******************************************************************************/
void Status_Tran(void)//״̬ת��
{
    static u16 init_waitingtime = 0;//��ʼ����ʱ���־λ: 0=> δ��ʼ��,1=>�ѳ�ʼ��
    static u8 back_prestatus = 0;
    s16 heat_timecnt = 0,wk_temp;
    u16 mma_active;
    
    switch (Get_CtrlStatus()) {
    case IDLE:
        switch(Get_gKey()) {
        case KEY_V1:
            if(gIs_restartkey != 1) {
                	if(Read_Vb(1) < 4) {
                	Set_CtrlStatus(TEMP_CTR);
                	init_waitingtime    = 0;
                	TEMPSHOW_TIMER  = 0;
                  UI_TIMER = 0;
                  G6_TIMER = 0;
                }
            }
            break;
        case KEY_V2:
            if(gIs_restartkey != 1) {
                Set_CtrlStatus(THERMOMETER);
                UI_TIMER = 0;
                Set_LongKeyFlag(1);
            }
            break;
        case KEY_CN|KEY_V3:
            break;
        }
        if(gIs_restartkey && (KD_TIMER == 0)) {
            gIs_restartkey = 0;
            Set_gKey(NO_KEY);
        }
        if(Read_Vb(1) == 0){
            if(Get_UpdataFlag() == 1) Set_UpdataFlag(0);
            Set_CtrlStatus(ALARM);
        }
        if(gPre_status != WAIT && gPre_status != IDLE){
            G6_TIMER = device_info.idle_time;
            Set_gKey(NO_KEY);
            gPre_status = IDLE;
        }
        break;
    case TEMP_CTR:
        switch(Get_gKey()) {
        case KEY_CN|KEY_V1:
        case KEY_CN|KEY_V2:
            Set_HeatingTime(0);
            Set_CtrlStatus(TEMP_SET);
            HEATING_TIMER       = 0;
            EFFECTIVE_KEY_TIMER = 500;
            break;
        case KEY_CN|KEY_V3:
            Set_HeatingTime(0);
            Set_LongKeyFlag(0);
            Set_CtrlStatus(IDLE);
            gPre_status = TEMP_CTR;
            gIs_restartkey = 1;
            KD_TIMER = 50; //
            break;
        }
        
        if(Read_Vb(1) >= 4) {
            Set_HeatingTime(0);
            Set_LongKeyFlag(0);
            Set_CtrlStatus(IDLE);
            gPre_status = TEMP_CTR;
            gIs_restartkey = 1;
            KD_TIMER = 50; // 2��
        }
        
        wk_temp = device_info.t_work;
        if(HEATING_TIMER == 0) {
            gTemp_data    = Get_Temp(wk_temp);
            heat_timecnt  = Heating_Time(gTemp_data,wk_temp);  //�������ʱ��
            Set_HeatingTime(heat_timecnt);
            HEATING_TIMER = HEATINGCYCLE;
        }
        if(Get_HeatingTime() == 0) {
            HEATING_TIMER = 0;
        }


        mma_active = Get_MmaShift();
        if(mma_active == 0) { //MMA_active = 0 ==> static ,MMA_active = 1 ==>move
            if(init_waitingtime == 0) {
                init_waitingtime    = 1;
                ENTER_WAIT_TIMER = device_info.wait_time;
            }
            if((init_waitingtime != 0) && (ENTER_WAIT_TIMER == 0)) {
                gHt_flag      = 0;
                UI_TIMER     = 0;
                Set_HeatingTime(0);
                Set_gKey(0);
                G6_TIMER = device_info.idle_time;
                Set_CtrlStatus(WAIT);
            }
        } else {
            init_waitingtime = 0;
        }
        if(Get_AlarmType() > NORMAL_TEMP) {   //////////////////����
            if(Get_UpdataFlag() == 1) Set_UpdataFlag(0);
            Set_CtrlStatus(ALARM);
        }
        break;
    case WAIT:
        wk_temp = device_info.t_standby;
        if(device_info.t_standby > device_info.t_work) { //�����¶ȱȹ����¶ȸ�
            wk_temp = device_info.t_work;
        }
        if(HEATING_TIMER == 0) {
            gTemp_data    = Get_Temp(wk_temp);
            heat_timecnt  = Heating_Time(gTemp_data,wk_temp);  //�������ʱ��
            Set_HeatingTime(heat_timecnt);
            HEATING_TIMER = 30;
        }
        
        if(Read_Vb(1) >= 4) {
            Set_HeatingTime(0);
            Set_LongKeyFlag(0);
            Set_CtrlStatus(IDLE);
            G6_TIMER = device_info.idle_time;
            gPre_status = WAIT;
            gIs_restartkey = 1;
            KD_TIMER = 50; // 2��
        }
        
        if(G6_TIMER == 0) { //�������
            Set_HeatingTime(0);
            Set_LongKeyFlag(0);
            gIs_restartkey = 1;
            KD_TIMER = 200; // 2��
            gPre_status = WAIT;
            Set_CtrlStatus(IDLE);
        }
        
        mma_active = Get_MmaShift();
        if(mma_active == 1 || Get_gKey() != 0) {
            UI_TIMER      = 0;
            G6_TIMER      = 0;
            init_waitingtime = 0;
            Set_CtrlStatus(TEMP_CTR);
        }
        
        if(Get_AlarmType() > NORMAL_TEMP) {   //////////////////����
            if(Get_UpdataFlag() == 1) Set_UpdataFlag(0);
            Set_CtrlStatus(ALARM);
        }
        break;
    case TEMP_SET:
        if(EFFECTIVE_KEY_TIMER == 0) {
            Set_CtrlStatus(TEMP_CTR);
            TEMPSHOW_TIMER = 0;
        }
        break;
    case THERMOMETER:
        if(KD_TIMER > 0) {
            Set_gKey(NO_KEY);
            break;
        }
        switch(Get_gKey()) {
        case KEY_CN|KEY_V1:
        case KEY_CN|KEY_V2:
            back_prestatus = 1;
            break;
        case KEY_CN|KEY_V3:
            Zero_Calibration();
            if(Get_CalFlag() == 1) {
                Disk_BuffInit();
                Config_Analysis();         // ��������U��
            }
            KD_TIMER = 200; //20150717 �޸�
            break;
        default:
            break;
        }
        if(back_prestatus == 1) {
            back_prestatus = 0;
            Set_HeatingTime(0);
            Set_CtrlStatus(IDLE);
            gPre_status = THERMOMETER;
            gIs_restartkey = 1;
            Set_LongKeyFlag(0);
            KD_TIMER = 50; //
        }
        break;
    case ALARM:
        switch(Get_AlarmType()) {
        case HIGH_TEMP:
        case SEN_ERR:
            wk_temp     = device_info.t_work;
            gTemp_data  = Get_Temp(wk_temp);
            if(Get_AlarmType() == NORMAL_TEMP) {
                Set_CtrlStatus(TEMP_CTR);
                Set_UpdataFlag(0);
            }
            break;
        case HIGH_VOLTAGE:
        case LOW_VOLTAGE:
            if(Read_Vb(1) >= 1 && Read_Vb(1) <= 3) {
                Set_HeatingTime(0);
                Set_LongKeyFlag(0);
                gIs_restartkey = 1;
                UI_TIMER = 2; // 2��
                gPre_status = THERMOMETER;
                Set_CtrlStatus(IDLE);
            }
            break;
        }

        if(Get_HeatingTime != 0) {
            Set_HeatingTime(0) ;                          //����ֹͣ����
            HEAT_OFF();
        }
        break;
    default:
        break;
    }
}

/******************************** END OF FILE *********************************/