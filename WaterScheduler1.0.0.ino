#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ESP8266mDNS.h>            
#include <ArduinoOTA.h>             // OTA Header File library
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeAlarms.h>             // Used for Schedules, Timers, and Time

// Initialize Telegram BOT
#define BOTtoken "XXXXXXXXXXXXXXXXXXXXXXXXX"  // your Bot Token (Get from Botfather)

//Enter one or two chat_ids to specify default users of your bot.
#define CHAT_ID "XXXXXXXXXX" 
#define CHAT_ID2 "XXXXXXXXXX"
#define MAX_VIEWS 2

X509List cert(TELEGRAM_CERTIFICATE_ROOT);

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);


int botRequestDelay = 1000;         // Checks for new messages every 1 second.
unsigned long lastTimeBotRan;       
short int view=0; //keeps track of the currently active view in the default reply keybaord
String chat_id; //stores chat_id of sender
String text; //stores text of message

struct schedules{        //structure to keep track of schedules
  short int startarr[3]; //starting time in HH MM SS
  short int endarr[3];   //ending time in HH MM SS
  int tid1,tid2;         //stores TimeAlarms ID of starting and ending alarms
  bool alarm_en;         //stores status of an alarm, false for disabled, true for enabled.
};

schedules s[5];        //maximum of 5 schedules at any time

// Replace with your network credentials
const char* ssid = "XXXXXXXXXXXXXX";
const char* password = "XXXXXXXXXXXXXXXX";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const int ledPin = 14;  //valve relay pin on ESP8266, can be changed accordingly
bool ledState = "ON"; //reverse logic

void MorningAlarm(){                                   //starting Alarm - valve turned On
  bot.sendMessage(CHAT_ID, "LED state set to ON", "");
  bot.sendMessage(CHAT_ID2, "LED state set to ON", "");
  ledState = LOW;
  digitalWrite(ledPin, ledState);
}

void EveningAlarm(){                                   //ending Alarm - valve turned Off
  bot.sendMessage(CHAT_ID, "LED state set to OFF", "");
  bot.sendMessage(CHAT_ID2, "LED state set to OFF", "");
  ledState = HIGH;
  digitalWrite(ledPin, ledState);          
}

void OnceOnly(){                                      //setting up a timer function
  bot.sendMessage(CHAT_ID, "LED state set to OFF", "");
  bot.sendMessage(CHAT_ID2, "LED state set to OFF", "");
  ledState = HIGH;
  digitalWrite(ledPin, ledState);
}

