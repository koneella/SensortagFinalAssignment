
/*
Samu Ahola
Janne Korhonen
*/


#include <stdio.h>
#include <math.h>
#include <string.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/drivers/I2C.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>

void record(int i, float ax, float ay, float az, float gx, float gy, float gz);
int tunnista(float array[200][7]);

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/bmp280.h"
#include "sensors/mpu9250.h"

/* Task */
#define STACKSIZE 2048
char mainStack[STACKSIZE];
char sensorStack[STACKSIZE];
char commStack[STACKSIZE];

enum state { ALOITUS=1, TUNNISTUS, SUCCESS, FAIL };
enum state myState = ALOITUS;

int tulos = -1;

char str[100];
float mittausData[200][7];


/* Display */
Display_Handle hDisplay;

// JTKJ: Pin configuration and variables here
// JTKJ: Painonappien konfiguraatio ja muuttujat
	static PIN_Handle buttonHandle;
	static PIN_State buttonState;
	
	static PIN_Handle ledHandle;
	static PIN_State ledState;

	
PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE, // Hox! TAI-operaatio
   PIN_TERMINATE // Määritys lopetetaan aina tähän vakioon
};


// Ledipinni
PIN_Config ledConfig[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, 
   PIN_TERMINATE // Määritys lopetetaan aina tähän vakioon
};

static PIN_Handle hMpuPin;
static PIN_State MpuPinState;
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};
 
void tunnistusNaytto(){
    Display_print0(hDisplay, 2, 1, "Tunnistetaan....");

}

void aloitusNaytto(){
    Display_print0(hDisplay, 1, 2, "Paina");
    Display_print0(hDisplay, 2, 2, "nappainta 1");
    Display_print0(hDisplay, 3, 2, "aloittaaksesi");
    Display_print0(hDisplay, 4, 2, "eleen");
    Display_print0(hDisplay, 5, 2, "tunnistaminen");
}

void successNaytto(){
    Display_print0(hDisplay, 2, 1, "Eleen tunnistus onnistui!");
    Display_print0(hDisplay, 3, 1, "Ele on: ");
    
    if(tulos == 1){
        Display_print0(hDisplay, 4, 1, "Ympyra");
    } else if (tulos == 2) {
        Display_print0(hDisplay, 4, 1, "Highfive");
    }
    
}

void failNaytto(){
    Display_print0(hDisplay, 2, 1, "epaonnistui =(");
}


void tulostus() {
    
    int j;
    for(j = 0; j < 200; j++){
        
        (str, "{%lf, %.4lf, %.4lf, %.4lf, %.4lf, %.4lf, %.4lf},\n",mittausData[j][0], mittausData[j][1], mittausData[j][2], mittausData[j][3], mittausData[j][4], mittausData[j][5], mittausData[j][6]);
        System_printf("%s", str);
        System_flush();
        Task_sleep(1000 / Clock_tickPeriod);
        
    }
    return 0;
}


// painonapin keskeytyksen käsittelijäfunktio
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    
if(pinId == Board_BUTTON0) {
   if (myState == ALOITUS) {
       myState = TUNNISTUS;
       System_printf("tunnistus");

   }
   else if (myState == TUNNISTUS) {
       //myState = SUCCESS;
       System_printf("success");
        
   }
   else if (myState == SUCCESS) {
       myState = ALOITUS;
       System_printf("aloitus");
   }
   else if (myState == FAIL){
       myState = ALOITUS;
   }
}


}


