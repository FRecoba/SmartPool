#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ADS1115.h>
#include <Time.h>
#include <ESP8266TimeAlarms.h>
#include "DHT.h"

//DHT
#define DHTPIN D3
#define DHTTYPE DHT11
#define PUMPPIN D4
//WIFI
#define WIFI_AP "iPhone de Felipe"
#define WIFI_PASSWORD "felifeli"
//  configuración datos thingsboard
#define NODE_NAME "SmartPool"   //nombre que le pusieron al dispositivo cuando lo crearon
#define NODE_TOKEN "alO8AhobADVh8otYwiiR"   //Token que genera Thingboard para dispositivo cuando lo crearon


// declarar cliente Wifi y PubSus
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Declarar e Inicializar sensores.
DHT dht(DHTPIN, DHTTYPE);

//Declaro conversor AD
ADS1115 adc;

AlarmId id;

// declarar variables control temporales
unsigned long lastSend;
const int elapsedTime = 10000; // tiempo transcurrido entre envios al servidor

//Strings del thingsBoard
char telemetryTopic[] = "v1/devices/me/telemetry";
char requestTopic[] = "v1/devices/me/rpc/request/+";  //RPC - El Servidor usa este topico para enviar rquests, cliente response
char attributesTopic[] = "v1/devices/me/attributes";  // Permite recibir o enviar mensajes dependindo de atributos compartidos
char thingsboardServer[] = "demo.thingsboard.io";

//Variables del sistema
float setPointPh;
float toleranciaPh;
int frecuenciaDeMedida;
int frecuenciaDeBombeo;
int tiempoBombeo;
int frecuenciaDeLimpieza;
int mlCloro;
int PinBombaGen=D6;
int PinBombaCL=D5;
int PinBombaUP=D7;
int PinBombaDOWN=D8;
float ph;
boolean medir;
boolean limpiar;
boolean bombear;
boolean enviar;

void setup(){
  Serial.begin(115200);
  Alarm.delay(1000);
  frecuenciaDeMedida = 15;
  setPointPh =7 ;
  toleranciaPh =0.3;
  tiempoBombeo = 2;
  frecuenciaDeBombeo = 10;
  mlCloro=1;
  frecuenciaDeLimpieza=120;
  enviar=true;
  medir =true;
  limpiar =true;
  bombear =true;
  ph=7;
  Serial.println("Configurando");
  //Habilito y apago todas las salidas a las bombas
  pinMode(PinBombaGen, OUTPUT);
  pinMode(PinBombaCL, OUTPUT);
  pinMode(PinBombaUP, OUTPUT);
  pinMode(PinBombaDOWN, OUTPUT);
  digitalWrite(PinBombaGen, LOW);
  digitalWrite(PinBombaCL, LOW);
  digitalWrite(PinBombaUP, LOW);
  digitalWrite(PinBombaDOWN, LOW);
  dht.begin(); //inicaliza el DHT

  //Conversor AD
  adc.begin();
  adc.set_data_rate(ADS1115_DATA_RATE_860_SPS);
  adc.set_mode(ADS1115_MODE_CONTINUOUS);
  adc.set_mux(ADS1115_MUX_GND_AIN0);
  adc.set_pga(ADS1115_PGA_MASK);
  // inicializar wifi y pubsus
  connectToWiFi();
  client.setServer( thingsboardServer, 1883 );

  // agregado para recibir callbacks
  client.setCallback(on_message);
   
  lastSend = 0; // para controlar cada cuanto tiempo se envian datos
}
  
void loop(){

 

  if ( !client.connected() ) {
    reconnect();
  }
  //Enviar datos
  if (enviar) { // Update and send only after 1 seconds
    
    // FUNCION DE TELEMETRIA para enviar datos a thingsboard
    Alarm.timerOnce(10,getAndSendData);  // FUNCION QUE ENVIA INFORMACIÓN DE TELEMETRIA
    enviar=false;
    
  }
 
  //Escuchar callback
  client.loop();

  
   Alarm.delay(50); // wait one second between clock display
   if(medir){   
    Alarm.timerOnce(frecuenciaDeMedida,medirPH);
    Serial.print( "MIDIENDO·········································" );
    Serial.print(frecuenciaDeMedida);
    medir =false;
   }
   if(limpiar){
    Alarm.timerOnce(frecuenciaDeLimpieza,limpieza);
    limpiar =false;
   }
   if(bombear){
    Alarm.timerOnce(frecuenciaDeBombeo,bombeoPrincipal);
    bombear=false;
   }
   
  }
void getAndSendData(){

  // Lectura de sensores 
  Serial.println("Collecting data.");

  // Reading temperature or humidity takes about 250 milliseconds!
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read PH
  

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)||isnan(ph)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  String temperature = String(t);
  String humidity = String(h);
  String PH = String(ph);
  

  //Debug messages
  Serial.print( "Sending temperature and humidity : [" );
  Serial.print( temperature ); Serial.print( "," );
  Serial.print( humidity );Serial.print( "," );
  Serial.print( PH );
  Serial.print( "]   -> " );


  // Preparar el payload del JSON payload, a modo de ejemplo el mensaje se arma utilizando la clase String. esto se puede hacer con
  // la biblioteca ArduinoJson (ver on message)
  // el formato es {key"value, Key: value, .....}
  // en este caso seria {"temperature:"valor, "humidity:"valor}
  //
  String payload = "{";
  payload += "\"temperature\":"; payload += temperature; payload += ",";
  payload += "\"humidity\":"; payload += humidity; //payload += ",";
  //payload += "\"PH\":"; payload += PH;
  payload += "}";
  Serial.println(payload);

  // Enviar el payload al servidor usando el topico para telemetria
  char attributes[200];
  payload.toCharArray( attributes, 200 );
  if (client.publish( telemetryTopic, attributes ) == true)
    Serial.println("publicado ok");
  
  Serial.println( attributes );
  enviar=true;

}



