/**
 * @brief		
 * @details		
 * @date		2016-11-15
 **/

/* Includes ------------------------------------------------------------------*/
#include "meter.h"
#include "crc.h"
#include "trace.h"

#if defined ( _WIN32 ) || defined ( _WIN64 )
#include "comm_socket.h"
#include <windows.h>
#elif defined ( __linux )
#include <unistd.h>
#include <pthread.h>
#include "comm_socket.h"
#else

#if defined (DEMO_STM32F091)
#include "stm32f0xx.h"
#include "delay.h"
#include "vspi1.h"
#endif

#endif

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define devspi      vspi1

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static enum __dev_status status = DEVICE_NOTINIT;
static void(*meter_callback)(void *buffer) = (void(*)(void *))0;

#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )
static SOCKET sock = INVALID_SOCKET;
static volatile int32_t metering_data[42] = {0};
static volatile uint8_t updating = 0;
#endif

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )
#if defined ( __linux )
static void *ThreadRecvMail(void *arg)
#else
static DWORD CALLBACK ThreadRecvMail(PVOID pvoid)
#endif
{
    int32_t buff[42];
	int32_t recv_size;
    uint32_t j;
    
    while(1)
    {
#if defined ( __linux )
    	usleep(8*1000);
#else
    	Sleep(8);
#endif
    	
	    if(sock == INVALID_SOCKET)
	    {
	        continue;
	    }
	    
		recv_size = receiver.read(sock, (uint8_t *)buff, sizeof(buff));
		if (recv_size < (42 * 4))
		{
			continue;
		}

		updating = 0xff;

#if defined ( __linux )
    	usleep(2*1000);
#else
    	Sleep(2);
#endif

		if (status == DEVICE_INIT)
		{
			for (j = 0; j<(int)R_ESC; j++)
			{
				metering_data[j] += buff[j];
			}
		}

		for (j = (int)R_ESC; j<(int)R_FREQ; j++)
		{
			metering_data[j] = buff[j];
		}

		updating = 0;
    }
    
	return(0);
}

/**
  * @brief  模拟程序使用当前电压来判断是否启动
  */
uint8_t is_powered(void)
{
    //只要任何一相电压大于等于1V即可
	if(metering_data[R_UARMS - 1] >= 1000 || \
        metering_data[R_UBRMS - 1] >= 1000 || \
        metering_data[R_UCRMS - 1] >= 1000)
    {
        return(0xff);
    }
    
    return(0);
}
#endif

/**
  * @brief  
  */
static enum __dev_status meter_status(void)
{
    //获取当前设备状态
    return(status);
}

/**
  * @brief  
  */
static void meter_init(enum __dev_state state)
{
#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )
	sock = receiver.open(50002);

	if(sock == INVALID_SOCKET)
	{
		TRACE(TRACE_INFO, "Create receiver for metering failed.");
	}
    
    if(status == DEVICE_NOTINIT)
    {
#if defined ( __linux )
        pthread_t thread;
        pthread_attr_t thread_attr;
        
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&thread, &thread_attr, ThreadRecvMail, NULL);
        pthread_attr_destroy(&thread_attr);
#else
        HANDLE hThread;
        hThread = CreateThread(NULL, 0, ThreadRecvMail, 0, 0, NULL);
        CloseHandle(hThread);
#endif
    }
	
	meter_callback= (void(*)(void *))0;
	status = DEVICE_INIT;
#else

