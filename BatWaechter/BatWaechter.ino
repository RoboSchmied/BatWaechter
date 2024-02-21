#include <EEPROM.h>
// Schuetzt Batterie vor Tiefenendladung durch Abschaltung
// "grosser" Verbraucher bei Unterspannung
// v0.1  Baterie-Spannung messen  A1
// v0.2  Solar-Strom messen A2
// v0.3  ohne Delay, viele Messungen und Schnitt nach Delay-Wert ausgeben
// v0.4  Weiterer Amp Messer auf A3    VerbraucherZähler oder BW-Melder-AmpScanner
//       Anti Cubieboard Failsafe: wait after "CPU:" message from uboot-loader
//       prevents stop of booting when Ardu serial output triggered Cubie-Failsafe mode
// v0.5  monostable relay switching emergency akku loading when voltage low
//       Switch Relais from serial
//       MilliMode
//       WaechterMode 0 1 2  
// v0.6  PengMode 0 1 2

/*
DisplayMoreThan5V sketch
prints the voltage on analog pin to the serial port
Do not connect more than 5 volts directly to an Arduino pin.
*/
String Version="v0.6";
String sss;
String ss;
boolean Debug=false;  // Debug outputs like raw voltages
// Ausschalten bei ..V
double VoltSchaltAus=9;
int Delay=800;
//Wiedereinschalten bei ..V
double VoltSchaltAn=11.3;

int WaechterModus=1;  // 0=aus 1=Akkuschonung(Rechner aus bei low V) 2=Immer An mit 230V Ladegeraet

const int PLed=13;
//Relai 1 (bi)
const int RPinAus=3;  // Digital 3 = Relay Aus Pin (reset): bistab. 2 coil Relai
const int RPinAn=4;   // 4 = set
boolean RelayStatus=false;
boolean MilliMode=true; // true = mA mV  false = Volts Amp
boolean ErstSchalt=true; // Erster Schaltvorgang (Relay-Stellung unbekannt)
boolean PengMode=0;  // Relay mit Tastendruck schalten 0=aus 1=Schalter 2=kurz An
//Relai 2 (mono)
const int R2Pin=7;  //monostab. Relai 2 an  D7
boolean R2Status=false;

// reserviert für Relai bistab. 2 = R3
//Relay 3 (bi) reserviert, anschlüsse sind schon angelötet
const int R3PinAus=5;  // PWM Digital 5 = Relay Aus Pin (reset): bistab. 2 coil Relai
const int R3PinAn=6;   // PWM set ON
boolean R3Status=false;
boolean R3ErstSchalt=true; // Erster Schaltvorgang (Relay-Stellung unbekannt)



// Ampere Messung
//int ACSoffset = 2430;  //2500
int ACSoffset = 2497;  //2500
int ACSoffset2 = 2497;  //2500


const double referenceVolts = 5.05; // the default reference on a 5-volt board
//const float referenceVolts = 3.3; // use this 
const double R1 = 2000; // value for a maximum voltage of 20 volts
const double R2 = 4000;

// serial Event
String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete


// determine by voltage divider resistors, see text


//const double resistorFactor = 255 / (R2/(R1 + R2));
const double rf2=(R2+R1)/R1;
//const float resistorFactor = 1023 / (R2/(R1 + R2));

// Anzahl der Spannungsmesser-Eingaenge
const int AnzSensoren=1;  
int val=0;
int i=1;
//const int batteryPin[]={A0,A1};  //element 0 wird nicht genutzt
int batteryPin=A1;  

//Schnitt-Bildung
double SummeVolt;
double SummeAmp;
double SummeRawAmp;
double SummeAmp2;  // Messgerät 2
double SummeRawAmp2;

int AnzMessung;  // Anzahl der Messungen pro Zyklus (Delay)
unsigned long ZeitPrint=0;  // letzter Zeitpunkt des Serial output

