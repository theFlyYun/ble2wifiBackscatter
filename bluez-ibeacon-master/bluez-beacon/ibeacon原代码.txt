// 用于蓝牙广播的程序，主要功能是开启蓝牙广播并发送指定的广播数据
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

// 宏函数cmd_opcode_pack，将操作组织者字段OGF,操作字段OCF打包成一个16 bit的命令操作码 (无符号整数类型)
// ocf & 0x03ff表示将操作字段的前10位（0x03ff为10位全1的二进制数）取出，忽略其他位的值。
// 然后将这10位OCF与OGF左移10位后的值进行按位或运算，得到一个16位的命令操作码
// 作用是将OGF和OCF组合成一个唯一标识某个蓝牙命令的值，方便后续使用这个值进行蓝牙通讯
#define cmd_opcode_pack(ogf, ocf) (uint16_t)((ocf & 0x03ff)|(ogf << 10)) 

#define EIR_FLAGS                   0X01 //设备的标志位
#define EIR_NAME_SHORT              0x08 //设备的短名称
#define EIR_NAME_COMPLETE           0x09 //设备的完整名称
#define EIR_MANUFACTURE_SPECIFIC    0xFF //表示设备的制造商自定义数据

int global_done = 0; //全局变量，用于判断是否接收到终止广播的信号

//辅助函数uuid_str_to_data,用于将字符串形式的UUID转换为整型数组形式的UUID,并返回整型数组的指针
// UUID是一种用于标识设备或服务的唯一标识符,函数将UUID字符串根据“-”分割成5个部分,然后将每个部分转换为整型数值,并存储到一个整型数组中
unsigned int *uuid_str_to_data(char *uuid)  // 函数的参数是一个指向字符数组的指针,表示需要转换的UUID字符串
{
  char conv[] = "0123456789ABCDEF"; //定义一个字符数组 conv，包含十六进制的字符
  int len = strlen(uuid); //获取UUID字符串的长度
  unsigned int *data = (unsigned int*)malloc(sizeof(unsigned int) * len); //根据长度动态分配了一个无符号整型数组的内存空间
  unsigned int *dp = data; // 指针dp，指向data的起始位置
  char *cu = uuid; // 指针cu，指向UUID字符串的起始位置

  for(; cu<uuid+len; dp++,cu+=2) //循环来历UUID字符串，每次循环 dp 指针向后移动一位（指向下一个整型数值），cu 指针每次移动两个位置（跳过 "-"）
  {
    *dp = ((strchr(conv, toupper(*cu)) - conv) * 16) + (strchr(conv, toupper(*(cu+1))) - conv);
    //循环中，通过 strchr 函数查找当前字符在 conv 数组中的位置，并计算出对应的十六进制数值。
    // 然后将这两个数值相加，得到最终的整型数值，并将其存储到 dp 指针指向的位置
  }
  return data; // 函数返回一个指向无符号整型数组的指针,存储了转换后的整型数值
}

//辅助函数twoc，用于将一个有符号整数转换为二进制补码形式(无符号整数)
unsigned int twoc(int in, int t) // 函数的输入参数为一个有符号整数 in，以及一个整数 t(位数)
{
  return (in < 0) ? (in + (2 << (t-1))) : in; // 返回值为无符号整数
}

