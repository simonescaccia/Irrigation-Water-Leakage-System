#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "timex.h"
#include "xtimer.h"
#include "ztimer.h"
#include "paho_mqtt.h"
#include "MQTTClient.h"
#include "stdio.h"
#include "periph/gpio.h"
#include "periph/adc.h"
#include "thread.h"
#include "analog_util.h"

int board=0; //  0 -> padre, che vien settato così  -----  1 -> figlio viene settato prima a 0 e poi cambia, dopo aver avuto conversazione precedente

#define MAIN_QUEUE_SIZE     (8)

#define BUF_SIZE                        1024
#define MQTT_VERSION_v311               4       /* MQTT v3.1.1 version is 4 */
#define COMMAND_TIMEOUT_MS              4000

#ifndef DEFAULT_MQTT_CLIENT_ID
#define DEFAULT_MQTT_CLIENT_ID          ""
#endif

#ifndef DEFAULT_MQTT_USER
#define DEFAULT_MQTT_USER               ""
#endif

#ifndef DEFAULT_MQTT_PWD
#define DEFAULT_MQTT_PWD                ""
#endif

/**
 * @brief Default MQTT port
 */
#define DEFAULT_MQTT_PORT               1883
#define DEFAULT_IP                      "192.168.92.168"
#define DEFAULT_TOPIC_AWS               "home"
char* DEFAULT_TOPIC_SUB; //subscribe
char* DEFAULT_TOPIC_PUBLISH; //publish

//#define DEFAULT_TOPIC_SUB               "publish" //subscribe
//char* DEFAULT_TOPIC_PUBLISH = "subscribe" //publish

/**
 * @brief Keepalive timeout in seconds
 */
#define DEFAULT_KEEPALIVE_SEC           10

#ifndef MAX_LEN_TOPIC
#define MAX_LEN_TOPIC                   100
#endif

#ifndef MAX_TOPICS
#define MAX_TOPICS                      4
#endif

#define IS_CLEAN_SESSION                1
#define IS_RETAINED_MSG                 0

static MQTTClient client;
static Network network;

static unsigned char buf[BUF_SIZE];
static unsigned char readbuf[BUF_SIZE];

char stack_led[THREAD_STACKSIZE_MAIN];
kernel_pid_t thread_led;

char stack_buzzer[THREAD_STACKSIZE_MAIN];
kernel_pid_t thread_buzzer;

char water_sensor_stack[THREAD_STACKSIZE_MAIN];
kernel_pid_t water_sensor_thread;

char publish_stack[THREAD_STACKSIZE_MAIN];
kernel_pid_t publish_thread_aws;

char publish_stack_node[THREAD_STACKSIZE_MAIN];
kernel_pid_t publish_thread_node;

gpio_t LED_PIN = GPIO_PIN(0, 2); //pin2
#define LED_PIN_NUMBER 2

gpio_t BUZZER_PIN = GPIO_PIN(0, 15); //pin23
#define BUZZER_PIN_NUMBER 15

#define WATER_FLOW ADC_LINE(0)
#define ADC_RES ADC_RES_12BIT

#define SECONDS 1*US_PER_SEC //regulator for water_flow_rate formula
#define SAMPLING 3*US_PER_SEC //duration time of sampling
#define TESTING 2*US_PER_SEC
#define DAILY_PERIODIC 8*US_PER_SEC //periodic time of sampling (daily or every 6 hours)
#define ANSWER_AGAIN_PERIODIC 3*US_PER_SEC //again time if son is sleeping
#define NO_WATER_AGAIN_PERIODIC 8*US_PER_SEC //again time if there is not water

xtimer_ticks32_t sample_time_now;
xtimer_ticks32_t sample_time_end;
xtimer_ticks32_t sample_time_diff;
xtimer_ticks32_t periodic_time;

xtimer_ticks32_t time_s0_source;
xtimer_ticks32_t time_s1_source;
xtimer_ticks32_t time_s2_source;
xtimer_ticks32_t time_s0_son;
xtimer_ticks32_t time_s1_son;
xtimer_ticks32_t time_s2_son;

char* mex; //for received messages

int ret = -1;
char message[200];
float water_flow_rate_other=0.0;
float water_flow_rate=0.0;
float water_flow_diff=0.0;
float total_quantity=0.0;

int answer=0;
int work=0;