double getAmps(int SeNr)  
{
  // SeNr = Analoger Eingang
  int mVperAmp = 185; // use 100 for 20A Module and 66 for 30A Module
  int RawValue;
  long RS=0; // RawSumme 
  double Voltage = 0;
   double Amps;

   
  //RawValue = analogRead(analogIn[SeNr]);
  //delay(1);
//  int AnzMessung=4;
//  for(int N = 1; N<=AnzMessung; N+=1)   // x mal messen und schnitt nutzen
//  {
    RawValue = analogRead(SeNr);
    //delay(5);
//    RS=RS+RawValue;
//  }
//  RawValue=(RS/AnzMessung);
  Voltage = (RawValue / 1023.0) * 5000; // Gets you mV
   if ( SeNr == 2 )
  {
     SummeRawAmp=SummeRawAmp + Voltage; //Schnitt-Bildung
     Amps = ((Voltage - ACSoffset) / mVperAmp);
  }
  else if ( SeNr == 3 )
  {
    SummeRawAmp2=SummeRawAmp2 + Voltage; //Schnitt-Bildung
    Amps = ((Voltage - ACSoffset2) / mVperAmp);
  }
  if (MilliMode)
  {
     Amps=(Amps*1000);
  }
  //if ( Voltage < ACSoffset ) { ACSoffset=Voltage;}
  /*
  if ( Debug )
  {
    Serial.print(F(" amp=") );
    Serial.print(Amps);
    Serial.print(F(" V=") );
    Serial.print(Voltage);
    Serial.print(F(" raw=") );
    Serial.println(RawValue);
  }
  */
  return Amps;
}

void setup()  // Wird bei Systemstart 1x Aufgerufen
{
 // for (i=1; i<=AnzSensoren; i++)
 // {
 //   pinMode(batteryPin[i],INPUT);
 // }
  pinMode(batteryPin,INPUT);
  pinMode(2,INPUT);   // AmpMess
  pinMode(3,INPUT);   // AmpMess
  pinMode(RPinAn,OUTPUT);  //Bistab. Relai 1 An-Pin
  pinMode(RPinAus,OUTPUT); //Aus-Pin
  pinMode(R2Pin,OUTPUT);   //Monostab. Relai 1

  digitalWrite(RPinAn,LOW); //bistab 2 coil Relay
  digitalWrite(RPinAus,LOW);
  digitalWrite(R2Pin,HIGH);  //Monostab  low=AN


  Serial.begin(115200);
  Serial.print("Batterie Waechter ");
  Serial.println(Version);
  readEEPROM(); // load default values
   // reserve 200 bytes for the inputString:
  inputString.reserve(200);

}

