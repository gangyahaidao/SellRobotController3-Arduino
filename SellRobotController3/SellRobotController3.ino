unsigned long startRunMillis = 0;
byte inChar;//用于缓存一帧的有用数据
boolean stringComplete = false;
byte recv_index = 0;
byte check = 0;
byte speed_buffer[4], speed_data[4];//前变量用于接收，后变量用于计算
void serialEvent() {//接收上位机发送的控制命令 #1F10@
  if (Serial.available()) {//数据必须要进行矫正，之后回复服务器端
    inChar = (unsigned char)Serial.read();
    if(inChar == '#') // 一帧数据的开始字节
    {
      recv_index = 0;
    }else if(inChar == '@') // 接收到数据尾部
    {
//      check = speed_buffer[0];
//      for(int i = 1; i < recv_index-1; i++){ //recv_index == 4
//        check = check^speed_buffer[i];
//      }
//      if(check == speed_buffer[recv_index-1]){
//        stringComplete = true;
//      }
      stringComplete = true; // 用于测试使用
      startRunMillis = millis(); // 获取收到命令的时间
      recv_index = 0;
    }else{ //将数据存储起来      
      if(recv_index >= 4){
        recv_index = 0;
      }else{
        speed_buffer[recv_index] = inChar;
        recv_index++;  
      }
    }
  }
}

int motorControlPins[] = {22, 23, 24, 25, 26, 27, 28, 29, 30}; // 电机继电器控制引脚
int motorENAPins[] = {2, 3, 4}; // 电机控制器引脚 ENA IN1 IN2
int pinDoorOpen = 5; // 门开触动开关
int motorStatusPin = 32; // 电机转动状态引脚，当转动检测开关弹起时值为0

void setup() {
  Serial.begin(115200);

  for(int i = 0; i < 3; i++) {
    pinMode(motorENAPins[i], OUTPUT);
  }
  digitalWrite(motorENAPins[0], LOW); // 初始关闭电机使能

  for(int j = 0; j < 9; j++) {
    pinMode(motorControlPins[j], OUTPUT); // 向引脚写入HIGH继电器接通
  }

  pinMode(pinDoorOpen, INPUT_PULLUP);
  pinMode(motorStatusPin, INPUT_PULLUP);  
}

int switchPinNum = 0;//哪个电机在转动
int getSwitchPinNum(char floorNum, char motorNum){
  int index = 0;
  int whichFloor = 0;
  if(floorNum == '1'){
    if(motorNum == 'B'){ // 后排
      index = 22;
    }else if(motorNum == 'M'){ // 中间一排
      index = 23;
    }else if(motorNum == 'F'){ // 前排
      index = 24;
    }
  }else if(floorNum == '2'){
    if(motorNum == 'B'){ // 后排
      index = 25;
    }else if(motorNum == 'M'){ // 中间一排
      index = 26;
    }else if(motorNum == 'F'){ // 前排
      index = 27;
    }
  }else if(floorNum == '3'){
    if(motorNum == 'B'){ // 后排
      index = 28;
    }else if(motorNum == 'M'){ // 中间一排
      index = 29;
    }else if(motorNum == 'F'){ // 前排
      index = 30;
    }
  }
  return index;
}

/**
获取引脚状态
*/
int readPinValue(int pin) {
  int value1 = digitalRead(pin);
  delay(10);
  int value2 = digitalRead(pin);
  value1 = value1 & value2;
  return value1;
}

void controllMotor(int motorPin, int cmd) {
  if(cmd == 1) { // 开始转动
    digitalWrite(motorENAPins[0], HIGH);
    digitalWrite(motorENAPins[1], HIGH);
    digitalWrite(motorENAPins[2], LOW);
    digitalWrite(motorPin, HIGH);
  } else { // 停止转动
    digitalWrite(motorPin, LOW); // 断开继电器
    digitalWrite(motorENAPins[0], LOW); // 关闭电机驱动器
  }    
}

int doorState = 0; // 门的状态 0--关闭 1--被打开  2--打开之后关闭重置为0
void loop() {
  if(stringComplete) { // 如果收到一帧购买数据
    Serial.println("#RECV@"); // 通知服务器已经收到控制命令
    memcpy(speed_data, speed_buffer, 3);

    switchPinNum = getSwitchPinNum(speed_data[0], speed_data[1]);
    stringComplete = false;
  }

  if(switchPinNum > 0) { // 如果有货需要掉下    
    controllMotor(switchPinNum, 1);// 控制电机开始转动
    while(true) {
      int motorStatus = digitalRead(motorStatusPin);
      if(motorStatus == HIGH) { // 如果电机检测开关被按下
        delay(50); // 避免不稳定阶段
        break;  
      }
      delay(10);
      if(millis() - startRunMillis >= 3500) {
        Serial.println("#STAGE1 TIMEOUT@");
        break;  
      }
    }
    while(true) {
      int motorStatus = digitalRead(motorStatusPin);
      if(motorStatus == LOW) { // 如果电机检测开关弹起
        delay(80);
        controllMotor(switchPinNum, 0); // 停止电机转动
        Serial.println("#OK@"); // 向服务器发送已经完成一个出货消息        
        switchPinNum = 0;
        Serial.print("#time = ");
        Serial.print(millis() - startRunMillis);
        Serial.println(" ms@");
        break;
      }  
      delay(10);
      if(millis() - startRunMillis >= 3500) {
        Serial.println("#STAGE2 TIMEOUT@");
        break;  
      }
    }
  }

  int doorValue = readPinValue(pinDoorOpen); // 低电平0，开关弹起是低电平
  if(doorValue == 0) { // 关闭
    if(doorState == 1) { // 门从打开状态变成关闭状态
      doorState = 0; // 表示进行了一次开关操作，重置为0
      Serial.println("#FINISHED@"); // 向服务器发送货柜门被打开的消息    
    }
  } else if(doorValue == 1) { // 门被打开
    if(doorState == 0) {
      doorState = 1; // 门被打开  
    }
  }
  
}

