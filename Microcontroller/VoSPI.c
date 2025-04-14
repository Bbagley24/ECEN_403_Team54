//Blake Bagley Remote Viewing System Code

/*----------------------------------------------------------------
This code has three key functions for the system:

1. Read picture data from the thermal imager

2. Use image processing to determine if switch is open or closed

3. Send both data to the desired website
----------------------------------------------------------------*/

//
#include <stdio.h>
#include <math.h>
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <string.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_heap_caps.h"
#include "esp_heap_caps_init.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_err.h"
#include "esp_event.h"
#include <float.h>


//GPIO Pin Definitions
#define CS_PIN      39//9 //     //Brown Stripe
#define SCLK_PIN    36//39//     //Pink
#define MOSI_PIN    35//35//     //Brown
#define MISO_PIN    37//14//     //Purple Stripe

//WiFi and HTTP definitions
#define SSID "BBagley"
#define PASS "TAMU24!$"
#define TAG "HTTP_CLIENT"


void app_main(void)
{
    
    printf("Initializing: \n");






//---------------SPI CONFIGURATION--------------------//

    spi_device_handle_t Lepton;

    spi_bus_config_t pins = { //GPIO pins
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1};

    
    spi_device_interface_config_t lepton_config = { //Communication configuration
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = 20000000,                 //20 MHz
        .duty_cycle_pos = 128,                      // 50% duty cycle
        .mode = 3,                                  //VSPI
        .spics_io_num = CS_PIN,                     //CS
        .queue_size = 1};

    spi_bus_initialize(SPI3_HOST, &pins, SPI_DMA_CH_AUTO);

    spi_bus_add_device(SPI3_HOST, &lepton_config, &Lepton);

//----------------------------------------------------//


//-------------------------------SET UP WIFI----------------------------------------//


    void wifi_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_CONNECTED:
            printf("CONNECTED \n");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            printf("DISCONNECTED from Wi-Fi. Retrying...\n");
            esp_wifi_connect();  // Attempt reconnection if disconnected
            break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            printf("Authentication mode changed.\n");
            break;
        default:
            break;
        }
    }

   nvs_flash_init();

   esp_netif_init();					
   esp_event_loop_create_default();
   esp_netif_create_default_wifi_sta();
   
   //Initialize WiFi
   wifi_init_config_t wifi_initialize_info = WIFI_INIT_CONFIG_DEFAULT();
   esp_wifi_init(&wifi_initialize_info);
   esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL);

   esp_wifi_set_mode(WIFI_MODE_STA); // Set to Access Point
   
   //Set WiFi Configurations
   wifi_config_t WiFi_Info = {.sta = {.ssid = SSID, .password = PASS}}; 
   esp_wifi_set_config(WIFI_IF_AP, &WiFi_Info);

   esp_wifi_start(); //Start WiFi

   esp_wifi_connect();
   printf("Connecting...");

   



//----------------------------------------------------------------------------------//


//------------------------------SET UP HTTP------------------------------------------//

void http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA: %.*s", evt->data_len, (char *)evt->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    
}

vTaskDelay(5000 / portTICK_PERIOD_MS);


printf("Initialized: \n");
printf("GOOOOOOOOOOOOOOOOO!!!!!!!!!!!!!!!\n\n");

//---------------Switch State Determination Variables----------------//
float rowSum = 0;                                               //sum of pixel values in a row
float frameAvg = 0;                                             //average value of pixels in a frame


int checkSum = 0;                                               //sum of pixel vaues in the window of check
float checkAvg = 0;                                             //average value of pixels in the window of check

float minHeat = FLT_MAX;                                        //initialized to first pixel value
float maxHeat = FLT_MIN;

float currHeat = 0;                                             //heat value of current pixel

//-------------------------------READ FRAME----------------------------------------//
/*
    This section works in this order:
    1. The first packet of the frame will be read. If it is a discard packet and/or not the starting packet,
            the code will loop until it reads the first packet correctly and store it's value in the frame array.

    2. Once it reads the first packet in, a loop of length 60 will run using the same checks and reads including a
            segment check. If the segment or packet IDs read bad, the storing of the frame is reset. For each segment,
            when one is stored the code will track all of the segments that have been stored into the frame.

    3. Once all segments have been read, the reading of the frame will end.
*/