void loop()
{ 
   if (stringComplete) {
      //Serial.print(inputString);     
      // prevent uboot bug: Ardu output activating failsafe on Cubieboard reboot
      if (inputString=="")
      {
        if (PengMode==1)
        {
          RelayMonoSchalten((!R2Status),2);
          
        }
        else if (PengMode==2)
        {
          RelayMonoSchalten(true,2);
          delay(20);
          RelayMonoSchalten(false,2);
        }
        
      }
      ss = inputString.substring(0,3);
      if (ss == "CPU")
      {
        delay(20000); // Warte 20 Sek. um uboot durchlaufen zu lassen ohne failsafe (mit serial print) zu triggern
        Serial.println(F("Beende Failsafe Wartepause"));
      }
      if (inputString == "h")
      {
        Serial.println(F("h                  help"));
        Serial.println(F("R1=0               switch OFF Relay 1"));
        Serial.println(F("R2=1               switch ON  Relay 2"));
        Serial.println(F("ACSoffset=2500     set zeroAmp-Volts"));
        Serial.println(F("ACSoffset2=2560    set zeroAmp-Volts #2"));
        Serial.println(F("VoltSchaltAn=11.3  set switch off volts"));
        Serial.println(F("VoltSchaltAus=9.5  set switch on volts"));
        Serial.println(F("Delay=1000         print out every ... ms"));
        Serial.println(F("DEBUG              switch - show raw values"));
        Serial.println(F("MILLIMODE          switch - show mV, mA values"));
        Serial.println(F("WaechterModus=1    switch akku guard mode"));
        Serial.println(F("  0 = do nothing on low voltage"));
        Serial.println(F("  1 = solar island: R1 OFF when V<=VoltSchaltAus"));
        Serial.println(F("  2 = 230V->12V charger: R2 ON when low voltage"));
        Serial.println(F("LOADVALUES         read values from rom"));
        Serial.println(F("SAVEVALUES         write values to rom"));
        Serial.println(F("PengMode=1         Enter sitches Relay2 modes:"));
        Serial.println(F("  0 = off (default)"));
        Serial.println(F("  1 = switch mode"));
        Serial.println(F("  2 = flash mode  (short on)"));
        
      }
      if (inputString == "i")
      {
        Serial.println(F("info"));
        Serial.println(F("Relai 1 = bistabil(D3 D4) fuer Rechner Notabschaltung"));
        Serial.println(F("Relai 2 = monostabil(D7) fuer Not-Ladung Akku von 230V"));
        Serial.println(F("Relai 3 = bistabil(D5 D6) reserviert fuer Verbraucher"));
        Serial.println(F("settings:"));
        printValues();
      }
      if (inputString == "DEBUG")
      {
        Debug=!Debug;
        Serial.print(F("DEBUG="));
        Serial.println(Debug);
      }
      if (inputString == "MILLIMODE")
      {
        MilliMode=!MilliMode;
        Serial.print(F("MILLIMODE="));
        Serial.println(MilliMode);
      }
      if (inputString == "SAVEVALUES")
      {
        // write to rom
        Serial.print(F("writing values to rom... "));
        writeEEPROM();
        Serial.println(F(" done"));
      }      
      if (inputString == "LOADVALUES")
      {
        // read rom
        Serial.print(F("loading values from rom... "));
        readEEPROM();
        Serial.println(F(" done"));
        printValues();
      }      

      ss = inputString.substring(0,10);
      if (ss == "ACSoffset=") 
      {
         sss=inputString.substring(10);
         ACSoffset = sss.toInt();
         Serial.print("ACSoffset=");
         Serial.println(ACSoffset);//delay(4);
      }
      ss = inputString.substring(0,11);
      if (ss == "ACSoffset2=") 
      {
         sss=inputString.substring(11);
         ACSoffset2 = sss.toInt();
         Serial.print("ACSoffset2=");
         Serial.println(ACSoffset2);//delay(4);
      }
      ss = inputString.substring(0,9);
      if (ss == "PengMode=") 
      {
         sss=inputString.substring(9);
         PengMode = sss.toInt();
         Serial.print("PengMode=");
         Serial.println(PengMode);//delay(4);
      }

      ss = inputString.substring(0,6);
      if (ss == "Delay=") 
      {
         sss=inputString.substring(6);
         Delay = sss.toInt();
         Serial.print("Delay=");
         Serial.println(Delay);//delay(4);
      }
      ss = inputString.substring(0,14);
      if (ss == "WaechterModus=") 
      {
         sss=inputString.substring(14);
         WaechterModusAEndern(sss.toInt());
         Serial.print("WaechterModus=");
         Serial.println(WaechterModus);//delay(4);
      }
      ss = inputString.substring(0,13);
      if (ss == "VoltSchaltAn=") 
      {
         sss=inputString.substring(13);
         VoltSchaltAn = strtod(sss.c_str(),NULL);
         Serial.print("Von=");
         Serial.println(VoltSchaltAn);//delay(4);
      }
      ss = inputString.substring(0,14);
      if (ss == "VoltSchaltAus=") 
      {
         Serial.print("Vvor=");
         Serial.println(VoltSchaltAus);//delay(4);
         sss=inputString.substring(14);
         Serial.print("sss=");
         Serial.println(sss);//delay(4);
         VoltSchaltAus = strtod(sss.c_str(),NULL);
            Serial.print("Vnach=");
         Serial.println(VoltSchaltAus);//delay(4);
         
      }
      ss = inputString.substring(0,3);
      if (ss == "R1=") 
      {
         sss=inputString.substring(3);
         int State = sss.toInt();
         Serial.print("Relai1=");
         Serial.println(State);//delay(4);
         RelaySchalten(State,1);
      }
     
      ss = inputString.substring(0,3);
      if (ss == "R2=") 
      {
         sss=inputString.substring(3);
         int State = sss.toInt();
         Serial.print("Relai2=");
         Serial.println(State);//delay(4);
         RelayMonoSchalten(State,2);
      }
      
      ss = inputString.substring(0,3);
      if (ss == "R3=") 
      {
         sss=inputString.substring(3);
         int State = sss.toInt();
         Serial.print("Relai3=");
         Serial.println(State);//delay(4);
         RelaySchalten(State,3);
      }

      inputString="";
      stringComplete = false;
   }
  // Messen
  AnzMessung=AnzMessung + 1; // fuer Schnitt-Bildung
  double Amp = getAmps(2);
  SummeAmp=SummeAmp + Amp; //Schnitt-Bildung
  double Amp2 = getAmps(3);
  SummeAmp2=SummeAmp2 + Amp2; //Schnitt-Bildung

  val = analogRead(batteryPin); // read the value from the sensor
  //double volts = (double(val) / resistorFactor) * referenceVolts ; // calculate the ratio
  double V2=double(val)/1024 * referenceVolts * rf2;
  if (MilliMode) V2=(V2*1000);
  SummeVolt=SummeVolt + V2; //Schnitt-Bildung

  if (millis()-ZeitPrint>=Delay) 
  {
    ZeitPrint=millis();
  //Anzeigen
    double AA=SummeAmp/AnzMessung;
    double AA2=SummeAmp2/AnzMessung;
    double VV=SummeVolt/AnzMessung;
    int Prec=2;  //Nachkommastellen printDouble
    String Milli=" ";
    if (MilliMode)
    {
       VV=(int)VV ; //round
       AA=(int)AA;
       AA2=(int)AA2;
       Prec=0;
       Milli=" m";
    }
    
    printDouble(VV,Prec);
    Serial.print(Milli);
    Serial.print("V  ");
    printDouble(AA,Prec);//delay(4);
    Serial.print(Milli);
    Serial.print(F("A  ") );
    printDouble(AA2,Prec);//delay(4);
    Serial.print(Milli);
    Serial.print(F("A   ") );
    if ( Debug )
    {
      double RRAA=SummeRawAmp/AnzMessung;
      Serial.print(F("raw="));     
      Serial.print(RRAA);
      Serial.print(F("    "));     
      double RRAA2=SummeRawAmp2/AnzMessung;
      Serial.print(F("raw2="));     
      Serial.print(RRAA2);
      Serial.print(F("    "));     

    }
    Serial.println(AnzMessung);

    if (MilliMode)
    {
      V2=V2/1000; //Rueckrechnen fuer Auswertung
    }

    if (WaechterModus==1) 
    {
       //AkkuSchonung Rechner Ausschalten
       if (V2<=VoltSchaltAus)
       {
         if (RelayStatus == true || ErstSchalt)
         { 
           //Ausschalten Relay
           Serial.print("BWaechter(");
           Serial.print(WaechterModus);
           Serial.println("):R1 0 V   Verbraucher aus");
           RelaySchalten(false,1); 
         }
       }
       if (V2>=VoltSchaltAn || ErstSchalt)
       {
          if (RelayStatus == false)
          { 
            //Einschalten Relay
            Serial.print("BWaechter(");
            Serial.print(WaechterModus);
            Serial.println("):R1 1 V   Verbraucher an");
            RelaySchalten(true,1); 
          }
       }
    } // end WaechterModus=1
    else if (WaechterModus==2) 
    {
      // AkkuSchonung Ladegeraet zusschalten
       if (V2<=VoltSchaltAus)
       {
          if (R2Status == false)
          { 
             //Einschalten Relay2
             Serial.print("BWaechter(");
             Serial.print(WaechterModus);
             Serial.println("):R2 1 V   Ladegeraet an");
             RelayMonoSchalten(true,2); 
          }
       }
       if (V2>=VoltSchaltAn || ErstSchalt)
       {
          if (R2Status == true)
          { 
           //AUSschalten Relay2 (Ladegeraet)
           Serial.print("BWaechter(");
           Serial.print(WaechterModus);
           Serial.println("):R2 0 V   Ladegeraet aus");
           RelayMonoSchalten(false,2); 
          }
       }
    }// end WaechterModus=2

    // reset values
    SummeVolt=0;
    SummeAmp=0;
    SummeRawAmp=0;
    SummeAmp2=0;
    SummeRawAmp2=0;
    AnzMessung=0;
    
  } // end if delay
  
  
//  delay(Delay);
}