#if defined (DEMO_STM32F091)
    GPIO_InitTypeDef GPIO_InitStruct;
    
    meter_callback= (void(*)(void *))0;
    
    if(state == DEVICE_NORMAL)
	{
		devspi.control.init(state);
		
		if(devspi.control.status() != DEVICE_INIT)
		{
			status = DEVICE_ERROR;
			return;
		}
		
		//PD12 教表参数/事件输出
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOD, ENABLE);
		
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
		GPIO_Init(GPIOD, &GPIO_InitStruct);
        
		//PC5 Power
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE);
		
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
        GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Level_1;
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
		GPIO_Init(GPIOC, &GPIO_InitStruct);
        GPIO_ResetBits(GPIOC, GPIO_Pin_5);
        
        mdelay(10);
        
		//PE15 Reset
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE, ENABLE);
		
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
        GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Level_1;
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
		GPIO_Init(GPIOE, &GPIO_InitStruct);
        
        GPIO_ResetBits(GPIOE, GPIO_Pin_15);
        mdelay(1);
        GPIO_SetBits(GPIOE, GPIO_Pin_15);
        mdelay(10);
        
		status = DEVICE_INIT;
	}
#endif

#endif
}

/**
  * @brief  
  */
static void meter_suspend(void)
{
#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )
	if(sock != INVALID_SOCKET)
	{
		receiver.close(sock);
		sock = INVALID_SOCKET;
	}
#else
    
#if defined (DEMO_STM32F091)
    meter_callback= (void(*)(void *))0;
    devspi.control.suspend();
#endif
    
#endif
    
    status = DEVICE_SUSPENDED;
}

#if defined (DEMO_STM32F091)
/**
  * @brief  将 __metering_meta 转换为 702x 的命令字
  */
static uint8_t meter_cmd_translate(enum __metering_meta id)
{
    uint8_t result = 0xff;
    
    switch(id)
    {
        case R_EPA:
            result = 0x1E;
            break;
        case R_EPB:
            result = 0x1F;
            break;
        case R_EPC:
            result = 0x20;
            break;
        case R_EPT:
            result = 0x21;
            break;
        case R_EQA:
            result = 0x22;
            break;
        case R_EQB:
            result = 0x23;
            break;
        case R_EQC:
            result = 0x24;
            break;
        case R_EQT:
            result = 0x25;
            break;
        case R_ESA:
            result = 0x35;
            break;
        case R_ESB:
            result = 0x36;
            break;
        case R_ESC:
            result = 0x37;
            break;
        case R_EST:
            result = 0x38;
            break;
        case R_PA:
            result = 0x01;
            break;
        case R_PB:
            result = 0x02;
            break;
        case R_PC:
            result = 0x03;
            break;
        case R_PT:
            result = 0x04;
            break;
        case R_QA:
            result = 0x05;
            break;
        case R_QB:
            result = 0x06;
            break;
        case R_QC:
            result = 0x07;
            break;
        case R_QT:
            result = 0x08;
            break;
        case R_SA:
            result = 0x09;
            break;
        case R_SB:
            result = 0x0A;
            break;
        case R_SC:
            result = 0x0B;
            break;
        case R_ST:
            result = 0x0C;
            break;
        case R_PFA:
            result = 0x14;
            break;
        case R_PFB:
            result = 0x15;
            break;
        case R_PFC:
            result = 0x16;
            break;
        case R_PFT:
            result = 0x17;
            break;
        case R_UARMS:
            result = 0x0D;
            break;
        case R_UBRMS:
            result = 0x0E;
            break;
        case R_UCRMS:
            result = 0x0F;
            break;
        case R_IARMS:
            result = 0x10;
            break;
        case R_IBRMS:
            result = 0x11;
            break;
        case R_ICRMS:
            result = 0x12;
            break;
        case R_ITRMS:
            result = 0x13;
            break;
        case R_PGA:
            result = 0x18;
            break;
        case R_PGB:
            result = 0x19;
            break;
        case R_PGC:
            result = 0x1A;
            break;
        case R_YUAUB:
            result = 0x26;
            break;
        case R_YUAUC:
            result = 0x27;
            break;
        case R_YUBUC:
            result = 0x28;
            break;
        case R_FREQ:
            result = 0x1C;
            break;
    }
    
    return(result);
}
#endif

