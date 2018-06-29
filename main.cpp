// ----------------------------------------------------------------------------
// Copyright 2016-2017 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#include "mbed.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed-trace-helper.h"
#include "simple-mbed-cloud-client.h"
#include "key-config-manager/kcm_status.h"
#include "key-config-manager/key_config_manager.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
//#include "ESP8266Interface.h"
#include "easy-connect.h"
#include "MMA7660.h"
#include "LM75B.h"

/* The following app uses Mbed Cloud with SD Card storage, button, & led */

// Placeholder to hardware that trigger events (timer, button, etc)
//Ticker timer;
/* K64 & K66                 */ 
InterruptIn sw2(SW2);
DigitalOut led2(LED2);
//ESP8266Interface net(D1, D0);
NetworkInterface *net;

/*                           */

// Placeholder for storage
/* K64 & K66                                    */
SDBlockDevice sd(PTE3, PTE1, PTE2, PTE4);
FATFileSystem fs("sd");
/*                                              */
 
// Pointers to the resources that will be created in main_application().
static MbedCloudClientResource* pattern_ptr;
static MbedCloudClientResource* button_ptr;

// Pointer to mbedClient, used for calling close function.
static SimpleMbedCloudClient *client;

static bool button_pressed = false;
static int button_count = 0;
      
void button_press() {
    button_pressed = true;
    ++button_count;
    button_ptr->set_value(button_count);
}

void pattern_updated(const char *) {
    printf("PUT received, new value: %s\n", pattern_ptr->get_value().c_str());
    // Placeholder for PUT action
}

void blink_callback(void *) {
    String pattern_str = pattern_ptr->get_value();
    const char *pattern = pattern_str.c_str();
    printf("POST received. LED pattern = %s\n", pattern);
    // Placeholder for POST action
    // The pattern is something like 500:200:500, so parse that.
    // LED blinking is done while parsing.

    while (*pattern != '\0') {
        //make a short blink on the led
        led2 = 0;
        wait_ms(20);
        led2 = 1; 
        // Wait for requested time.
        wait_ms(atoi(pattern));                
        // Search for next value.
        pattern = strchr(pattern, ':');
        if(!pattern) {
            //we're done, give one last blink to end the pattern
            led2 = 0;
            wait_ms(20);
            led2 = 1; 
            break; // while
        }
        pattern++;
    }
}

void button_callback(const M2MBase& object, const NoticationDeliveryStatus status)
{
    printf("Button notification. Callback: (%s)\n", object.uri_path());
    // Placeholder for GET
}

void accel_callback(const M2MBase& object, const NoticationDeliveryStatus status)
{
    // Do nothing.
}

void temp_callback(const M2MBase& object, const NoticationDeliveryStatus status)
{
    // Do nothing.
}




