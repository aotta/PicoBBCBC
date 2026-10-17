/*
//                       PicoBBCBC MultiCART by Andrea Ottaviani 
// BBC Bridge Companion  multicart based on Raspberry Pico "Purple" clone board -

// v. 1.0 2026-10-16 : Initial version for Pi Pico 
//
// 
//  More info on https://github.com/aotta/ 
*/

#include "hardware/gpio.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "string.h"
#include "splash.h"

// include for Flash files
#include "SdFat_Adafruit_Fork.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"

Adafruit_FlashTransport_RP2040 flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

FatVolume fatfs;
File32 root;
File32 file;
File32 romFile;

Adafruit_USBD_MSC usb_msc;
bool fs_formatted;
bool fs_changed = false;


// Pico pin usage definitions

#define A0_PIN    0
#define A1_PIN    1
#define A2_PIN    2
#define A3_PIN    3
#define A4_PIN    4
#define A5_PIN    5
#define A6_PIN    6
#define A7_PIN    7
#define A8_PIN    8
#define A9_PIN    9
#define A10_PIN  10
#define A11_PIN  11
#define A12_PIN  12
#define A13_PIN  13
#define A14_PIN  14
#define A15_PIN  15
#define D0_PIN   16
#define D1_PIN   17
#define D2_PIN   18
#define D3_PIN   19
#define D4_PIN   20
#define D5_PIN   21
#define D6_PIN   22
#define D7_PIN   23
#define MREQ_PIN  24
#define ROMOE_PIN  25
#define CEROM1_PIN  26
#define CEROM2_PIN  27
#define CEROM3_PIN  28 
#define CEROM4_PIN  29 

// Pico pin usage masks

#define A0_PIN_MASK     0x00000001L
#define A1_PIN_MASK     0x00000002L
#define A2_PIN_MASK     0x00000004L
#define A3_PIN_MASK     0x00000008L
#define A4_PIN_MASK     0x00000010L
#define A5_PIN_MASK     0x00000020L
#define A6_PIN_MASK     0x00000040L
#define A7_PIN_MASK     0x00000080L
#define A8_PIN_MASK     0x00000100L
#define A9_PIN_MASK     0x00000200L
#define A10_PIN_MASK    0x00000400L
#define A11_PIN_MASK    0x00000800L
#define A12_PIN_MASK    0x00001000L
#define A13_PIN_MASK    0x00002000L
#define A14_PIN_MASK    0x00004000L
#define A15_PIN_MASK    0x00008000L
#define D0_PIN_MASK     0x00010000L
#define D1_PIN_MASK     0x00020000L
#define D2_PIN_MASK     0x00040000L
#define D3_PIN_MASK     0x00080000L
#define D4_PIN_MASK     0x00100000L
#define D5_PIN_MASK     0x00200000L  // gpio 21
#define D6_PIN_MASK     0x00400000L
#define D7_PIN_MASK     0x00800000L

#define MREQ_PIN_MASK   0x01000000L //gpio 24
#define ROMOE_PIN_MASK  0x02000000L
#define CEROM1_PIN_MASK 0x04000000L  //gpio 26
#define CEROM2_PIN_MASK 0x08000000L
#define CEROM3_PIN_MASK 0x10000000L
#define CEROM4_PIN_MASK 0x20000000L

// Aggregate Pico pin usage masks
#define ALL_GPIO_MASK  	0x3FFFFFFFL
#define BUS_PIN_MASK    0x0000FFFFL
#define DATA_PIN_MASK   0x00FF0000L
#define FLAG_MASK       0x2F000000L
#define CEROM_MASK ( CEROM1_PIN_MASK | CEROM2_PIN_MASK)
#define ROM_MASK ( MREQ_PIN_MASK  | CEROM1_PIN_MASK | CEROM2_PIN_MASK)
#define ALWAYS_IN_MASK  (BUS_PIN_MASK | FLAG_MASK)