void sensorTask(UArg arg0, UArg arg1) {
    
    
    I2C_Handle      i2c;
    I2C_Params      i2cParams;
    I2C_Handle      i2cMPU;
    I2C_Params      i2cMPUParams;
    
    double pres,temp;
    float ax, ay, az, gx, gy, gz;
    char str[200];
    char message[16] = "default";
    

    
    // Create I2C for sensors
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;
    

    //Open i2cmpu
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }
    else {
        System_printf("I2CMPU Initialized!\n");
    }

    // Power on
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);
    Task_sleep(1000000 / Clock_tickPeriod);
    
    System_flush();
    
    // Setup and calibration
    mpu9250_setup(&i2cMPU);
    I2C_close(i2cMPU);

    // BMP280 OPEN i2C
    i2c = I2C_open(Board_I2C, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }
    else {
        System_printf("I2C Initialized!\n");
    }

    bmp280_setup(&i2c);
    I2C_close(i2c);

    while (1) {

        //BMP280 OPEN i2C
        i2c = I2C_open(Board_I2C, &i2cParams);
	    if (i2c == NULL) {
	        System_abort("Error Initializing I2C11\n");
	    }

        bmp280_get_data(&i2c, &pres, &temp);
        temp = ((temp - 32) * 5) / 9;
        sprintf(str, "%.1lf celsius", temp);
        Display_print0(hDisplay, 10, 1, str);
        
        
        I2C_close(i2c);
        
        // OPEN MPU I2C
        i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
	    if (i2cMPU == NULL) {
	        System_abort("Error Initializing I2CMPU\n");
	    }
        
    
        if(myState == TUNNISTUS){
            int i;
            for(i = 0; i < 200; i++){
                mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
                record(i, ax, ay, az, gx, gy, gz);
                Task_sleep(20 * 1000 / Clock_tickPeriod);
            }
            
            tulos = tunnista(mittausData);
            if(tulos == 1 | tulos == 2){
                if(tulos == 1){
                    strcpy(message, "Ympyra!");
                } else if(tulos == 2){
                    strcpy(message, "Ylavitonen!");
                }
                Send6LoWPAN(IEEE80154_SERVER_ADDR, message, strlen(message));
                Task_sleep(100000);
                StartReceive6LoWPAN();
                myState = SUCCESS;
            } else {
                myState = FAIL;
            }
        }
            
        

        I2C_close(i2cMPU);
    	// Kerran sekunnissa
    	Task_sleep(1000000 / Clock_tickPeriod);
    	
    }
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
}

void record(int i, float ax, float ay, float az, float gx, float gy, float gz){
    
            
    mittausData[i][0] = i;
    mittausData[i][1] = ax;
    mittausData[i][2] = ay;
    mittausData[i][3] = az;
    mittausData[i][4] = gx;
    mittausData[i][5] = gy;
    mittausData[i][6] = gz;
    
}

int tunnista(float array[200][7]){

    float keskiarvoX, keskiarvoY, keskiarvoZ;
    float sumX = 0.f, sumY = 0.f, sumZ = 0.f;
    float sumX1 = 0.f, sumY1 = 0.f, sumZ1 = 0.f;
    float varianssiX, varianssiY, varianssiZ;
    float keskihajontaX, keskihajontaY, keskihajontaZ;


    // lasketaan taulukon alkiot yhteen
    int i;
    for(i = 0; i < 200; i++) {
        sumX += array[i][1];
        sumY += array[i][2];
        sumZ += array[i][3];
    }

    //lasketaan keskiarvot 
    keskiarvoX = sumX / 200;
    keskiarvoY = sumY / 200;
    keskiarvoZ = sumZ / 200;

    //välitulokset
    for(i = 0; i < 200; i++){

        sumX1 += pow((array[i][1] - keskiarvoX), 2);
        sumY1 += pow((array[i][2] - keskiarvoY), 2);
        sumZ1 += pow((array[i][3] - keskiarvoZ), 2);
    }

    //varianssi
    varianssiX = sumX1 / 200;
    varianssiY = sumY1 / 200;
    varianssiZ = sumZ1 / 200;

    //keskihajonta
    keskihajontaX = sqrt(varianssiX);
    keskihajontaY = sqrt(varianssiY);
    keskihajontaZ = sqrt(varianssiZ);
    
    //ympyrä
    if(0.25 < varianssiZ & varianssiZ < 0.8 & keskiarvoY < -0.08){
        return 1;
    //highfive
    } else if(0.30 < keskihajontaX & keskihajontaX < 0.60 & varianssiZ > 1.0) {
        return 2;
    //ei kumpikaan
    } else {
        return 3;
    }

    
    //printf("varianssiX = %lf keskihajontaX = %lf \n", varianssiX, keskihajontaX);
    //printf("varianssiY = %lf keskihajontaY = %lf \n", varianssiY, keskihajontaY);
    //printf("varianssiZ = %lf keskihajontaZ = %lf \n", varianssiZ, keskihajontaZ);
}