void RelaySchalten(boolean Zustand, int ReNr) //bistabiles 2 Spulen Relai
{
  // schaltet das Bistabile 2-Spulen-Relay RT314F03 von Schrack
  // Vorsicht: 3V Spule, nur kurz schalten! 
  int PinAus;
  int PinAn;
  if ( ReNr==1 )
  {
      PinAn=RPinAn;
      PinAus=RPinAus;
      ErstSchalt=false;

    }
  else if ( ReNr==3 )
  {
      PinAn=R3PinAn;
      PinAus=R3PinAus;
      R3ErstSchalt=false;

  }

    if (Zustand)   
    {
      digitalWrite(PLed,HIGH); 
      digitalWrite(PinAn,HIGH); 
      delay(50);
      digitalWrite(PLed,LOW); 
      digitalWrite(PinAn,LOW);
    }
    else
    {
      digitalWrite(PLed,HIGH); 
      digitalWrite(PinAus,HIGH); 
      delay(50);
      digitalWrite(PinAus,LOW);
      digitalWrite(PLed,LOW); 

    }
    
    if ( ReNr==1 )
    {
       RelayStatus=Zustand;
    }
    else if ( ReNr==3 )
    {
       R3Status=Zustand;
    }

}
void RelayMonoSchalten(boolean Zustand, int ReNr) //bistabiles 2 Spulen Relai
{
  // schaltet monostabile 1-Spulen-Relay
   int PinNr=0;  
   if ( ReNr == 2 )
   {
       PinNr=R2Pin;
   }
   if (Zustand)   
   {
     digitalWrite(PinNr,LOW); 
   }
   else
   {
     digitalWrite(PinNr,HIGH); 
   }
   if ( ReNr == 2 )
   {
      R2Status=Zustand;
   }
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read(); 
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
    else
    {
    // add it to the inputString:
    inputString += inChar;
    } 
  }
}