char* message_on_aws;
char* value_message_on_received;
//sudo BOARD=esp32-heltec-lora32-v2 BUILD_IN_DOCKER=1 DOCKER="sudo docker" PORT=/dev/ttyUSB0 make flash term

/*
We are in the same situation of YF-S201 water flow sensor - 30L/min

l_hour = (flow_frequency * 60 / 7.5)
every liter of water that flows, the Hall Sensor outputs 450 pulses. 

V_total(L) = N* 1/450(L) 

the total volume of fluid flowing through the water flow sensor is equal to the water flow rate(Q - unit L/s) multiplied by time t(unit s) .
V_total(L) = Q(L/s)*t(s) 

N* 1/450 = Q(L/s)*t(s) 
N/t = 450 * Q(L/s) 

f = 450*Q(L/s); 
Q(L/s) = f/450; 
Q(L/min) = f*60/450 = f/7.5 
Q(L/hour) = f*60*60/450 = f*60 /7.5 

l_hour = (flow_frequency * 60 / 7.5)


LEGGERE: https://wiki.seeedstudio.com/Water-Flow-Sensor/#:~:text=Water%20flow%20sensor%20consists%20of,outputs%20the%20corresponding%20pulse%20signal.


SU AMAZON ->  1L = 596 impulsi ----- https://www.amazon.it/dp/B079QYRQT5?psc=1&ref=ppx_yo2ov_dt_b_product_details
K=541,3307 [Impulsi/Litro] con errore di +-1% ma anche così 
USIAMO QUESTA
*/

static int connect(void)
{

    /* ensure client isn't connected in case of a new connection */
    if (client.isconnected) {
        printf("mqtt_example: client already connected, disconnecting it\n");
        MQTTDisconnect(&client);
        NetworkDisconnect(&network);
    }

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = MQTT_VERSION_v311;

    data.clientID.cstring = DEFAULT_MQTT_CLIENT_ID;
    data.username.cstring = DEFAULT_MQTT_USER;
    data.password.cstring = DEFAULT_MQTT_PWD;
    data.keepAliveInterval = DEFAULT_KEEPALIVE_SEC;
    data.cleansession = IS_CLEAN_SESSION;
    data.willFlag = 0;

    printf("mqtt_example: Connecting to MQTT Broker from %s %d\n",
            DEFAULT_IP, DEFAULT_MQTT_PORT);
    printf("mqtt_example: Trying to connect to %s, port: %d\n",
            DEFAULT_IP, DEFAULT_MQTT_PORT);
    ret = NetworkConnect(&network, DEFAULT_IP, DEFAULT_MQTT_PORT);
    if (ret < 0) {
        printf("mqtt_example: Unable to connect\n");
        return ret;
    }

    printf("user:%s clientId:%s password:%s\n", data.username.cstring,
             data.clientID.cstring, data.password.cstring);
    ret = MQTTConnect(&client, &data);
    if (ret < 0) {
        printf("mqtt_example: Unable to connect client %d\n", ret);
        return ret;
    }
    else {
        printf("mqtt_example: Connection successfully\n");
    }

    return (ret > 0) ? 0 : 1;
}

/*
void* led(void* arg){
    while(1){
        printf("Set led to HIGH \n");
        gpio_set(LED_PIN);
        xtimer_sleep(2);
        printf("Set led to LOW \n");
        gpio_clear(LED_PIN);
        xtimer_sleep(2);
        if (fire==0){
            thread_sleep();
        }
    }    
}

void* buzzer(void* arg){
    while(1){
        printf("Set buzzer to HIGH \n");
        gpio_set(BUZZER_PIN); 
        xtimer_sleep(2);
        printf("Set buzzer to LOW \n");
        gpio_clear(BUZZER_PIN);
        xtimer_sleep(2);  
        if (fire==0){
            thread_sleep();
        }
    }    
}
*/
void* publish_on_aws(void* arg){
    while(1){
        enum QoS qos = QOS2;
        char* topic=DEFAULT_TOPIC_AWS;
        MQTTMessage message;
        message.qos = qos;
        message.retained = IS_RETAINED_MSG;
        message.payload = message_on_aws;
        message.payloadlen = strlen(message.payload);

        int rc;
        if ((rc = MQTTPublish(&client, topic, &message)) < 0) {
            printf("mqtt_example: Unable to publish (%d)\n", rc);
        }
        else {
            printf("mqtt_example: Message (%s) has been published to topic %s"
            "with QOS %d\n",
            (char *)message.payload, topic, (int)message.qos);
        }
        thread_sleep();
    }
    return 0;
}