static void meter_runner(uint16_t msecond)
{
#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )

#else

#if defined (DEMO_STM32F091)

#endif

#endif
}



static int32_t meter_data_read(enum __metering_meta id)
{
#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )
    int32_t result;
    
    if(((uint16_t)id < (uint16_t)R_EPT) || ((uint16_t)id > (uint16_t)R_FREQ))
    {
        return(0);
    }
    
    if(updating)
    {
#if defined ( __linux )
        usleep(5*1000);
#else
        Sleep(5);
#endif
    }
    	
    switch(id)
    {
        case R_EPA:
        case R_EPB:
        case R_EPC:
        case R_EPT:
        case R_EQA:
        case R_EQB:
        case R_EQC:
        case R_EQT:
        case R_ESA:
        case R_ESB:
        case R_ESC:
        case R_EST:
            result = metering_data[(uint16_t)id - 1];
            metering_data[(uint16_t)id - 1] = 0;
            break;
        case R_PA:
        case R_PB:
        case R_PC:
        case R_PT:
        case R_QA:
        case R_QB:
        case R_QC:
        case R_QT:
        case R_SA:
        case R_SB:
        case R_SC:
        case R_ST:
        case R_PFA:
        case R_PFB:
        case R_PFC:
        case R_PFT:
        case R_UARMS:
        case R_UBRMS:
        case R_UCRMS:
        case R_IARMS:
        case R_IBRMS:
        case R_ICRMS:
        case R_ITRMS:
        case R_PGA:
        case R_PGB:
        case R_PGC:
        case R_YUAUB:
        case R_YUAUC:
        case R_YUBUC:
        case R_FREQ:
            result = metering_data[(uint16_t)id - 1];
            break;
        default:
            result = 0;
    }
    
    return(result);
#else
    
#if defined (DEMO_STM32F091)
    int32_t result;
    uint64_t val;
    uint8_t addr;
    
    if(status == DEVICE_INIT)
    {
        addr = meter_cmd_translate(id);
        devspi.select(0);
        devspi.octet.write(addr);
        udelay(10);
        val = devspi.octet.read();
        val <<= 8;
        val += devspi.octet.read();
        val <<= 8;
        val += devspi.octet.read();
        devspi.release(0);
        
		if((addr >= 0x0d) && (addr <= 0x0f))
		{
			val = val * 1000 / 0x2000;
		}
		else if((addr >= 0x10) && (addr <= 0x12))
		{
			val = val * 10000 / 0x1000;
			val /= 6 * 47 / 5;
			val = (uint64_t)((float)val * 1.5 / 1 + 0.5);
			val /= 10;
		}
		
        result = val;
    }
    else
    {
        result = 0;
    }
    
    return(result);
#endif
    
#endif
}



/**
  * @brief  
  */
static bool meter_calibrate_load(uint32_t size, const void *param)
{
#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )
	if(size < sizeof(struct __calibrate_data))
	{
		return(false);
	}
	
	if(((struct __calibrate_data *)param)->check != crc32(param, (sizeof(struct __calibrate_data) - sizeof(uint32_t))))
	{
		return(false);
	}
	
	return(true);
#else

#if defined (DEMO_STM32F091)
    uint8_t loop;
    
	if(size < sizeof(struct __calibrate_data))
	{
		return(false);
	}
	
	if(((struct __calibrate_data *)param)->check != crc32(param, (sizeof(struct __calibrate_data) - sizeof(uint32_t))))
	{
		return(false);
	}
    
    //清校表数据
    devspi.select(0);
    devspi.octet.write(0x80 + 0xc3);
    devspi.octet.write(0);
    devspi.octet.write(0);
    devspi.octet.write(0);
    devspi.release(0);
    
    //写校表数据
    for(loop=0; loop<59; loop++)
    {
        devspi.select(0);
        devspi.octet.write(0x80 + (((struct __calibrate_data *)param)->reg[loop].address & 0x7f));
        devspi.octet.write(0);
        devspi.octet.write((((struct __calibrate_data *)param)->reg[loop].value >> 8) & 0xff);
        devspi.octet.write((((struct __calibrate_data *)param)->reg[loop].value >> 0) & 0xff);
        devspi.release(0);
    }
	
	return(true);
#endif

#endif
}