void writeEEPROM() {
  int addr = 0;
  EEPROM.write(addr, 111);  //Signatur fuer Batterie-Waechter, dass schonmal gespeichert wurde
  addr=addr + 1;
  eepromWriteInt(addr, Delay); //int
  addr=addr + 2;
  eepromWriteInt(addr, ACSoffset); //int
  addr=addr + 2;
  EEPROM_writeDouble(addr, VoltSchaltAn); //double
  addr=addr + 4;
  EEPROM_writeDouble(addr, VoltSchaltAus); //double
  addr=addr + 4;
  EEPROM.write(addr, Debug); //bool
  addr=addr + 1;
  eepromWriteInt(addr, ACSoffset2); //int
  addr=addr + 2;
  EEPROM.write(addr, MilliMode); //bool
  addr=addr + 1;
  eepromWriteInt(addr, WaechterModus); //int
  addr=addr + 2;
}

// load values
void readEEPROM() {
  int addr = 0;
  int WW=EEPROM.read(addr); //bool
  
  if ( WW == 111 )
    {
      addr=addr + 1;
      Delay=eepromReadInt(addr); //int
      addr=addr + 2;
      ACSoffset=eepromReadInt(addr); //int
      addr=addr + 2;
      VoltSchaltAn=EEPROM_readDouble(addr); //double
      addr=addr + 4;
      VoltSchaltAus=EEPROM_readDouble(addr); //double
      addr=addr + 4;
      Debug=(boolean)EEPROM.read(addr); //bool
      addr=addr + 1;
      ACSoffset2=eepromReadInt(addr); //int
      addr=addr + 2;
      MilliMode=(boolean)EEPROM.read(addr); //bool
      addr=addr + 1;
      WaechterModusAEndern(eepromReadInt(addr)); //int
      addr=addr + 2;
    }
    else
    {
       Serial.print(F("no default values found on rom"));
    }
}