int main(void)
{
    // Requires DAPLink 245+ (https://github.com/ARMmbed/DAPLink/pull/364)
    // Older versions: workaround to prevent possible deletion of credentials:
    wait(2);

    // Misc OS setup
    srand(time(NULL));

    printf("Start Simple Mbed Cloud Client\n");

    // Initialize SD card
    int status = sd.init();
    if (status != BD_ERROR_OK) {
        printf("Failed to init SD card\r\n");
        return -1;
    }

    // Mount the file system (reformatting on failure)
    status = fs.mount(&sd);
    if (status) {
        printf("Failed to mount FAT file system, reformatting...\r\n");
        status = fs.reformat(&sd);
        if (status) {
            printf("Failed to reformat FAT file system\r\n");
            return -1;
        } else {
            printf("Reformat and mount complete\r\n");
        }
    }

    printf("Connecting to the network using Wi-Fi...\n");

    net = easy_connect(true);
//    status = net.connect(WIFI_SSID, WIFI_PSWD, NSAPI_SECURITY_WPA_WPA2);
    
    SimpleMbedCloudClient mbedClient(net);
    // Save pointer to mbedClient so that other functions can access it.
    client = &mbedClient;

    status = mbedClient.init();
    if (status) {
        return -1;
    }

    printf("Client initialized\r\n");

    // Mbed Cloud Client resource setup
    MbedCloudClientResource *button = mbedClient.create_resource("3200/0/5501", "button_resource");
    button->set_value("0");
    button->methods(M2MMethod::GET);
    button->observable(true);
    button->attach_notification_callback(button_callback);
    button_ptr = button;
    
    MbedCloudClientResource *pattern = mbedClient.create_resource("3201/0/5853", "pattern_resource");
    pattern->set_value("500:500:500:500");
    pattern->methods(M2MMethod::GET | M2MMethod::PUT);
    pattern->attach_put_callback(pattern_updated);
    pattern_ptr = pattern;

    MbedCloudClientResource *blink = mbedClient.create_resource("3201/0/5850", "blink_resource");
    blink->methods(M2MMethod::POST);
    blink->attach_post_callback(blink_callback);

    /* Accelerometer */
    const int NUM_AXIS = 3;
    MbedCloudClientResource* accel[NUM_AXIS];
    accel[0] = mbedClient.create_resource("3313/0/5702", "accel_x");
    accel[1] = mbedClient.create_resource("3313/0/5703", "accel_y");
    accel[2] = mbedClient.create_resource("3313/0/5704", "accel_z");
    for (int i=0; i < NUM_AXIS; i++) {
        accel[i]->set_value(0);
        accel[i]->methods(M2MMethod::GET);
        accel[i]->attach_notification_callback(accel_callback);
        accel[i]->observable(true);
    }
    MbedCloudClientResource* acc_unit = mbedClient.create_resource("3313/0/5701", "unit");
    acc_unit->set_value("G");

    /* Temperature */
    MbedCloudClientResource *temp = mbedClient.create_resource("3303/0/5700", "temperature");
    temp->set_value("0");
    temp->methods(M2MMethod::GET);
    temp->attach_notification_callback(temp_callback);
    temp->observable(true);
    MbedCloudClientResource *temp_unit = mbedClient.create_resource("3303/0/5701", "unit");
    temp_unit->set_value("Cel");

    mbedClient.register_and_connect();

    printf("Waiting for register and connect ");
    // Wait for client to finish registering
    while (!mbedClient.is_client_registered()) {
        printf(".");
        wait_ms(500);
    }
    printf("\n\n");

    // Placeholder for callback to update local resource when GET comes.
    //timer.attach(&button_press, 5.0);
      sw2.mode(PullUp);
      sw2.fall(button_press);
      button_count = 0;

    // For sensors
    LM75B lm75b(I2C_SDA, I2C_SCL);     // temperature
    MMA7660 mma7660(I2C_SDA, I2C_SCL); // accel

    const long measurementPeriod = 5;  // sec
    Timer timer;
    timer.start();


    // Check if client is registering or registered, if true sleep and repeat.
    while (mbedClient.is_register_called()) {
        //static int button_count = 0;
    
        wait_ms(100);

        if (button_pressed) {
            button_pressed = false;
            //printf("button clicked %d times\r\n", ++button_count);
            //button->set_value(button_count);
            printf("button clicked %d times\r\n", button_count);            
        }
        if( timer.read() > measurementPeriod) {
            timer.reset();
            float temperature, acc[3];
            const unsigned int buf_size = 20;
            char buf[buf_size];


            mma7660.readData(acc);
            for(int i=0; i < NUM_AXIS; i++) {
                snprintf(buf, buf_size, "%f", acc[i]);
                accel[i]->set_value(buf);
            }
            printf("acc: %f,%f,%f\n", acc[0], acc[1], acc[2]);

            temperature = lm75b.read();
            snprintf(buf, buf_size, "%f", temperature);
            temp->set_value(buf);
            printf("temp: %s\n", buf);
        }

    }
    // Client unregistered, exit program.
    return 0;
}