static int publish(char* mex){
    enum QoS qos = QOS2;
    char* topic=DEFAULT_TOPIC_PUBLISH;
    MQTTMessage message;
    message.qos = qos;
    message.retained = IS_RETAINED_MSG;
    message.payload = mex;
    message.payloadlen = strlen(message.payload);

    int rc;
    if ((rc = MQTTPublish(&client, topic, &message)) < 0) {
        printf("mqtt_example: Unable to publish (%d)\n", rc);
    }
    else {
        printf("mqtt_example: Message (%s) has been published to topic %s"
         "with QOS %d\n",
        (char *)message.payload, topic, (int)message.qos);
    }
    return 0;
}

void* publish_on_node(void* arg){
    while(1){
        enum QoS qos = QOS2;
        char* topic=DEFAULT_TOPIC_PUBLISH;
        MQTTMessage message;
        message.qos = qos;
        message.retained = IS_RETAINED_MSG;
        message.payload = value_message_on_received;
        message.payloadlen = strlen(message.payload);

        int rc;
        if ((rc = MQTTPublish(&client, topic, &message)) < 0) {
            printf("mqtt_example: Unable to publish (%d)\n", rc);
        }
        else {
            printf("mqtt_example: Message (%s) has been published to topic %s"
            "with QOS %d\n",
            (char *)message.payload, topic, (int)message.qos);
        }
        thread_sleep();
    }
    return 0;
}

static void _on_msg_received(MessageData *data)
{
    printf("\npaho_mqtt_water_sensor: message received on topic"
           " %.*s: %.*s\n",
           (int)data->topicName->lenstring.len,
           data->topicName->lenstring.data, (int)data->message->payloadlen,
           (char *)data->message->payload);

    mex = (char *)data->message->payload;

    if((strcmp (mex, "heloy"))==0){
        time_s0_son = xtimer_now();
        value_message_on_received="answr";
        thread_wakeup(publish_thread_node); //publish("answr");
        time_s1_son = xtimer_now();
    }

    else if((strcmp (mex, "answr"))==0){
        answer=1;
        time_s1_source = xtimer_now();
        value_message_on_received="start";
        thread_wakeup(publish_thread_node);
        thread_wakeup(water_sensor_thread);
    }

    else if((strcmp (mex, "start"))==0){
        work=1;
        thread_wakeup(water_sensor_thread);
    }

    else{
        if(work==1){
            //thread_wakeup(thread_buzzer);
            //thread_wakeup(thread_led);
            water_flow_rate_other = atof(mex);
            time_s2_son = xtimer_now();
            if(water_flow_rate-water_flow_rate_other > 3 || water_flow_rate_other-water_flow_rate > 3){
                if(water_flow_rate-water_flow_rate_other > 0)
                {    water_flow_diff=water_flow_rate-water_flow_rate_other; }
                else
                {   water_flow_diff=water_flow_rate_other-water_flow_rate;  }

                printf("LEAKEGE DETECTED \t -> \t My flow:%f \t Other: %f \t difference: %f\n",water_flow_rate,water_flow_rate_other,water_flow_diff);                             
                
                total_quantity=total_quantity+water_flow_diff/60*8;
                //sprintf(message, "{\"id\": \"%d\", \"flow_son\": \"%f\", \"flow_diff\": \"%f\", \"quantity leak\": \"%f\"}", board, water_flow_rate, water_flow_diff, total_quantity);             
                sprintf(message, "{\"time_s0_son\": \"%d\", \"time_s1_son\": \"%d\", \"sample_time_now\": \"%d\", \"sample_time_end\": \"%d\", \"time_s2_son\": \"%d\"}", time_s0_son, time_s1_son, sample_time_now, sample_time_end, time_s2_son);             
                message_on_aws=message;
                thread_wakeup(publish_thread_aws);
            }
            else{
                if(water_flow_rate-water_flow_rate_other > 0)
                {    water_flow_diff=water_flow_rate-water_flow_rate_other; }
                else
                {   water_flow_diff=water_flow_rate_other-water_flow_rate;  }

                printf("NO LEAKEGE\n");

                total_quantity=total_quantity+water_flow_diff/60*8;
                //sprintf(message, "{\"id\": \"%d\", \"flow_son\": \"%f\", \"flow_diff\": \"%f\", \"quantity leak\": \"%f\"}", board, water_flow_rate, water_flow_diff, total_quantity);             
                sprintf(message, "{\"time_s0_son\": \"%d\", \"time_s1_son\": \"%d\", \"sample_time_now\": \"%d\", \"sample_time_end\": \"%d\", \"time_s2_son\": \"%d\"}", time_s0_son, time_s1_son, sample_time_now, sample_time_end, time_s2_son);             
                message_on_aws=message;
                thread_wakeup(publish_thread_aws);
            }
        answer=0;    
        work=0;    
        }
    }
    memset(mex,0,strlen(mex));
    //memset(value_message_on_received,0,strlen(value_message_on_received));
}