#define SET_DATA_MODE_OUT   gpio_set_dir_out_masked(DATA_PIN_MASK)
#define SET_DATA_MODE_IN    gpio_set_dir_in_masked(DATA_PIN_MASK)
// We're going to erase and reprogram a region 256k from the start of flash.
// Once done, we can access this at XIP_BASE + 256k.


char RBLo,RBHi;
#define BINLENGTH  65536L
unsigned char ROM[BINLENGTH];
unsigned char game[BINLENGTH];
unsigned char files[256*256] = {0};
unsigned char nomefiles[32*25] = {0};
char curPath[256] = "";
char path[256];
int fileda=0,filea=0;
volatile uint32_t Gaddr;
volatile bool cmd_executing=false;
volatile uint32_t Choosen;

// Callbacks USB MSC
int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  return flash.readBlocks(lba, (uint8_t*)buffer, bufsize / 512) ? bufsize : -1;
}

int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}

void msc_flush_cb(void) {
  flash.syncBlocks();
  fatfs.cacheClear();
  fs_changed = true;
}


////////////////////////////////////////////////////////////////////////////////////
//                     REBOOT
////////////////////////////////////////////////////////////////////////////////////
void doReboot() {
  rp2040.reboot();
}
////////////////////////////////////////////////////////////////////////////////////
//                     HANDLE BUS
////////////////////////////////////////////////////////////////////////////////////

