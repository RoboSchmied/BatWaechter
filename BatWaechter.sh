# schreiben nicht möglich
#stty -F /dev/ttyS0 115200 raw
#sleep 1
# schreiben OK

#Schw=20  # SchwellWert in mV / mA  MindestAEnderung eines Wertes fuer neuen LogEintrag
Schw=50  # SchwellWert in mV / mA  MindestAEnderung eines Wertes fuer neuen LogEintrag
V1alt=0
A1alt=0
A2alt=0

V1log=0  # gelogte Werte
A1log=0
A2log=0

cd /home/cubie

LogTmp="/tmp/BatWaechter.output"
Pfad=$(dirname "$(readlink -e "$0")")
Log="$Pfad/BatWaechter.log"



Anz=$(ps ax | grep "BatWaechter.sh" | grep -v grep | wc -l)
if [ $Anz -gt 2 ]; then
  echo "$Anz laeuft schon, ende"
  exit 1
fi

echo "starte $0" >> "$LogTmp"
stty -F /dev/ttyS0 cs8 115200 ignbrk -brkint -icrnl -imaxbel -opost -onlcr -isig -icanon -iexten -echo -echoe -echok -echoctl -echoke noflsh -ixon -crtscts

Anz=$(ps ax | grep "cat /dev/ttyS0" | grep -v grep | wc -l)
echo "$Anz"
if [ $Anz -lt 1 ]; then
  cat /dev/ttyS0 >> "$LogTmp" &
  PID=$!
  echo "PID=$PID"
  # kill background process on SIGTERM
  trap 'kill $PID; echo ciao; Datum=$(date +%Y-%m-%d-%H:%M.%S); echo "# $Datum Beende BatWaechter"; exit' SIGINT SIGTERM
else
  trap 'echo ciao; Datum=$(date +%Y-%m-%d-%H:%M.%S); echo "# $Datum Beende BatWaechter" | tee -a "$Log"; exit' SIGINT SIGTERM
  echo "Verbindung mit /dev/ttyS0 besteht schon"
fi


tail -f $LogTmp | while read Zeile
do
  if [ -n "$1" ]; then
    echo "$Zeile" #$V1 $A1 $A2   $V1d $A1d $A2d"
  fi 
  V1=$(echo "$Zeile" | awk '{print $1}')   # Volts
  E1=$(echo "$Zeile" | awk '{print $2}')   # Einheit
  A1=$(echo "$Zeile" | awk '{print $3}')   # Ampere 1
  A2=$(echo "$Zeile" | awk '{print $5}')

  if [ "$V1" == "" ] || [ "$A1" == "" ] || [ "$A2" == "" ] || [ "$E1" != "mV" ]; then
    echo "# $Zeile" | tee -a $Log
  else
  
    V1d=$(($V1 - $V1alt))
    A1d=$(($A1 - $A1alt))
    A2d=$(($A2 - $A2alt))
    # Vorzeichen entfernen V1d -> V1D
    if [ $V1d -lt 0 ]; then V1D=$(($V1d * -1)); else V1D=$V1d ; fi
    if [ $A1d -lt 0 ]; then A1D=$(($A1d * -1)); else A1D=$A1d ; fi
    if [ $A2d -lt 0 ]; then A2D=$(($A2d * -1)); else A2D=$A2d ; fi

    if [ $V1D -gt $Schw ] || [ $A1D -gt $Schw ] || [ $A2D -gt $Schw ]; then 
      #Datum=$(date +%Y-%m-%d-%H:%M.%S+%s)
      Datum=$(date +%Y-%m-%d-%H:%M.%S)
      echo "$Datum $V1 $A1 $A2   $V1d $A1d $A2d" | tee -a "$Log"
      V1alt=$V1
      A1alt=$A1
      A2alt=$A2
    fi
  fi
done # read log