// 函数enable_advertising，用于使能蓝牙广播
// 函数的输入参数有广告间隔、广播 UUID、Major号、Minor号和 RSSI值
int enable_advertising(int advertising_interval, char *advertising_uuid, int major_number, int minor_number, int rssi_value)
{
  int device_id = hci_get_route(NULL);  //获取蓝牙设备ID

  int device_handle = 0;
  if((device_handle = hci_open_dev(device_id)) < 0) //打开蓝牙设备
  {
    perror("Could not open device"); //如果打开失败(设备ID的返回值<0),那么perror函数打印错误信息
    exit(1); //调用 exit 函数退出程序，并返回状态码1
  }

  //设置广播参数，包括广播时间间隔、信道映射等
  le_set_advertising_parameters_cp adv_params_cp; // 定义adv_params_cp结构体变量，用于存储广播参数。结构体类型为 le_set_advertising_parameters_cp
  memset(&adv_params_cp, 0, sizeof(adv_params_cp)); // memset函数将 adv_params_cp 变量的内存空间清零，确保结构体中的所有成员变量都被初始化为0
  adv_params_cp.min_interval = htobs(advertising_interval); // 调用 htobs 函数将advertising_interval转换为对应的字节序，并赋值给 adv_params_cp.min_interval
  adv_params_cp.max_interval = htobs(advertising_interval); // 调用 htobs 函数将advertising_interval转换为对应的字节序，并赋值给 adv_params_cp.max_interval
  adv_params_cp.chan_map = 7; // 信道映射。设置为 7，表示使用所有的信道进行广播


  // 通过发送 HCI 请求，将广告参数设置到蓝牙设备中，并通过 status 变量获取操作的结果状态
  uint8_t status; // 保存状态信息
  struct hci_request rq; // 定义结构体,用于保存 HCI 请求的相关信息
  memset(&rq, 0, sizeof(rq)); // 初始化结构体内所有成员变量为0
  rq.ogf = OGF_LE_CTL; // 表示请求的操作组代码
  rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS; // 表示请求的操作命令代码
  rq.cparam = &adv_params_cp; // 结构体的地址
  rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE; // 广告参数的大小
  rq.rparam = &status; // 结果状态的地址
  rq.rlen = 1; // 表示结果状态的大小为1字节

  int ret = hci_send_req(device_handle, &rq, 1000); // hci_send_req函数负责向蓝牙设备发送HCI请求,ret保存发送请求的结果

  if (ret < 0) // ret<0，则表示发送请求失败
  {
    hci_close_dev(device_handle); //调用 hci_close_dev 函数关闭蓝牙设备的连接
    fprintf(stderr, "Can't send request %s (%d)\n", strerror(errno), errno); // fprintf 函数输出错误信息，包括错误描述和错误代码
    return(1); // 函数返回 1 表示失败
  }


  //发送一个 HCI 请求来使能蓝牙广播，并获取结果状态
  le_set_advertise_enable_cp advertise_cp;  // 定义advertise_cp结构体变量,用于保存蓝牙广播使能,结构体类型为 le_set_advertise_enable_cp       
  memset(&advertise_cp, 0, sizeof(advertise_cp)); // 将 advertise_cp 的所有成员变量初始化为 0
  advertise_cp.enable = 0x01;

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
  rq.cparam = &advertise_cp;
  rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  ret = hci_send_req(device_handle, &rq, 1000);// 通过hci_send_req函数发送请求

  if (ret < 0)
  {
    hci_close_dev(device_handle);
    fprintf(stderr, "Can't send request %s (%d)\n", strerror(errno), errno);
    return(1);
  }

  // 设置广播数据，包括广播数据段的长度、类型和内容。包括广播标志、制造商特定数据、UUID、major和minor号以及RSSI校准
  le_set_advertising_data_cp adv_data_cp;  // 定义adv_data_cp结构体变量,用于设置广播数据的内容,结构体类型为 le_set_advertising_data_cp    
  memset(&adv_data_cp, 0, sizeof(adv_data_cp)); // 将 adv_data_cp 的所有成员变量初始化为 0

  uint8_t segment_length = 1;  // 定义名为 segment_length 的变量，用于保存每个段的长度。初始化为 1
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(EIR_FLAGS); segment_length++; // 设置 Flags 段的内容,将Flags的值保存在adv_data_cp.data数组中,更新segment_length
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x1A); segment_length++;
  adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1);

  adv_data_cp.length += segment_length;

  segment_length = 1;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(EIR_MANUFACTURE_SPECIFIC); segment_length++; //将Manufacturer Specific数据的值保存在adv_data_cp.data数组中,更新segment_length
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x4C); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x00); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x02); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x15); segment_length++;

  unsigned int *uuid = uuid_str_to_data(advertising_uuid); // 将UUID字符串(16字节)转换为无符号整数数组，将 UUID 的值保存在 adv_data_cp.data 数组中，并更新 segment_length
  int i;
  for(i=0; i<strlen(advertising_uuid)/2; i++) //for循环遍历UUID字符串的每个字符,每次循环处理两个字符,循环条件是i<strlen(advertising_uuid)/2,因为一个UUID字符占用两个字节
  {
    adv_data_cp.data[adv_data_cp.length + segment_length]  = htobs(uuid[i]); segment_length++; //依次在数组中存储UUID转换成整数的结果，然后更新segment_length
  }

  // Major number (2字节)
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(major_number >> 8 & 0x00FF); segment_length++; // 分离major_number的高8位字节
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(major_number & 0x00FF); segment_length++; // 分离major_number的低8位字节

  // Minor number (2字节)
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(minor_number >> 8 & 0x00FF); segment_length++; // 分离minor_number的高8位字节
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(minor_number & 0x00FF); segment_length++; // 分离minor_number的低8位字节

  // RSSI calibration (信号强度，单位为 dBm)
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(twoc(rssi_value, 8)); segment_length++;

  adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1);

  adv_data_cp.length += segment_length;

  memset(&rq, 0, sizeof(rq));  // 初始化结构体cp内所有成员变量为0
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  rq.cparam = &adv_data_cp;
  rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  ret = hci_send_req(device_handle, &rq, 1000); // 通过hci_send_req函数发送请求

  hci_close_dev(device_handle); //关闭设备

 // 根据hci_send_req函数返回的状态判断广播是否成功
  if(ret < 0) // 如果ret小于0，则表示发送请求失败，输出错误信息并返回1
  {
    fprintf(stderr, "Can't send request %s (%d)\n", strerror(errno), errno); // strerror(errno)用于获取与错误码对应的错误描述字符串,errno保存最近一次发生的错误码
    return(1);
  }

  if (status) // 如果status不为0,则表示广播操作返回了错误状态,输出错误信息并返回1
  {
    fprintf(stderr, "LE set advertise returned status %d\n", status);
    return(1);
  }
}