while (true)
{
    char* packet = (char*)malloc(164 * sizeof(char));                       //buffer for video packet

    char **frame = malloc(240 * sizeof(char*));     //Total Frame Data
    
    for (int i = 0; i < 240; i++) {
        frame[i] = malloc(164 * sizeof(char));
    }


    char* Nframe = malloc(120 * 160 * sizeof(char)); //Total Normalized Frame Data
    
    vTaskDelay(250 / portTICK_PERIOD_MS);
    bool segment[4] = {false};                                              //array telling if a segment as been stored into frame

    spi_transaction_t receive;
    memset(&receive, 0, sizeof(receive));   //set buffer to default settings
    receive.length = 164 * 8;               //size of buffer
    receive.rx_buffer = packet;             //sets buffer to be the stored packet
    
    int index = 0; // index of frame to be read
    int error = 0;
    while(segment[0] != true || segment[1] != true || segment[2] != true || segment[3] != true){

        int segmentIndex = 0;                                       //index of current segement
        bool badSegment = false;
        if(error >= 1000){
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            error = 0;
        }
        printf(" Packet ID: %d %d %d %d %d \n", (int)packet[0], (int)packet[1], (int)packet[2], (int)packet[3], (int)packet[4]);
        vTaskDelay((rand() % 8) / portTICK_PERIOD_MS);
        spi_device_transmit(Lepton, &receive);                      //read for first row in segment
        error++;
        
        if(((int)packet[0] != 15 && (int)packet[0] != 31 && (int)packet[0] != 63 && (int)packet[0] != 127 && (int)packet[0] != 255) && (int)packet[1] == 0){ //checks for discard frames            

            memcpy(frame[index], packet, 164 * sizeof(char));       //store first row
            index++;               
            segmentIndex++;

            while(segmentIndex < 60)                                //stores the rest of the segment
            {   
                spi_device_transmit(Lepton, &receive);              //reads for rows
                if(((int)packet[0] != 15 && (int)packet[0] != 31 && (int)packet[0] != 63 && (int)packet[0] != 127 && (int)packet[0] != 255) && (int)packet[1] < 60){ //checks for discard frames
                    
                    if(((int)packet[1] != segmentIndex && ((int)packet[1] != 0  || segmentIndex != 60)) && !badSegment){                            
                        //printf("BAD FRAME!!! %d != %d\n\n", (int)packet[1], segmentIndex);                       
                    }

                    if(segmentIndex == 20){                         //Checks which segment is read
                        //printf("SEGMENT ID: %d , %d\n\n", (int)packet[0], (int)packet[1]);

                        if((int)packet[0] == 16){                   // Segment 1
                            segment[0] = true;
                        }
                        else if((int)packet[0] == 32){              // Segment 2
                            segment[1] = true;
                        }
                        else if((int)packet[0] == 48){              // Segment 3
                            segment[2] = true;
                        }
                        else if((int)packet[0] == 64){              // Segment 4
                            segment[3] = true;
                        }
                        else{                                       // Bad Segment
                            badSegment = true;
                        }
                    }
                    
                    memcpy(frame[index], packet, 164 * sizeof(char)); //stores rows
                    index++;
                    segmentIndex++;                       
                }
            }
            if(badSegment){                                           //resets frame for bad reading
                index = 0;
                memset(segment, false, sizeof(segment));
            }
        }
    }
    index = 0;
    memset(segment, false, sizeof(segment));

//-----------------------------------------------------------------------//
// Segment ID Test
//for(int i = 0; i < 163; i += 2){
//    if(i <= 2){
//        printf("ID %d: %d , %d \n", i, (int)frame[20][i], (int)frame[20][i+1]);
//    }
//    else{
//        printf("PIXEL %d: %d , %d \n", (i/2)-2, (int)frame[20][i], (int)frame[20][i+1]);
//    }
//    
//}



//------------------------Switch State Determination---------------------//
/*
1. The first thing done is the code will sift through the stored frame and determine the maximum and minimum 
    pixel values on the entire frame.

2. Then, the code will use the values found in the previous step to create a normalized version of the frame
    having the values range from 0 to 255.

3. Finally, the code will find the average intensity of the entire frame and the average intensity of an
    area of pixels where the contact of the switch can be located. It will then compare the two averages
    to determine if the switch is in one state or the other.
*/



rowSum = 0;                                                 //sum of pixel values in a row
frameAvg = 0;                                             //average value of pixels in a frame


checkSum = 0;                                               //sum of pixel vaues in the window of check
checkAvg = 0;                                             //average value of pixels in the window of check

minHeat = FLT_MAX;    
maxHeat = FLT_MIN;

currHeat = 0;                                             //heat value of current pixel


//Find MIN and MAX of Pixel intensity
for(int j = 0; j < 240; j += 1){
    for(int i = 4; i < 163; i += 2){
        currHeat = ((float)frame[j][i] * 128) + (float)frame[j][i+1];   //converts pixel data to one float value
        if(currHeat <= minHeat){                                         //finds minimum
            minHeat = currHeat;
        }
        if(currHeat >= maxHeat){                                         //finds maximum
            maxHeat = currHeat;
        }
    }
}


int side = 0; //0 means left
int sideCheck = 0; //checks which side the current packet is on
uint8_t newPixel = 0;
float pixelF = 0; 
int order = 0;

//Converts original frame data to 8-bit numbers
for(int j = 0; j < 240; j += 1){
    for(int i = 4; i < 163; i += 2){
        currHeat = ((float)frame[j][i] * 128) + (float)frame[j][i+1];                       //converts pixel data to one float value
        pixelF = (currHeat - minHeat) * (255/(maxHeat-minHeat));
        newPixel = (uint8_t)pixelF;

        if(sideCheck == 0){
            Nframe[order] = newPixel;
            //Nframe[j/2][(i-4)/2] = newPixel;
                
            side = 1;
        }
        else if(sideCheck == 1){     
            Nframe[order] = newPixel;
            //Nframe[(j-1)/2][((i-4)/2)+80] = newPixel; //converts values to range from 0-255 (right side of row)
            
            side = 0;
        }
        order++;
    }
    sideCheck = side; 
}


// Average for frame
for(int j = 0; j < 120 * 160 ; j += 1){
    rowSum += Nframe[j];
}
frameAvg = rowSum/(160 * 120);


// Average for window of check
int pixel = 0;                          //# of pixels in window
for(int j = 23; j < 54 ; j += 1){ //j = row, i = col
    for(int i = 31; i < 63; i += 1){
        //checkSum += Nframe[j][i];       //adds all pixel values
        checkSum += Nframe[(160 * j) + i];
        pixel++;
    }
}

for(int j = 17; j < 49 ; j += 1){ //j = row, i = col
    for(int i = 21; i < 53; i += 1){
        if( (i == 21 || i == 52)  || (j == 17 || j == 48)){
            Nframe[(160 * j) + i] = 255;
        }

    }
}
checkAvg = checkSum/(pixel);            //calculates average

printf("Frame Average: %f \n", frameAvg);
printf("Check Average: %f \n", checkAvg);




//--------------------------------------------------------------------//

//-------Prints values for MATLAB test--------------//
//order = 0;
//for(int i = 0; i < 120 ; i++){
//    for(int j = 0; j < 160; j++){
//        printf("%d ", (int)Nframe[order]);
//        order++;
//    }
//        printf("; \n");                
//}


//------------------------Website Communication---------------------//
/*
1. The microcontroller will set the website as the desired desitnation and send headers of info
like the switch-state determination and the image data
*/
esp_http_client_config_t config_post = {
    .host = "10.247.214.208",
    .port = 3000,
    .method = HTTP_METHOD_POST,
    .cert_pem = NULL,
    .event_handler = http_event_handler,
    .path = "/ping",
    .timeout_ms = 20000};

esp_http_client_handle_t client = esp_http_client_init(&config_post);

esp_http_client_set_post_field(client, Nframe , 160 * 120 * sizeof(char));
esp_http_client_set_header(client, "Content-Type", "application/octet-stream");


if((checkAvg-frameAvg)/(frameAvg) > .20){ //compares averages to determine switch state
    printf("SWITCH STATE 1 \n");
    esp_http_client_set_header(client, "Switch-State", "Closed");
}
else{
    printf("SWITCH STATE 2 \n");
    esp_http_client_set_header(client, "Switch-State", "Open");
}


esp_http_client_perform(client);
esp_http_client_cleanup(client);


//-------------------------Freeing Memory-----------------------------//
for (int i = 0; i < 240; i++) { //Frees Frame Storage
    free(frame[i]);
}
free(frame);

free(Nframe);                   //Frees Normalized Frame Storage

free(packet);                   //Frees Packet Storage
}
vTaskDelay(500 / portTICK_PERIOD_MS);









}
