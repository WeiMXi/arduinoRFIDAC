
#define NUMBER_OF_BYTES 32 // 加密字节数，也是密钥字节数

////////////////////////////////////////

//将int(2byte转换为字节数组)//////////////
union byte_int_union
{
   uint16_t int_year;
   uint8_t  byte_year[2];
};
byte_int_union b_i_year;
////////////////////////////////////////

//记录的UID/////////////////////////////
#define N 3 //UID个数
#define M 4 //UID字节数
static const byte f[N][M] = //uid字节
{
  {0x95, 0x53, 0x8D, 0xC2},
  {0x2F, 0x4A, 0x85, 0x88},
  {0x2F, 0x4A, 0x85, 0x88},
};





///////// DS1307时钟模块所需  ////////////
#include "RTClib.h"

RTC_DS1307 rtc; //创建一个RTC_DS1307对象
////////////////////////////////////////

////ASK无线模块所需//////////////////////
#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile

RH_ASK driver(2000, 5, 6, 7); //依次为接收，发送，ptt端
////////////////////////////////////////

///舵机、RFID模块////////////////////////////
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>


#define RST_PIN         9          // Configurable, see typical pin layout above
#define SS_PIN          10         // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
Servo mg90s;
////////////////////////////////////

byte key[NUMBER_OF_BYTES]; // 加密、解密密钥

void new_en_de(byte* text, byte* key) //加密、解密函数，明文或密文会存在text中
{
  for (int i = 0; i < NUMBER_OF_BYTES; i++)
  {
    text[i] = key[i] ^ text[i];
  }
}

void setkey(byte* inputkey, uint32_t sec) //刷新密钥函数
{
  randomSeed(sec / 30);
  Serial.println(sec / 30);
  for (int i = 0; i < 32; i++)
  {
    inputkey[i] = random(0, 255);
  }
  Serial.print("key:"); //debug
  debug_32by_print(inputkey);  //debug
  Serial.println("========set key completed==========");
}

void send_it_en(byte* it) //加密并发送，且密文会存在it中
{
  //Serial.println("hello");
  new_en_de(it, key); //加密
  //Serial.println("hello again and again");
  driver.send((byte*)it, strlen(it)); //发送
  //Serial.println("hello again and again and again");
  //delay(100);
  driver.waitPacketSent();  //等待发送完成
  //Serial.println("hello again and again and again and again");
}

int recv_it_and_de(byte* it)   //接收并解密，将明文存在msg中
{
  byte buf[NUMBER_OF_BYTES]; //是否可省
  byte buflen = sizeof(buf);  //是否可省
  if (driver.recv(buf, &buflen)) // Non-blocking
  {
    cpbyte(it, buf, NUMBER_OF_BYTES); //复制
    Serial.print("enmsg:"); //debug
    debug_32by_print(buf); //debug
    new_en_de(it, key); //解密
     // Message with a good checksum received, dump it.
    //driver.printBuffer("Got:", buf, buflen); //debug
    Serial.print("demsg:"); //debug
    debug_32by_print(it); //debug
    return 1;
  }
  return 0;
}

void cpbyte(byte* cpin, byte* cpout, int num) //将cpout复制到cpin字节数组中，需要输入num字节数
{
  for (int i = 0; i < num; i++)
  {
    cpin[i] = cpout[i];
  }
}

void debug_32by_print(byte* msg) //debug使用，将以16进制显示msg信息
{
  //Serial.print("msg:");
  for (int i = 0; i < NUMBER_OF_BYTES; i++)
  {
    Serial.print(msg[i], DEC);
    Serial.print(" ");
  }
  Serial.println();
}

int compare_bytes(const byte* a,const int start_a,const byte* b,const int start_b,const int num) //比较a,b两个字节数组，输入a,b,开始比较的位置与比较的长度
{
  const int maxi = num + start_a;
  int i = start_a;
  int j = start_b;
  for (; i < maxi;)
  {
    if (a[i] != b[j])
    {
      return 0;
    }
    i++;
    j++;
  }
  return 1;
}