void __time_critical_func(loop1()) {   //HandleBUS()
  char dataWrite=0;
  uint32_t pins=0;
  uint32_t addr;
  
  // Main loop   
   
  gpio_set_dir_in_masked(ALWAYS_IN_MASK);
  
  // Initial conditions
  SET_DATA_MODE_IN;
  Choosen=0;
  cmd_executing=false;  
  while (1)
 {
  while (!cmd_executing) {
 // while (1) {
    while (!(pins = gpio_get_all() & (CEROM_MASK ))); //removed for compatibility with SG-1000, which doesn't have IOR
   
    pins = gpio_get_all();   // to keep for SG-1000 compatibility, we read the pins again after the wait for MREQ to go low
	
    addr = (pins & (BUS_PIN_MASK ) );
    Gaddr=addr;
    if ((addr>=0x5000)&&(addr<=0x5008))  {
        Choosen=(addr&0xf)+1;
    }
    if (!(pins & CEROM1_PIN_MASK)) {
       SET_DATA_MODE_OUT;
		    gpio_put_masked(DATA_PIN_MASK, ROM[addr&0x3fff] << 16);  
        // Sync: wait end of read from CPU Z80
        while (!(gpio_get_all() & (CEROM1_PIN_MASK)));  // removed for compatibility with Mark III
        SET_DATA_MODE_IN;
    }
    }

    while (!(pins = gpio_get_all() & (CEROM_MASK ))); //removed for compatibility with SG-1000, which doesn't have IOR
   
    pins = gpio_get_all();   // to keep for SG-1000 compatibility, we read the pins again after the wait for MREQ to go low
	   
  if (Choosen==3) {  // bridge builder è su 4 rom?
    addr = (pins & (BUS_PIN_MASK ) & 0x1FFF);
 
    if (!(pins & CEROM1_PIN_MASK)) {
       SET_DATA_MODE_OUT;
		    gpio_put_masked(DATA_PIN_MASK, ROM[addr] << 16);  
        // Sync: wait end of read from CPU Z80
        while (!(gpio_get_all() & (CEROM1_PIN_MASK)));  // removed for compatibility with Mark III
        SET_DATA_MODE_IN;
    }
    if (!(pins & CEROM2_PIN_MASK)) {
      addr=addr | 0x2000;
     SET_DATA_MODE_OUT;
		    gpio_put_masked(DATA_PIN_MASK, ROM[addr] << 16);  
        // Sync: wait end of read from CPU Z80
        while (!(gpio_get_all() & (CEROM2_PIN_MASK)));  // removed for compatibility with Mark III
        SET_DATA_MODE_IN;
    }
    if (!(pins & CEROM3_PIN_MASK)) {
      addr=addr | 0x4000;
     SET_DATA_MODE_OUT;
		    gpio_put_masked(DATA_PIN_MASK, ROM[addr] << 16);  
        // Sync: wait end of read from CPU Z80
        while (!(gpio_get_all() & (CEROM3_PIN_MASK)));  // removed for compatibility with Mark III
        SET_DATA_MODE_IN;
    }
    if (!(pins & CEROM4_PIN_MASK)) {
      addr=addr | 0x6000;
     SET_DATA_MODE_OUT;
		    gpio_put_masked(DATA_PIN_MASK, ROM[addr] << 16);  
        // Sync: wait end of read from CPU Z80
        while (!(gpio_get_all() & (CEROM4_PIN_MASK)));  // removed for compatibility with Mark III
        SET_DATA_MODE_IN;
    }

    } else {
    addr = (pins & (BUS_PIN_MASK ) & 0x3FFF);
 
    if (!(pins & CEROM1_PIN_MASK)) {
        SET_DATA_MODE_OUT;
		    gpio_put_masked(DATA_PIN_MASK, ROM[addr] << 16);  
        // Sync: wait end of read from CPU Z80
        while (!(gpio_get_all() & (CEROM1_PIN_MASK)));  // removed for compatibility with Mark III
        SET_DATA_MODE_IN;
    }
    if (!(pins & CEROM2_PIN_MASK)) {
      addr=addr | 0x4000;
     SET_DATA_MODE_OUT;
		    gpio_put_masked(DATA_PIN_MASK, ROM[addr] << 16);  
        // Sync: wait end of read from CPU Z80
        while (!(gpio_get_all() & (CEROM2_PIN_MASK)));  // removed for compatibility with Mark III
        SET_DATA_MODE_IN;
    } 
	}
 }

}
////////////////////////////////////////////////////////////////////////////////////
//                     RESET Z80
////////////////////////////////////////////////////////////////////////////////////    
void reset() {
  
  rp2040.idleOtherCore();
  while (!(gpio_get_all() & CEROM_MASK));
  SET_DATA_MODE_OUT;
  gpio_put_masked(DATA_PIN_MASK,0xc7<<16);
  while (!(gpio_get_all() & CEROM_MASK));
  SET_DATA_MODE_IN;

   while (!(gpio_get_all() & CEROM_MASK));
  SET_DATA_MODE_OUT;
  gpio_put_masked(DATA_PIN_MASK,0xc7<<16);
  while (!(gpio_get_all() & CEROM_MASK));
  SET_DATA_MODE_IN;

  
   while (!(gpio_get_all() & CEROM_MASK));
  SET_DATA_MODE_OUT;
  gpio_put_masked(DATA_PIN_MASK,0xc7<<16);
  while (!(gpio_get_all() & CEROM_MASK));
  SET_DATA_MODE_IN;
  rp2040.resumeOtherCore();
}       

////////////////////////////////////////////////////////////////////////////////////
//                     LOAD ROM
////////////////////////////////////////////////////////////////////////////////////
void loadROM(){ 
  String riga;
  int numErr=0;
  
  for(int i=0;i<(sizeof(_actest));i++){
    unsigned int dataW= _actest[i];
    ROM[i]=dataW;
  }
}
////////////////////////////////////////////////////////////////////////////////////
//                     LOAD Game
////////////////////////////////////////////////////////////////////////////////////
void LoadGame(){ 
   switch (Choosen) { 	
        case 1: 
            load_file("1_advbidng.bin");
            break;
        case 2: 
            load_file("2_advdefnc.bin");
            break; 
        case 3: 
            load_file("3_bbuilder.bin");
            break; 
        case 4: 
            load_file("4_cplay1.bin");
            break; 
        case 5: 
            load_file("5_cplay2.bin");
            break; 
        case 6: 
            load_file("6_cplay3.bin");
            break; 
        case 7: 
            load_file("7_convent1.bin");
            break; 
        case 8: 
            load_file("8_dublict1.bin");
            break; 
        case 9: 
            load_file("9_mplay1.bin");
            break; 
        default: 
            Serial.print("Numero non compreso\n"); 
            break; 
    } 

   
   // vreg_set_voltage(VREG_VOLTAGE_1_20); // set to 1_15 or 1_20 if you experience some glitches
   // sleep_ms(10);
   // set_sys_clock_khz(270000, true); // settled in compiler IDE as 250mhz overclocked
  
    reset(); 
   	memcpy(ROM,game,BINLENGTH);
	  
  
    reset(); 
    
  }  