void commTask(UArg arg0, UArg arg1){
    
    uint16_t senderAddr;
    char payload[16];
    char str[100];
    
   int32_t result = StartReceive6LoWPAN();
   if(result != true) {
      System_abort("Wireless receive start failed");
   }
   

   while (1) {
       
       if(GetRXFlag() == 1) {
           
           // tyhjennys
           memset(payload,0,16);
           Receive6LoWPAN(&senderAddr, payload, 16);
           System_printf(payload);
           sprintf(str, "%s", payload);
           Display_print0(hDisplay, 9, 1, str);
           System_flush();
           
       }
       
   }
    
}



void mainTask(UArg arg0, UArg arg1){
   enum state lastState = myState;
   Display_Params params;
   Display_Params_init(&params);
   params.lineClearMode = DISPLAY_CLEAR_BOTH;

   // Näyttö käyttöön ohjelmassa
   hDisplay = Display_open(Display_Type_LCD, &params);

   Display_clear(hDisplay);
   System_flush();
    

    while (1) {
        switch (myState) {
            case ALOITUS:
                aloitusNaytto();
                break;
            case TUNNISTUS:
                tunnistusNaytto();
                break;
            case SUCCESS:
                successNaytto();
                break;
            case FAIL:
                failNaytto();
                break;
        }
        
        Task_sleep( 100000 / Clock_tickPeriod);
        if (myState != lastState) {
            Display_clear(hDisplay);
        }
        lastState = myState;
    }
    
}


int main(void) {



    // Task variables
   Task_Params mainParams;
   Task_Handle mainHandle;
   Task_Params sensorParams;
   Task_Handle sensorHandle;
   Task_Params commParams;
   Task_Handle commHandle;
   
    // Init comms
    Init6LoWPAN();

    // Initialize board
    Board_initGeneral();
    Board_initI2C();
    
    

    // Nappi ja ledi käyttöön
   buttonHandle = PIN_open(&buttonState, buttonConfig);
   if(!buttonHandle) {
      System_abort("Error initializing button pins\n");
   }
   ledHandle = PIN_open(&ledState, ledConfig);
   if(!ledHandle) {
      System_abort("Error initializing LED pins\n");
   }
   
    // Suoritusparametrit, pinomuisti, prioriteetti
    Task_Params_init(&mainParams);
    mainParams.stackSize = STACKSIZE;
    mainParams.stack = &mainStack;
    mainParams.priority = 2;
    
    // Taskin luonti 
    mainHandle = Task_create((Task_FuncPtr)mainTask, &mainParams, NULL);
    if(mainHandle == NULL){
        System_abort("Task create failed");
    }

    Task_Params_init(&sensorParams);
    sensorParams.stackSize = STACKSIZE;
    sensorParams.stack = &sensorStack;
    sensorParams.priority = 2;
    
    sensorHandle = Task_create((Task_FuncPtr)sensorTask, &sensorParams, NULL);
    if(sensorHandle == NULL){
        System_abort("Task create failed");
    }
    
    Task_Params_init(&commParams);
    commParams.stackSize = STACKSIZE;
    commParams.stack = &commStack;
    commParams.priority = 1;

    commHandle = Task_create((Task_FuncPtr)commTask, &commParams, NULL);
    if(commHandle == NULL){
        System_abort("Task create failed");
    }
    
   if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
      System_abort("Error registering button callback function");
   }
   
    System_flush();
    /* Start BIOS */
    BIOS_start();

    return (0);
}