int uid_judge(byte* msg) //判断信息是否为UID信息，若不是则返回0，若是则返回1
{
  const byte isuid[4] = { 0x0, 0x0, 0x0, 0x0 }; //判断是否为UID信息
  int flag = 0;
  int i = 0;
  if (compare_bytes(msg, 0, isuid, 0, 4))
  {
    for (i = 0; i < N; i++)
    {
      if (compare_bytes(msg, 4, f[i], 0, 4))
      {
        flag = 1;
      }
    }
  }

  //debug/////////////////////////////
  if (flag)
  {
    Serial.println("uid ture");
  }
  else
  {
    Serial.println("uid flase");
  }
  /////////////////////////////////////

  return flag;

}

void syn_time(byte *msg, DateTime now) //判断msg是否含有时间信息，若是，则同步时间
{
  //DateTime now = rtc.now();
  const byte istime[4] = { 0x1, 0x1, 0x1, 0x1 }; //判断是否为时间信息
  b_i_year.byte_year[0] = msg[4]; //debug
  b_i_year.byte_year[1] = msg[5]; //debug
  Serial.print("dt = "); //debug
  Serial.println(
                abs(  DateTime(b_i_year.int_year, msg[6], msg[7], msg[8], msg[9], msg[10]).unixtime() - now.unixtime()  ) < 30
                ); //debug
  if (  compare_bytes(msg, 0, istime, 0, 4) &&
        (
          abs(DateTime(b_i_year.int_year, msg[6], msg[7], msg[8], msg[9], msg[10]).unixtime() - now.unixtime()  ) < 30
        )
     )
  {
    debug_time_print(now);  //debug
    Serial.println("time syn begin"); //debug
    //b_i_year.byte_year[0] = msg[4];
    //b_i_year.byte_year[1] = msg[5];
    rtc.adjust( DateTime(b_i_year.int_year, msg[6], msg[7], msg[8], msg[9], msg[10]) );
    Serial.println("time syn end"); //debug
    debug_time_print(now);  //debug
  }
}

void debug_time_print(DateTime now) //显示now时间的函数debug用
{
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print('/');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
}

void setup()
{
  while (!Serial); // for Leonardo/Micro/Zero
  Serial.begin(57600);

  ////初始化时钟模块////////////////////////
  //Serial.begin(57600);
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (!rtc.isrunning())
  {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  ///////////////////////////////////////

  //////RFID初始化////////////////////////
  /*
  SPI.begin();
  mfrc522.PCD_Init();    // Init MFRC522
  */
  //mfrc522.PCD_DumpVersionToSerial();






  ////////////////////////////////////////

  //初始化ASK模块//////////////////////////
  if (!driver.init())
    Serial.println("init failed");
  ////////////////////////////////////////

  //初始化舵机/////////////////////////////
  mg90s.attach(3);
  ///////////////////////////////////////

  //初始化密钥/////////////////////////////
  DateTime now = rtc.now();
  setkey(key, now.unixtime());
  ///////////////////////////////////////
}

void loop()
{
  int i = 0;
  byte msg[NUMBER_OF_BYTES];

  //创建时间类
  DateTime now = rtc.now();

  //每30s刷新密钥///////////////////
  if (now.unixtime() % 30 == 0)
  {
    setkey(key, now.unixtime());
  }
  /////////////////////////////////

  if (recv_it_and_de(msg)) //如果收到信息则将信息解密并存入 msg 中，后进入此分支
  {
    //进入UID判断部份///////////////////////////////
    uid_judge(msg);
    //////////////////////////////////////////////
    debug_time_print(now);  //debug
    //进入时间同步部份//////////////////////////////
    syn_time(msg,now);
    /////////////////////////////////////////////
  }
  delay(100);
}