String canner(){                                      //scans for local wifi networks
  Serial.println("Scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  String ns = String(n);
  String fin;
  Serial.println("Scan done");
  if (n == 0) {
      return ("No Networks Found");
  } else {
    fin=(String(n)+" Networks Found: \n");
    for (int i = 0; i < n; ++i) {
      fin+=String(i+1);
      fin+=" : "+WiFi.SSID(i);
      fin+=" ("+String(WiFi.RSSI(i))+") ";
      fin+=(WiFi.encryptionType(i)==AUTH_OPEN ? "O" : "*"); //display if network is open or closed
      fin+="\n";
      delay(10);
    }
    return fin;
  }
}

void getReply(){                                              //get chat_id and text of a reply to a command
  while(!(bot.getUpdates(bot.last_message_received+1))) { };
  chat_id = String(bot.messages[0].chat_id);
  text = bot.messages[0].text;
}

String digitalClockDisplay()                                 //formats Time to be displayed
{
  String str="";
  str+=String(hour());
  str+=printDigits(minute());
  str+=printDigits(second());
  str+="\n";

  return str;
}

String printDigits(int digits)
{
  String str="";
  str+=":";
  if(digits < 10)
    str+="0";
  str+=digits;

  return str;
}

void autoTime(){                                        //autoamtically calibrates Time to NTP time
    timeClient.update();

    time_t epochTime = timeClient.getEpochTime(); //epoch (UNIX) time i.e. time since Jan 1st 1970 considering Time Offset (19800 for IST)
    struct tm *ptm = gmtime((time_t*)&epochTime); //time structure to get Day, Month and Year

    setTime(timeClient.getHours(),timeClient.getMinutes(),timeClient.getSeconds(),ptm->tm_mday,ptm->tm_mon+1,ptm->tm_year+1900);
}

void manTime(String chat_id){                                                        //manually setting time
    bot.sendMessage(chat_id, "Enter <HH MM SS DD MM YYYY>: ","");
    getReply();
    String str = text;

    String keyboardJson = "[[\"YES\",\"NO\"]]"; //Json for reply keyboard options
    bot.sendMessageWithReplyKeyboard(chat_id, "Entered Time is:\n"+str+"\nEnter 'YES' to confirm: ", "", keyboardJson, true);
    getReply();

    if(text == "YES"){
        short int StringCount=0;
        short int timearr[6];
        
        while (str.length() > 0){      //splitting time input string and entering them into an array
            int index = str.indexOf(' ');
            if (index == -1) {
                timearr[StringCount++] = str.toInt();
                break;
            }
            else{
                timearr[StringCount++] = str.substring(0, index).toInt();
                str = str.substring(index+1);
            }
        }       
        setTime(timearr[0],timearr[1],timearr[2],timearr[3],timearr[4],timearr[5]); //setting the manual time using the filled array

        bot.sendMessage(chat_id, "Time has been changed","");
    } else { bot.sendMessage(chat_id, "Exited... ",""); } 
}

void setTimer(String chat_id){                                //set a timer (in minutes)
    String keyboardJson = "[[\"1\", \"5\",\"10\",\"15\"]]";   //a few default options shown in reply keyboard
    bot.sendMessageWithReplyKeyboard(chat_id, "Enter duration (in minutes):", "", keyboardJson, true);
    getReply();

    String cpy=text;
    keyboardJson = "[[\"YES\",\"NO\"]]";
    bot.sendMessageWithReplyKeyboard(chat_id, "Setting timer for "+text+" minutes. Enter 'YES' to confirm, anything else to cancel.", "", keyboardJson, true);
    getReply();

    if(text.equals("YES")){
        int dur=cpy.toInt()*60;
        Serial.println(dur);

        bot.sendMessage(chat_id, "Timer set for "+cpy+" minutes!","");    
        Alarm.timerOnce(dur,OnceOnly);  //setting alarm for timer

        ledState = LOW;
        digitalWrite(ledPin, ledState);

        bot.sendMessage(CHAT_ID, "LED state set to ON", "");
        bot.sendMessage(CHAT_ID2, "LED state set to ON", "");
    }  
    else{ bot.sendMessage(chat_id,"Timer cancelled!"); } 
}

void displaySchedule(String chat_id){               //displays the currently enabled schedules
    String mess="Existing Schedules: \n";
    mess += "Starting\t\t\tEnding\n";
    for(int count=0;count<5;count++){
        if(s[count].alarm_en){                      //displays only enabled schedules
            mess += "Schedule "+String(count+1)+":\n";
            for(int i=0;i<3;i++){ mess += String(s[count].startarr[i])+((i==2)?"":":"); }
            mess += "\t\t\t";
            for(int i=0;i<3;i++){ mess +=  String(s[count].endarr[i])+((i==2)?"":":"); }
            mess += "\n";
        }      
    }
    bot.sendMessage(chat_id,mess,"");
}

void addSchedule(String chat_id){                                      //add a schedule
    bot.sendMessage(chat_id, "Enter starting time(24 hour format): ");
    getReply();
    String s1 = text;

    bot.sendMessage(chat_id, "Enter ending time(24 hour format):");
    getReply();
    String s2 = text;
    
    String keyboardJson = "[[\"YES\",\"NO\"]]";
    bot.sendMessageWithReplyKeyboard(chat_id, "Starting time: "+s1+"\nEnding time: "+s2+"\nEnter 'YES' to confirm:", "", keyboardJson, true);
    
    getReply();

    if(text == "YES"){
        bool flag=false;
        for(int count=0;count<5;count++){  //splits starting time string into array of a structure instance
            if(!s[count].alarm_en){
                int StringCount = 0;
                while (s1.length() > 0){
                    int index = s1.indexOf(' ');
                    if (index == -1) {
                        s[count].startarr[StringCount++] = s1.toInt();
                        break;
                    }
                    else{
                        s[count].startarr[StringCount++] = s1.substring(0, index).toInt();
                        s1 = s1.substring(index+1);
                    }
                }

                StringCount = 0;
                while (s2.length() > 0){ //splits ending time string into array of a structure instance
                    int index = s2.indexOf(' ');
                    if (index == -1) {
                        s[count].endarr[StringCount++] = s2.toInt();
                        break;
                    }
                    else{
                        s[count].endarr[StringCount++] = s2.substring(0, index).toInt();
                        s2 = s2.substring(index+1);
                    }
                }

                s[count].tid1 = Alarm.alarmRepeat(s[count].startarr[0],s[count].startarr[1],s[count].startarr[2],MorningAlarm);  //set starting alarm and store alarm ID in structure
                s[count].tid2 = Alarm.alarmRepeat(s[count].endarr[0],s[count].endarr[1],s[count].endarr[2],EveningAlarm);        //set ending alarm and store alarm ID in structure
                s[count].alarm_en=true;
                flag=true;
                bot.sendMessage(chat_id,"Schedule has been added","");
                break;
            }
        }

        if(!flag){
            bot.sendMessage(chat_id, "Too many Schedules!","");
        }
    }
}

void delSchedule(String chat_id){
    displaySchedule(chat_id); 
    
    bool flag = true;
    String keyboardJson="[[";
    for(count=0;count<5;count++){
        if(s[count].alarm_en){
            if(!flag){ keyboardJson += ",";}
            keyboardJson += "\""+String(count+1)+"\"";
            if(flag){ flag = false; }
        }
    }
    keyboardJson += "]]";

    Serial.println(keyboardJson);

    bot.sendMessageWithReplyKeyboard(chat_id, "Enter schedule to delete: ", "", keyboardJson, true);
    
    getReply();
    
    int choice = (text.toInt()-1);
    
    if(choice>(-1) && choice<5){
        if(s[choice].alarm_en){
            Alarm.disable(s[choice].tid1);
            Alarm.disable(s[choice].tid2);
            for(int count=0;count<3;count++){
                s[choice].startarr[count]=0;
                s[choice].endarr[count]=0;
            }
            s[choice].tid1=0;
            s[choice].tid2=0;
            s[choice].alarm_en=false;
            bot.sendMessage(chat_id,"Schedule has been deleted!","");
        }
        else{ bot.sendMessage(chat_id, "No schedule to delete!",""); }
    }
    else{ bot.sendMessage(chat_id, "Selected Schedule Out of Bounds!"); }
}

void rebootESP(String chat_id){
    bot.sendMessage(chat_id, "Enter passcode:","");
    getReply();

    if(text=="3643ASHTOJO"){
        bot.sendMessage(chat_id, "Authenticated. Rebooting now...","");
        delay(100);
        ESP.restart();
    }
}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    chat_id = String(bot.messages[i].chat_id);
    
    if (chat_id != CHAT_ID && chat_id != CHAT_ID2){
        bot.sendMessage(chat_id, "Unauthorized user", "");
        continue;
    }
    
    text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (text == "/valve_off") {
        bot.sendMessage(chat_id, "Valve state set to OFF", "");
        ledState = HIGH;
        digitalWrite(ledPin, ledState);
    }
    
    else if (text == "/valve_on") {
        bot.sendMessage(chat_id, "Valve state set to ON", "");
        ledState = LOW;
        digitalWrite(ledPin, ledState);
    }

    else if (text == "/state") {
      if (digitalRead(ledPin)){
        bot.sendMessage(chat_id, "LED is OFF", "");
      }
      else{
        bot.sendMessage(chat_id, "LED is ON", "");
      }
    }

    else if(text == "/network_details"){
        bot.sendMessage(chat_id,"SSID: "+WiFi.SSID()+" Strength: "+WiFi.RSSI(),"");
    }

    else if(text == "/network_scan"){
        bot.sendMessage(chat_id, canner(),"");
    }

    else if(text == "/get_time"){
      bot.sendMessage(chat_id,digitalClockDisplay(),"");
    }

    else if(text == "/set_time"){
      manTime(chat_id);
      goto x;
    }

    else if(text == "/cal_time"){
        autoTime();
    }

    else if(text == "/set_timer"){     
        setTimer(chat_id);
        goto x;
    }
  
    else if(text == "/disp_schedule"){
        displaySchedule(chat_id);
    }

    else if(text == "/add_schedule"){
        addSchedule(chat_id);
        goto x;
    }
  
    else if(text == "/del_schedule"){
        delSchedule(chat_id);
        goto x;
    }

    else if(text == "/reboot"){
        rebootESP(chat_id);
    }

    else if(text == "/next"){
        view = (view+1)%MAX_VIEWS;
        goto x;
    }

    else if(text == "/prev"){
        if(view>=1){
            view = (view-1);
        }
        else view=MAX_VIEWS-1;

        goto x;
    }

    else if(text == "/verbose_menu"){
        String welcome = "Here's the verbose menu, " + from_name + ".\n";
        welcome += "Use the following commands to control your outputs.\n\n";
        welcome += "/valve_on to turn Tap ON \n";
        welcome += "/valve_off to turn Tap ON \n";
        welcome += "/network_details to get network strenght \n";
        welcome += "/network_scan to get nearby network(s) \n";
        welcome += "/set_timer to get network strength \n";
        welcome += "/state to request current Valve state \n";
        welcome += "/get_time to get ESP time\n";
        welcome += "/set_time to set ESp time\n";
        welcome += "/disp_schedule to get schedules\n";
        welcome += "/add_schedule to get schedules\n";
        welcome += "/del_schedule to get schedules\n";
        welcome += "/calibrate_time to set to IST NTP Time\n";

        bot.sendMessage(chat_id, welcome,"");
    }

    else { //triggers if no commands is given
        x:
        String keyboardJson;
        switch(view){
            case 0:
                keyboardJson = "[[\"/ledon\", \"/ledoff\",\"/state\"],[\"/disp_schedule\",\"/add_schedule\",\"/del_schedule\"],[\"/get_time\",\"/set_time\",\"/cal_time\"],[\"/prev\",\"/set_timer\",\"/next\"]]";
                break;
            case 1:
                keyboardJson = "[[\"/network_details\",\"/network_scan\"],[\"/next\",\"/prev\"]]";
                break;
            default:
                keyboardJson = "[[\"/ledon\", \"/ledoff\",\"/state\"],[\"/disp_schedule\",\"/add_schedule\",\"/del_schedule\"],[\"/get_time\",\"/set_time\",\"/cal_time\"],[\"/prev\",\"/set_timer\",\"/next\"]]";
                break;
        }

        String welcome = "Enter a choice via the reply keyboard:\nEnter /verbose_menu for list of all commands:";

        bot.sendMessageWithReplyKeyboard(chat_id, welcome, "", keyboardJson, true);
    }
  }
}