/* load file in  ROM */

/////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////* load file in  ROM */////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

void load_file(const char *filename) {
    Serial.print("load file: ");
    Serial.println(filename);

    if (!(file.open(filename))) {
        Serial.print("Can't open file ");
        Serial.println(filename);
        return;
    }

    // 🔥 AZZERA IL BUFFER PRIMA DI LEGGERE
    memset(game, 0, BINLENGTH);
    
    int n = file.read(game, BINLENGTH);
    Serial.print("Read: ");
    Serial.print(n);
    Serial.println(" bytes");
    file.close();
}


////////////////////////////////////////////////////////////////////////////////////
//                     SETUP
////////////////////////////////////////////////////////////////////////////////////

void setup() {

  //
   
  gpio_init_mask(ALL_GPIO_MASK);
  gpio_set_dir_in_masked(ALWAYS_IN_MASK);

  
 // Initialize the bus state variables

bool carton = false;

  /* 


 int testpin=MREQ_PIN; 
  pinMode(testpin,INPUT);

  //while(!Serial);
 

 while (gpio_get(testpin) == 1 && to_ms_since_boot(get_absolute_time()) < 1600) {
    if (gpio_get(testpin) == 1) {
    carton = true;  
    }
    sleep_ms(1);
  }
    
   if (gpio_get(testpin) == 1) {
    carton = true;  
    }
*/
  carton=true;  // poivia


  vreg_set_voltage(VREG_VOLTAGE_1_15); // set to 1_15 or 1_20 if you experience some glitches
  sleep_ms(10);
  set_sys_clock_khz(250000, true); // settled in compiler IDE as 250mhz overclocked


   // Inizializza flash
  flash.begin();
  
  // Configura USB MSC
  usb_msc.setID("BBC-BCO", "External Flash", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  usb_msc.setCapacity(flash.size() / 512, 512);
  usb_msc.setUnitReady(true);
  usb_msc.begin();
  
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    sleep_ms(10);
    TinyUSBDevice.attach();
    sleep_ms(10);
  }
  
  // Monta filesystem
  fs_formatted = fatfs.begin(&flash);
  

  if (!carton) {
    // Modalità PC
    Serial.println("P-ON your BBC Bridge Companion!");
    usb_msc.setUnitReady(true);

    while (1) {
      pinMode(25,OUTPUT);
      gpio_put(25,1);
      delay(100);
      gpio_put(25,0);
      delay(500);
    }
  }


     Serial.println("BBC Bridge Companion! Powered on");
 
  // === MODALITÀ CONSOLE ===
  usb_msc.setUnitReady(false);
  
    reset(); 
  memcpy(ROM,_actest,sizeof(_actest));
  reset();   
  
   
}
////////////////////////////////////////////////////////////////////////////////////
//                     MAIN LOOP
////////////////////////////////////////////////////////////////////////////////////    
  
void loop() 

{

	 	
 if ((Choosen)&&(!cmd_executing)) {
    Serial.print(Gaddr,HEX);
    Serial.print("-");Serial.print(to_ms_since_boot(get_absolute_time()));
    Serial.print("-");Serial.println(Choosen,DEC);
   // delay(400);
    LoadGame();
    //Choosen=0;
    //delay(400);
    cmd_executing=true;
  }
 // Serial.print(".");

 }

////////////////////////////////////////////////////////////////////////////////////
//                     MAIN LOOP
////////////////////////////////////////////////////////////////////////////////////    
  
void setup1()
{
   
}