void printValues() {
  Serial.print(F("WaechterModus="));     
  Serial.println(WaechterModus);
  Serial.print(F("ACSoffset="));     
  Serial.println(ACSoffset);
  Serial.print(F("ACSoffset2="));     
  Serial.println(ACSoffset2);
  Serial.print(F("VoltSchaltAn="));     
  Serial.println(VoltSchaltAn);
  Serial.print(F("VoltSchaltAus="));     
  Serial.println(VoltSchaltAus);
  Serial.print(F("MilliMode="));     
  Serial.println(MilliMode);
  Serial.print(F("Delay="));     
  Serial.println(Delay);
  Serial.print(F("Debug="));     
  Serial.println(Debug);
  Serial.print(F("PM="));     
  Serial.println(PengMode);
  Serial.print(F("R1="));     
  Serial.println(RelayStatus);
  Serial.print(F("R2="));     
  Serial.println(R2Status);
  Serial.print(F("R3="));     
  Serial.println(R3Status);

}

void eepromWriteInt(int adr, int wert) {
// 2 Byte Integer Zahl im EEPROM ablegen an der Adresse
// Eingabe: 
//   adr: Adresse +0 und +1 wird geschrieben
//   wert: möglicher Wertebereich -32,768 bis 32,767
// Ausgabe: 
//   -
// 2 Byte Platz werden belegt.
//
// Matthias Busse 5.2014 V 1.0
 
byte low, high;
 
  low=wert&0xFF;
  high=(wert>>8)&0xFF;
  EEPROM.write(adr, low); // dauert 3,3ms 
  EEPROM.write(adr+1, high);
  return;
} //eepromWriteInt
 
int eepromReadInt(int adr) {
// 2 Byte Integer Zahl aus dem EEPROM lesen an der Adresse
// Eingabe: 
//   adr: Adresse +0 und +1 wird gelesen
// Ausgabe: int Wert
//
// Matthias Busse 5.2014 V 1.0
 
byte low, high;
 
  low=EEPROM.read(adr);
  high=EEPROM.read(adr+1);
  return low + ((high << 8)&0xFF00);
} //eepromReadInt

double EEPROM_readDouble(int ee)
{
   double value = 0.0;
   byte* p = (byte*)(void*)&value;
   for (int i = 0; i < sizeof(value); i++)
       *p++ = EEPROM.read(ee++);
   return value;
}
void EEPROM_writeDouble(int ee, double value)
{
   byte* p = (byte*)(void*)&value;
   for (int i = 0; i < sizeof(value); i++)
       EEPROM.write(ee++, *p++);
}

void printDouble( double val, byte precision){
  // prints val with number of decimal places determine by precision
  // precision is a number from 0 to 6 indicating the desired decimial places
  // example: lcdPrintDouble( 3.1415, 2); // prints 3.14 (two decimal places)
 
  if(val < 0.0){
    Serial.print('-');
    val = -val;
  }

  Serial.print (int(val));  //prints the int part
  if( precision > 0) {
    Serial.print("."); // print the decimal point
    unsigned long frac;
    unsigned long mult = 1;
    byte padding = precision -1;
    while(precision--)
  mult *=10;
 
    if(val >= 0)
 frac = (val - int(val)) * mult;
    else
 frac = (int(val)- val ) * mult;
    unsigned long frac1 = frac;
    while( frac1 /= 10 )
 padding--;
    while(  padding--)
 Serial.print("0");
    Serial.print(frac,DEC) ;
  }
}
void WaechterModusAEndern(int Modus)
{
  //change init Settings
  if (Modus==1)
  {
     RelayMonoSchalten(false,2); // Ladegeraet aus
  }
  else  if (Modus==2)
  {
    RelaySchalten(true,1); // Rechner An
  }
  else
  {
    RelayMonoSchalten(false,2); // Ladegeraet aus
    RelaySchalten(true,1); // Rechner An
  }
  WaechterModus=Modus;
}
