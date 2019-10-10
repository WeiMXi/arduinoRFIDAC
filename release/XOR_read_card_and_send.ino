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

//记录loop循环次数///////////////////////
static int num_loop = 0;
////////////////////////////////////////

///////// DS1307时钟模块所需  ////////////
#include "RTClib.h"

RTC_DS1307 rtc;
////////////////////////////////////////

////ASK无线模块所需//////////////////////
#include <RH_ASK.h>/
#include <SPI.h> // Not actually used but needed to compile

RH_ASK driver(2000, 5, 6, 7);
////////////////////////////////////////

///舵机、RFID模块////////////////////////////
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN         9          // Configurable, see typical pin layout above
#define SS_PIN          10         // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
////////////////////////////////////

byte key[NUMBER_OF_BYTES]; // 加密、解密密钥

///保持电源输出所需///////////////////////
uint32_t nexttime;
///////////////////////////////////////

void new_en_de(byte *text, byte *key) //加密、解密函数
{
  for (int i = 0; i < NUMBER_OF_BYTES; i++)
  {
    text[i] = key[i] ^ text[i];
  }
}

void setkey(byte* inputkey, uint32_t sec) //刷新密钥函数
{
  randomSeed(sec / 30);
  for (int i = 0; i < 32 ; i ++)
  {
    inputkey[i] = random(0,255);
  }
}

void send_it_en(byte *it) //加密并发送
{
  new_en_de(it,key); //加密
  driver.send((byte *)it, strlen(it)); //发送
  driver.waitPacketSent();  //等待发送完成
}

void recv_it_and_de(byte *it)   //接收并解密
{
  byte buf[NUMBER_OF_BYTES]; //是否可省
  byte buflen = sizeof(buf);  //是否可省
  if (driver.recv(buf, &buflen)) // Non-blocking
  {
   new_en_de(buf,key); //解密
   cpbyte(it, buf, NUMBER_OF_BYTES);
  }
}

void cpbyte(byte *cpin, byte *cpout, int num)
{
  for(int i = 0; i < num; i++)
  {
    cpin[i] = cpout[i];
  }
}

void keep_power()
{
  digitalWrite(8, LOW);
  delay(200);
  digitalWrite(8, HIGH);
  delay(50);
}

void setup()
{
  ////初始化时钟模块////////////////////////
  if (! rtc.begin())
  {
    //Serial.println("Couldn't find RTC");
    while (1);
  }
  if (! rtc.isrunning())
  {
    //Serial.println("RTC is NOT running!");
  }
  ///////////////////////////////////////

  //////RFID初始化////////////////////////
  SPI.begin();
  mfrc522.PCD_Init();    // Init MFRC522

  ////////////////////////////////////////

  //初始化ASK模块//////////////////////////
      if (!driver.init())
         //Serial.println("init failed");
  ////////////////////////////////////////

  //初始化密钥/////////////////////////////
  DateTime now = rtc.now();
  setkey(key,now.unixtime());
  ///////////////////////////////////////

  //////////////////////////////////////
  pinMode(8,OUTPUT);  //保持电源输出的端口
  digitalWrite(8,HIGH); //打开上拉电阻
  nexttime = now.unixtime();
  /////////////////////////////////////
}

void loop()
{
  int i = 0;
  byte msg[NUMBER_OF_BYTES];

  //告诉收信器，这是一个uid信息///////////
  for(i = 0; i < 4 ; i++)
  {
    msg[i] = 0;
  }
  //////////////////////////////////////

  //创建时间类
  DateTime now = rtc.now();

  if ( now.unixtime() > (nexttime + 13) )
  {
    nexttime = nexttime + 13;
    keep_power();
  }

  //每30s刷新密钥///////////////////
  if( now.unixtime() % 30 == 0)
  {
    setkey(key,now.unixtime());
  }

  ////如果读到卡，将获取的UID复制到msg中/////////
  if(mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
  {
    for (i = 0; i < 4; i++)
    {
      msg[i] = 0;
    }
    for (i = 0; i < 4; i++)
    {
      msg[i + 4] = mfrc522.uid.uidByte[i];
    }


    //随机填充后续字符/////////////////
    for(i = 8; i < NUMBER_OF_BYTES; i ++)
    {
      msg[i] = random(0,255);
    }
    /////////////////////////////////

    send_it_en(msg); //发送含有UID的信息

  }

  //每循环20000次，同步一次时间/////////////
  if ( num_loop == 20000)
  {
    //告诉收信器，这是一个时间信息////////
    for(i = 0; i < 4 ; i++)
    {
      msg[i] = 1;
    }
    ///////////////////////////////////

    //将时间信息存入msg中////////////////
    b_i_year.int_year = now.year();
    msg[4] = b_i_year.byte_year[0];
    msg[5] = b_i_year.byte_year[1];
    msg[6] = now.month();
    msg[7] = now.day();
    msg[8] = now.hour();
    msg[9] = now.minute();
    msg[10] = now.second();
    ///////////////////////////////////

    //随机填充后续字符/////////////////
    for(i = 11; i < NUMBER_OF_BYTES; i ++)
    {
      msg[i] = random(0,255);
    }
    /////////////////////////////////
    send_it_en(msg);  //发送
    num_loop = 0; //计数归零
  }
  num_loop++;
  delay(100);
}