// 函数disable_advertising，用于关闭蓝牙广播
int disable_advertising() 
{
  int device_id = hci_get_route(NULL);  //获取蓝牙设备ID

  int device_handle = 0;
  if((device_handle = hci_open_dev(device_id)) < 0)
  {
    perror("Could not open device");
    return(1);
  }

  le_set_advertise_enable_cp advertise_cp; // 跟使能蓝牙的结构体相同
  uint8_t status;

  memset(&advertise_cp, 0, sizeof(advertise_cp));

  struct hci_request rq;
  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
  rq.cparam = &advertise_cp;
  rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  int ret = hci_send_req(device_handle, &rq, 1000);

  hci_close_dev(device_handle);

  if (ret < 0) 
  {
    fprintf(stderr, "Can't set advertise mode: %s (%d)\n", strerror(errno), errno);
    return(1);
  }

  if (status) 
  {
    fprintf(stderr, "LE set advertise enable on returned status %d\n", status);
    return(1);
  }
}

// ctrlc_handler函数是一个信号处理函数，用于接收到终止广播的信号时设置global_done为1
void ctrlc_handler(int s)
{
  global_done = 1;
}

// 主函数,用于开启广播并等待终止广播
void main(int argc, char **argv)
{
  if(argc != 6) // if语句判断命令行参数的个数是否为6，如果不是，则打印出正确的命令行参数格式，并通过调用exit函数退出程序
  {
    fprintf(stderr, "Usage: %s <advertisement time in ms> <UUID> <major number> <minor number> <RSSI calibration amount>\n", argv[0]);
    exit(1);
  }

  // 调用enable_advertising函数开启广播,该函数接受五个参数,分别是广播时间（以ms为单位）、UUID、主要号、次要号和RSSI校准量
  // 函数返回一个整型值rc,用于判断广播是否成功开启
  int rc = enable_advertising(atoi(argv[1]), argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5])); // 用enable_advertising函数开启广播
  if(rc == 0)  //广播成功开启
  {
    struct sigaction sigint_handler; // 创建sigaction结构体变量sigint_handler

    sigint_handler.sa_handler = ctrlc_handler;  //将其成员变量sa_handler设置为ctrlc_handler函数
    sigemptyset(&sigint_handler.sa_mask); // 调用sigemptyset函数将sigint_handler结构体变量的成员变量sa_mask设置为空集
    sigint_handler.sa_flags = 0; // 将sigint_handler结构体变量的成员变量sa_flags设置为0

    sigaction(SIGINT, &sigint_handler, NULL); // 调用sigaction函数将SIGINT信号与sigint_handler结构体变量关联起来，注册信号处理函数

    fprintf(stderr, "Hit ctrl-c to stop advertising\n"); // 打印一条提示信息，告诉用户按下Ctrl+C可以停止广播

    //在一个while循环中,不断检查全局变量global_done的值,如果为0,则每隔1秒休眠一次
    //当接收到Ctrl+C信号时，global_done会被设置为1，从而退出while循环
    while(!global_done) { sleep(1); } 

    fprintf(stderr, "Shutting down\n"); // 打印一条提示信息，表示正在关闭广播
    disable_advertising(); // 调用disable_advertising函数关闭广播
  }
}