/* 
 *  Este callback se llama cuando se utilizan widgets de control que envian mensajes por el topico requestTopic
 *  Notar que en la función de reconnect se realiza la suscripción al topico de request
 *  
 *  El formato del string que llega es "v1/devices/me/rpc/request/{id}/Operación. donde operación es el método get declarado en el  
 *  widget que usan para mandar el comando e {id} es el indetificador del mensaje generado por el servidor
 */
void on_message(const char* topic, byte* payload, unsigned int length) {
  // Mostrar datos recibidos del servidor
  Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(json);

  // Decode JSON request
  // Notar que a modo de ejemplo este mensaje se arma utilizando la librería ArduinoJson en lugar de desarmar el string a "mano"
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }
// if(attributesTopic[]==topic){
//    // Obtener el nombre del método invocado, esto lo envia el switch de la puerta y el knob del motor que están en el dashboard
//  String methodName = String((const char*)data["method"]);
//  Serial.print("Nombre metodo:");   probar si queda vacio>>????
//  Serial.println(methodName);
// }else if(requestTopic[]==topic) {
  String PPHH = String((const char*)data["PH"]);
  if(PPHH!=""){
   ph=PPHH.toFloat();;
  Serial.println(ph);
  }


  String TB = String((const char*)data["Tiempo"]);
  if(TB!=""){
    tiempoBombeo=TB.toInt();
  Serial.println(TB);
  }
  
  String FB = String((const char*)data["FrecuenciaDeBombeo"]);
  if(FB!=""){
    frecuenciaDeBombeo=FB.toInt();
    Serial.println(FB);
  }

  String FL = String((const char*)data["FrecuenciaDeLimpieza"]);
  if(FL!=""){
    frecuenciaDeLimpieza=FL.toInt();
    Serial.println(FL);
  }

  String FM = String((const char*)data["FrecuenciaDeMedida"]);
  if(FM!=""){
    frecuenciaDeMedida=FM.toInt();
    Serial.println(FM);
  }

  String SP = String((const char*)data["SetPoint"]);
  if(SP!=""){
    setPointPh=SP.toFloat();
    Serial.println(SP);
  }
    String T = String((const char*)data["Tolerancia"]);
  if(T!=""){
    toleranciaPh=T.toFloat();
    Serial.println(T);
  }
    String CL = String((const char*)data["Cloro"]);
  if(CL!=""){
    mlCloro=CL.toInt();
  Serial.println(CL);
  }
  

}
void reconnect() {
  int statusWifi = WL_IDLE_STATUS;
  // Loop until we're reconnected
  while (!client.connected()) {
    statusWifi = WiFi.status();
    connectToWiFi();
    
    Serial.print("Connecting to ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect(NODE_NAME, NODE_TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
      
      // Suscribir al Topico de request
      client.subscribe(requestTopic); 
            client.subscribe(attributesTopic); 
            client.subscribe(telemetryTopic);

      
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}

/*
 * función para conectarse a wifi
 */
void connectToWiFi()
{
  Serial.println("Connecting to WiFi ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}


void medirPH(){
 
  float val = 0;
  for(int i=0;i<100;i++){
            val += adc.read_sample(); // tomo la medida de la sonda 
  }
  val = (val/100);
  val=(val*7.81)/1000;
  val=val/57.14;
  //ph=-val+7;
  Serial.print("PH: ");
  Serial.println(ph);
  compensarPH();
  medir= true;
}
void compensarPH(){
  //Discrimino entre Acido o basico
  float deltaPH=(setPointPh-ph);
  Serial.println("Diferencia de PH");
  Serial.println(deltaPH);
  if(((deltaPH)>toleranciaPh)||(((-1)*deltaPH)>toleranciaPh)){ //Veo si esta fuera del rango de tolerancia
     Serial.println("COMPENSAR");
    float tiempo = deltaPH*5; // 2 segundo por cada unidad de diferencia de ph
    if(deltaPH>0){
      digitalWrite(PinBombaUP, HIGH);
      Alarm.timerOnce(tiempo,pumpUP);
    }else{
      tiempo=-tiempo;
      digitalWrite(PinBombaDOWN, HIGH);
      Alarm.timerOnce(tiempo,pumpDOWN);
    }
  }
}

void bombeoPrincipal(){
  bombear = true;
  //Prendo y mando a apagar en tiempoBombeo
  digitalWrite(PinBombaGen, HIGH);
  Alarm.timerOnce(tiempoBombeo,pumpGen);
}

void limpieza(){
  limpiar = true;
  //Prendo y mando a apagar en cuando bombe mlCLoro
  digitalWrite(PinBombaCL, HIGH);
  Alarm.timerOnce(mlCloro,pumpCL); //3 ml de cloro por seg aprox
}

void pumpDOWN(){
    digitalWrite(PinBombaDOWN, LOW);
}
  
void pumpUP(){
    digitalWrite(PinBombaUP, LOW);
}
void pumpCL(){
    digitalWrite(PinBombaCL, LOW); 
} 
void pumpGen(){
    digitalWrite(PinBombaGen, LOW);
} 