/**
  * @brief  
  */
static bool meter_calibrate_enter(uint32_t size, void *args)
{
#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )
	if(size < sizeof(struct __calibrates))
	{
		return(false);
	}
	
	memset((void *)&(((struct __calibrates *)args)->data), \
           0, \
           sizeof(((struct __calibrates *)args)->data));
    
	((struct __calibrates *)args)->data.check = crc32((const void *)&(((struct __calibrates *)args)->data), \
                                                      sizeof(((struct __calibrates *)args)->data) - sizeof(uint32_t));
	return(true);
#else

#if defined (DEMO_STM32F091)
	if(size < sizeof(struct __calibrates))
	{
		return(false);
	}
	
	memset((void *)&(((struct __calibrates *)args)->data), \
           0, \
           sizeof(((struct __calibrates *)args)->data));
	
	((struct __calibrates *)args)->data.reg[0].address = 0x13;
	((struct __calibrates *)args)->data.reg[0].value = 0x0000;
	((struct __calibrates *)args)->data.reg[1].address = 0x14;
	((struct __calibrates *)args)->data.reg[1].value = 0x0000;
	((struct __calibrates *)args)->data.reg[2].address = 0x15;
	((struct __calibrates *)args)->data.reg[2].value = 0x0000;
	((struct __calibrates *)args)->data.reg[3].address = 0x21;
	((struct __calibrates *)args)->data.reg[3].value = 0x0000;
	((struct __calibrates *)args)->data.reg[4].address = 0x22;
	((struct __calibrates *)args)->data.reg[4].value = 0x0000;
	((struct __calibrates *)args)->data.reg[5].address = 0x23;
	((struct __calibrates *)args)->data.reg[5].value = 0x0000;
	((struct __calibrates *)args)->data.reg[6].address = 0x24;
	((struct __calibrates *)args)->data.reg[6].value = 0x0000;
	((struct __calibrates *)args)->data.reg[7].address = 0x25;
	((struct __calibrates *)args)->data.reg[7].value = 0x0000;
	((struct __calibrates *)args)->data.reg[8].address = 0x26;
	((struct __calibrates *)args)->data.reg[8].value = 0x0000;
	((struct __calibrates *)args)->data.reg[9].address = 0x27;
	((struct __calibrates *)args)->data.reg[9].value = 0x0007;
	
	((struct __calibrates *)args)->data.reg[10].address = 0x28;
	((struct __calibrates *)args)->data.reg[10].value = 0x0007;
	((struct __calibrates *)args)->data.reg[11].address = 0x29;
	((struct __calibrates *)args)->data.reg[11].value = 0x0007;
	((struct __calibrates *)args)->data.reg[12].address = 0x2a;
	((struct __calibrates *)args)->data.reg[12].value = 0x0000;
	((struct __calibrates *)args)->data.reg[13].address = 0x2b;
	((struct __calibrates *)args)->data.reg[13].value = 0x0000;
	((struct __calibrates *)args)->data.reg[14].address = 0x2c;
	((struct __calibrates *)args)->data.reg[14].value = 0x0000;
	((struct __calibrates *)args)->data.reg[15].address = 0x2d;
	((struct __calibrates *)args)->data.reg[15].value = 0x0000;
	((struct __calibrates *)args)->data.reg[16].address = 0x2e;
	((struct __calibrates *)args)->data.reg[16].value = 0x0000;
	((struct __calibrates *)args)->data.reg[17].address = 0x2f;
	((struct __calibrates *)args)->data.reg[17].value = 0x0000;
	((struct __calibrates *)args)->data.reg[18].address = 0x04;
	((struct __calibrates *)args)->data.reg[18].value = 0x3f82;
	((struct __calibrates *)args)->data.reg[19].address = 0x05;
	((struct __calibrates *)args)->data.reg[19].value = 0x3fb1;
	
	((struct __calibrates *)args)->data.reg[20].address = 0x06;
	((struct __calibrates *)args)->data.reg[20].value = 0x4031;
	((struct __calibrates *)args)->data.reg[21].address = 0x07;
	((struct __calibrates *)args)->data.reg[21].value = 0x3f82;
	((struct __calibrates *)args)->data.reg[22].address = 0x08;
	((struct __calibrates *)args)->data.reg[22].value = 0x3fb1;
	((struct __calibrates *)args)->data.reg[23].address = 0x09;
	((struct __calibrates *)args)->data.reg[23].value = 0x4031;
	((struct __calibrates *)args)->data.reg[24].address = 0x0a;
	((struct __calibrates *)args)->data.reg[24].value = 0x3f82;
	((struct __calibrates *)args)->data.reg[25].address = 0x0b;
	((struct __calibrates *)args)->data.reg[25].value = 0x3fb1;
	((struct __calibrates *)args)->data.reg[26].address = 0x0c;
	((struct __calibrates *)args)->data.reg[26].value = 0x4031;
	((struct __calibrates *)args)->data.reg[27].address = 0x0d;
	((struct __calibrates *)args)->data.reg[27].value = 0xffef;
	((struct __calibrates *)args)->data.reg[28].address = 0x0e;
	((struct __calibrates *)args)->data.reg[28].value = 0xfffa;
	((struct __calibrates *)args)->data.reg[29].address = 0x0f;
	((struct __calibrates *)args)->data.reg[29].value = 0xfff4;
	
	((struct __calibrates *)args)->data.reg[30].address = 0x10;
	((struct __calibrates *)args)->data.reg[30].value = 0xffee;
	((struct __calibrates *)args)->data.reg[31].address = 0x11;
	((struct __calibrates *)args)->data.reg[31].value = 0xfffb;
	((struct __calibrates *)args)->data.reg[32].address = 0x12;
	((struct __calibrates *)args)->data.reg[32].value = 0xfffa;
	((struct __calibrates *)args)->data.reg[33].address = 0x17;
	((struct __calibrates *)args)->data.reg[33].value = 0xcbcc;
	((struct __calibrates *)args)->data.reg[34].address = 0x18;
	((struct __calibrates *)args)->data.reg[34].value = 0xcbf5;
	((struct __calibrates *)args)->data.reg[35].address = 0x19;
	((struct __calibrates *)args)->data.reg[35].value = 0xcbf0;
	((struct __calibrates *)args)->data.reg[36].address = 0x1a;
	((struct __calibrates *)args)->data.reg[36].value = 0x7aa7;
	((struct __calibrates *)args)->data.reg[37].address = 0x1b;
	((struct __calibrates *)args)->data.reg[37].value = 0x7a53;
	((struct __calibrates *)args)->data.reg[38].address = 0x1c;
	((struct __calibrates *)args)->data.reg[38].value = 0x7afa;
	((struct __calibrates *)args)->data.reg[39].address = 0x20;
	((struct __calibrates *)args)->data.reg[39].value = 0x5160;
	
	((struct __calibrates *)args)->data.reg[40].address = 0x01;
	((struct __calibrates *)args)->data.reg[40].value = 0xbd7f;
	((struct __calibrates *)args)->data.reg[41].address = 0x02;
	((struct __calibrates *)args)->data.reg[41].value = 0x0100;
	((struct __calibrates *)args)->data.reg[42].address = 0x03;
	((struct __calibrates *)args)->data.reg[42].value = 0x79c4;
	((struct __calibrates *)args)->data.reg[43].address = 0x16;
	((struct __calibrates *)args)->data.reg[43].value = 0x0000;
	((struct __calibrates *)args)->data.reg[44].address = 0x1d;
	((struct __calibrates *)args)->data.reg[44].value = 0x0160;
	((struct __calibrates *)args)->data.reg[45].address = 0x1e;
	((struct __calibrates *)args)->data.reg[45].value = 0x0078;
	((struct __calibrates *)args)->data.reg[46].address = 0x1f;
	((struct __calibrates *)args)->data.reg[46].value = 0x14a0;
	((struct __calibrates *)args)->data.reg[47].address = 0x30;
	((struct __calibrates *)args)->data.reg[47].value = 0x0001;
	((struct __calibrates *)args)->data.reg[48].address = 0x31;
	((struct __calibrates *)args)->data.reg[48].value = 0x3c37;
	((struct __calibrates *)args)->data.reg[49].address = 0x32;
	((struct __calibrates *)args)->data.reg[49].value = 0x0000;
	
	((struct __calibrates *)args)->data.reg[50].address = 0x33;
	((struct __calibrates *)args)->data.reg[50].value = 0x0000;
	((struct __calibrates *)args)->data.reg[51].address = 0x35;
	((struct __calibrates *)args)->data.reg[51].value = 0x000f;
	((struct __calibrates *)args)->data.reg[52].address = 0x36;
	((struct __calibrates *)args)->data.reg[52].value = 0x0033;
	((struct __calibrates *)args)->data.reg[53].address = 0x37;
	((struct __calibrates *)args)->data.reg[53].value = 0x0000;
	((struct __calibrates *)args)->data.reg[54].address = 0x70;
	((struct __calibrates *)args)->data.reg[54].value = 0x0002;
	((struct __calibrates *)args)->data.reg[55].address = 0x6d;
	((struct __calibrates *)args)->data.reg[55].value = 0xFF00;
	((struct __calibrates *)args)->data.reg[56].address = 0x6e;
	((struct __calibrates *)args)->data.reg[56].value = 0x0DB8;
	((struct __calibrates *)args)->data.reg[57].address = 0x6f;
	((struct __calibrates *)args)->data.reg[57].value = 0xD1DA;
	((struct __calibrates *)args)->data.reg[58].address = 0x6b;
	((struct __calibrates *)args)->data.reg[58].value = 0x0000;
	
	((struct __calibrates *)args)->data.check = crc32((const void *)&(((struct __calibrates *)args)->data), \
                                                      sizeof(((struct __calibrates *)args)->data) - sizeof(uint32_t));
	
    return(true);
#endif

#endif
}