void sub_to_topic(void)
{
    enum QoS qos = QOS0;
    printf("usage: %s <topic name> [QoS level]\n",DEFAULT_TOPIC_SUB);

    printf("mqtt_example: Subscribing to %s\n", DEFAULT_TOPIC_SUB);
    int ret = MQTTSubscribe(&client,DEFAULT_TOPIC_SUB, qos, _on_msg_received);
    if (ret < 0) {
        printf("mqtt_example: Unable to subscribe to %s (%d)\n",
               DEFAULT_TOPIC_SUB, ret);
        //_cmd_discon(0, NULL);
    }
    else {
        printf("\nmqtt_example: Now subscribed to %s, QOS %d\n",
               DEFAULT_TOPIC_SUB, (int) qos);
     
    }

    return;
}

int water_sensor_test(void){

    printf(" \n- - - - - - - - - - - - - - - - - - - - - - - - - - - - \n");
    printf("water sensor test start\n");
    
    int middle_turn=-1;
    int sample=0;
    float number_turn=0.0;
    
    sample = adc_sample(WATER_FLOW,ADC_RES);
    sample_time_now=xtimer_now();

    while(1){
        sample = adc_sample(WATER_FLOW,ADC_RES);
        
        if (middle_turn==0 && sample==4095){
            number_turn++;
        }
        middle_turn=sample;
        
        sample_time_end=xtimer_now();
        sample_time_diff=xtimer_diff(sample_time_end, sample_time_now); //microsecondi
        
        if(sample_time_diff > TESTING && number_turn==0.0){
            if(board!=0){
                break; //se no flow nella seconda va controllato il leakage
            }
            printf("No flow detected\n");
            return 0;
        }
        if(sample_time_diff > TESTING && number_turn!=0.0){
            printf("water flow detected\n");
            
            return 1;
        }
    }

    return 0;
}

void* water_sensor_sampling(void* arg){
    while(1){ 
        printf("water sensor  sampling thread start\n");
        
        int middle_turn=-1;
        int sample=0;

        float number_turn=0.0;
        float remain=0.0;
        float frequency=0.0;
        water_flow_rate=0.0;
        
        sample = adc_sample(WATER_FLOW,ADC_RES);

        if (sample==0){
            //middle_turn=0;
            remain=1;
        }
        else{ 
            //middle_turn=0;  
        }
        
        sample_time_now=xtimer_now();

        while(1){
            sample = adc_sample(WATER_FLOW,ADC_RES);
            
            if (middle_turn==0 && sample==4095){
                number_turn++;
            }
            middle_turn=sample;
            
            sample_time_end=xtimer_now();
            sample_time_diff=xtimer_diff(sample_time_end, sample_time_now); //microsecondi
            
            if(sample_time_diff > SAMPLING){
                break;
            }
        }

        if(remain!=0){
            number_turn=number_turn+remain;
        }
        
        printf("time: %d microseconds\t turns: %f\n",sample_time_diff,number_turn);
            
        //Q(L/s) = f/impulsi; -> Q(L/min) = f*60/impulsi  
        //usiamo per ora 541
        frequency=number_turn/(float)sample_time_diff*SECONDS;
        water_flow_rate=(float)frequency*60/541; 
        printf("frequency: %f Hz\t water_flow_rate: %f L/min\n",frequency,water_flow_rate);
        printf("water sensor thread end\n");
        if(board==0){
            time_s2_source = xtimer_now();
            sprintf(message, "%f", water_flow_rate);
            publish(message);
            
            total_quantity=total_quantity+water_flow_rate/60*8;
            //sprintf(message, "{\"id\": \"%d\", \"flow source\": \"%f\", \"quantity\": \"%f\"}", board, water_flow_rate, total_quantity);             
            sprintf(message, "{\"time_s0_source\": \"%d\", \"time_s1_source\": \"%d\", \"time_s2_source\": \"%d\"}", time_s0_source, time_s1_source, time_s2_source); 
            message_on_aws=message;
            thread_wakeup(publish_thread_aws);
        }

        thread_sleep();
    }

    return NULL;
}