void setup() {
  Serial.begin(115200);

  #ifdef ESP8266
    configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP
    client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  #endif

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);
  
  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  #ifdef ESP32
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  #endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.setTimeOffset(19800); //GMT Offset +5.30 -> 5.5*3600=19800

  ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  autoTime();

  s[0].startarr[0]=11;
  s[0].startarr[1]=00;
  s[0].startarr[2]=00;
  
  s[0].endarr[0]=11;
  s[0].endarr[1]=15;
  s[0].endarr[2]=00;
  
  s[0].tid1 = Alarm.alarmRepeat(s[0].startarr[0],s[0].startarr[1],s[0].startarr[2],MorningAlarm);
  s[0].tid2 = Alarm.alarmRepeat(s[0].endarr[0],s[0].endarr[1],s[0].endarr[2],EveningAlarm);

  s[0].alarm_en=true;

  Serial.println("Bot on");

  bot.sendMessage(CHAT_ID, "Bot on!","");
}

void loop(){
  ArduinoOTA.handle();
  timeClient.update();
  Alarm.delay(900);//checks for alarm trigger every 900 milliseconds

  if (millis() > lastTimeBotRan + botRequestDelay)  { //checks every <botRequestDelay> milliseconds
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);  //number of new messages

    while(numNewMessages) {
      handleNewMessages(numNewMessages); //call function to handle each message
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}