/**
  * @brief  
  */
static bool meter_calibrate_status(void)
{
#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )
	return(true);
#else

#if defined (DEMO_STM32F091)
	return(true);
#endif

#endif
}

/**
  * @brief  
  */
static bool meter_calibrate_exit(void)
{
#if defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( __linux )
	return(true);
#else

#if defined (DEMO_STM32F091)
	return(true);
#endif

#endif
}



/**
  * @brief  handler 用于向外抛出异常
  */
static void meter_handler_filling(void(*callback)(void *buffer))
{
	if(callback)
    {
        meter_callback = callback;
    }
}

static void meter_handler_remove(void)
{
    meter_callback= (void(*)(void *))0;
}










/**
  * @brief  
  */
const struct __meter meter = 
{
    .control        = 
    {
        .name       = "att7022e",
        .status     = meter_status,
        .init       = meter_init,
        .suspend    = meter_suspend,
    },
    
    .runner         = meter_runner,
    
    .read           = meter_data_read,
    
    .calibrate      = 
    {
        .load       = meter_calibrate_load,
        .enter      = meter_calibrate_enter,
        .status     = meter_calibrate_status,
        .exit       = meter_calibrate_exit,
    },
    
    .handler		= 
    {
		.filling	= meter_handler_filling,
		.remove		= meter_handler_remove,
    },
};