int main(void)
{
    if(board==0){
        DEFAULT_TOPIC_SUB = "sub"; //subscribe
        DEFAULT_TOPIC_PUBLISH = "publish"; //publish
    }
    else{
        DEFAULT_TOPIC_SUB = "publish"; //subscribe
        DEFAULT_TOPIC_PUBLISH = "sub"; //publish
    }

    printf("You are running RIOT on a(n) %s board.\n", RIOT_BOARD);

    NetworkInit(&network);
 
    xtimer_sleep(3);

    MQTTClientInit(&client, &network, COMMAND_TIMEOUT_MS, buf, BUF_SIZE,
                   readbuf,
                   BUF_SIZE);

    printf("Running water-sensor example.\n");
  
    xtimer_sleep(3);

    MQTTStartTask(&client);
 
    while(ret<0){
        connect();
        xtimer_sleep(1);
    }

    // initialize the FLAME_PIN (ADC) line
    if (adc_init(WATER_FLOW) < 0) {
        printf("Initialization of ADC_LINE(%u) failed\n", WATER_FLOW);
        return 1;
    } else {
        printf("Successfully initialized ADC_LINE(%u)\n", WATER_FLOW);
    }
    publish_thread_node=thread_create(publish_stack_node, sizeof(publish_stack_node),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_SLEEPING,
                  publish_on_node, NULL, "publish_on_node");  
	
	water_sensor_thread=thread_create(water_sensor_stack, sizeof(water_sensor_stack),
                  THREAD_PRIORITY_MAIN + 1, THREAD_CREATE_SLEEPING,
                  water_sensor_sampling, NULL, "water_sensor");

    publish_thread_aws=thread_create(publish_stack, sizeof(publish_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_SLEEPING,
                  publish_on_aws, NULL, "publish_on_received");              

    sub_to_topic();
    periodic_time = xtimer_now(); 
    
    while(1){
        /*
        if(board==0){

            while(1){  
                if(water_sensor_test()==1){
                    time_s0_source = xtimer_now();
                    publish("heloy");
                    xtimer_periodic_wakeup(&periodic_time, ANSWER_AGAIN_PERIODIC);
                    while(1){
                        if(answer==0){
                            printf("To do again, no answer from son\n");
                            time_s0_source = xtimer_now();
                            publish("heloy");
                            xtimer_periodic_wakeup(&periodic_time, ANSWER_AGAIN_PERIODIC);
                        }
                        else{
                            break;
                        }
                    }
                    break;
                }   
                else{
                    printf("To do again, no water\n");
                    xtimer_periodic_wakeup(&periodic_time, NO_WATER_AGAIN_PERIODIC);
                } 
            }

            time_s1_source = xtimer_now();
            value_message_on_received="start";
            thread_wakeup(publish_thread_node);
            thread_wakeup(water_sensor_thread);
            //publish("start");
            
            printf("I am doing the sampling\n");
            xtimer_periodic_wakeup(&periodic_time, DAILY_PERIODIC);
            if(ret<0){
                connect();
                xtimer_sleep(1);
            }
        } 
        */
        if(board==0){

            while(1){  
                if(water_sensor_test()==1){
                    time_s0_source = xtimer_now();
                    publish("heloy");
                }
                xtimer_periodic_wakeup(&periodic_time, DAILY_PERIODIC);
                if(ret<0){
                    connect();
                    xtimer_sleep(1);
                }
            } 
        }

        if(board!=0){
            while(1){
                xtimer_periodic_wakeup(&periodic_time, DAILY_PERIODIC);
                if(ret<0){
                    connect();
                    xtimer_sleep(1);
                }
            }
        }  

    }

    printf("main thread end\n");

	return 0;
    
}    
