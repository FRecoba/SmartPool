/* Ejemplo para utilizar la comunicación con thingboards paso a paso con nodeMCU v2
 *  Gastón Mousqués
 *  Basado en varios ejemplos de la documentación de  https://thingsboard.io
 *  
 */

// includes de bibliotecas para comunicación
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ADS1115.h>
#include <Time.h>
#include <ESP8266TimeAlarms.h>






 //SCL = pin 1
 //SDA = pin 2
//***************MODIFICAR PARA SU PROYECTO *********************
//  configuración datos wifi 
// decomentar el define y poner los valores de su red y de su dispositivo
#define WIFI_AP "iPhone de Felipe"
#define WIFI_PASSWORD "felifeli"


//  configuración datos thingsboard
#define NODE_NAME "SENSORPH"   //nombre que le pusieron al dispositivo cuando lo crearon
#define NODE_TOKEN "lo2YiyyMgOoTi5fIUiLD"   //Token que genera Thingboard para dispositivo cuando lo crearon


//***************NO MODIFICAR *********************
char thingsboardServer[] = "demo.thingsboard.io";

/*definir topicos.
 * telemetry - para enviar datos de los sensores
 * request - para recibir una solicitud y enviar datos 
 * attributes - para recibir comandos en baes a atributtos shared definidos en el dispositivo
 */
char telemetryTopic[] = "v1/devices/me/telemetry";
char requestTopic[] = "v1/devices/me/rpc/request/+";  //RPC - El Servidor usa este topico para enviar rquests, cliente response
char attributesTopic[] = "v1/devices/me/attributes";  // Permite recibir o enviar mensajes dependindo de atributos compartidos


// declarar cliente Wifi y PubSus
WiFiClient wifiClient;
PubSubClient client(wifiClient);
//Declaro conversor AD
ADS1115 adc;

// declarar variables control loop (para no usar delay() en loop
unsigned long lastSend;
const int elapsedTime = 5000; // tiempo transcurrido entre envios al servidor






//
//************************* Funciones Setup y loop *********************
// función setup micro
void setup()
{
  Serial.begin(115200);
  //Conversor AD
  adc.begin();
  adc.set_data_rate(ADS1115_DATA_RATE_860_SPS);
  adc.set_mode(ADS1115_MODE_CONTINUOUS);
  adc.set_mux(ADS1115_MUX_DIFF_AIN0_AIN1);
  adc.set_pga(ADS1115_PGA_MASK);
  // inicializar wifi y pubsus
  connectToWiFi();
  client.setServer( thingsboardServer, 1883 );

   
  lastSend = 0; // para controlar cada cuanto tiempo se envian datos
  delay(10);
}

// función loop micro
void loop()
{
  if ( !client.connected() ) {
    reconnect();
  }

  if ( millis() - lastSend > elapsedTime ) { // Update and send only after 1 seconds
    
    // FUNCION DE TELEMETRIA para enviar datos a thingsboard
    getAndSendData();   // FUNCION QUE ENVIA INFORMACIÓN DE TELEMETRIA
    
    lastSend = millis();
  }

  client.loop();
}

//***************MODIFICAR PARA SU PROYECTO *********************
/*
 * función para leer datos de sensores y enviar telementria al servidor
 * Se debe sensar y armar el JSON especifico para su proyecto
 * Esta función es llamada desde el loop()
 */
void getAndSendData()
{

  // Lectura de sensores 
  Serial.println("Collecting temperature data.");


  float ph = medirPH();

  // Check if any reads failed and exit early (to try again).
 


  String PH = String(ph);


  //Debug messages
  Serial.print( "Sending PH : [" );
  String payload = "{";
  payload += "\"PH\":"; payload += PH;
  payload += "}";

  // Enviar el payload al servidor usando el topico para telemetria
  char attributes[200];
  payload.toCharArray( attributes, 200 );
  if (client.publish( attributesTopic, attributes ) == true)
    Serial.println("publicado ok");
  if (client.publish(telemetryTopic, attributes ) == true)
    Serial.println("publicado ok");
  Serial.println( attributes );

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
float medirPH(){
     float val = 0;/* You will be oversampling if the loop takes too short a time */
            //Promedio 100 lecturas, de manera de filtrar la medida a nivel de software
            for(int i=0;i<100;i++){
              val += adc.read_sample();
            }
            val = (val/100);

            Serial.print("Value read is ");
            Serial.println(val);
            
            Serial.print("Value mv");
            val=(val*7.81)/1000;
            Serial.println(val);
            
            Serial.print("Value PH");
            val=val/57.14;
            val=-val+7;
            Serial.println(val);
    return val;
}



