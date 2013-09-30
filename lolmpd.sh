#!/bin/bash
#lolmpd.sh - simple lold MPD track notification
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License
#
#requires mpc, grep and awk to be installed to work

#help text
usage() {
cat << EOF
usage: $0 [OPTIONS]
This script regularily sends the running MPD track to lold

OPTIONS:
  -h:  Show this message
  -l:  lold host
  -p:  lold port
  -m:  MPD host
  -P:  MPD port
EOF
}

#read settings
LOLD_HOST=localhost
LOLD_PORT=10101
MPD_HOST=localhost
MPD_PORT=6600
while getopts "hl:p:m:P:" OPTION; do
  case $OPTION in
    h)
      usage
      exit 1
      ;;
    l)
      LOLD_HOST=$OPTARG
      ;;
    p)
      LOLD_PORT=$OPTARG
      ;;
    m)
      MPD_HOST=$OPTARG
      ;;
    P)
      MPD_PORT=$OPTARG
      ;;
  esac
done

#main loop
MPC="mpc -h $MPD_HOST -p $MPD_PORT"
OLDTRACK=
DURATION=0
SLEEPCOUNT=0
while true; do
  TRACK=`$MPC | grep -v volume | head -n 1`       #read track
  TRACK=`echo "$TRACK" | sed 's/[^[:print:]]//'`  #strip non-printable
  TRACK=`echo "$TRACK" | sed 's/^.*\///'`         #strip path
  TRACK=`echo "$TRACK" | sed 's/.mp3$//'`         #strip ext. on filenames

  if [ "$TRACK" == "$OLDTRACK" ] && [ "$SLEEPCOUNT" -lt "$DURATION" ]; then
    #old animation probably still playing -> sleep
    sleep 5
    SLEEPCOUNT=$(($SLEEPCOUNT+5))
  else
    #time to resend track / show new track
    OLDTRACK=$TRACK
    SLEEPCOUNT=0
    
    #calculate approximate duration (default delay of 50ms)
    frmnum=`lolplay -m $TRACK -o | wc -l`
    DURATION=$(($frmnum*50/1000+10))

    #output track
    lolplay -h $LOLD_HOST -p $LOLD_PORT -C 1 -m " " #interrupt running
    sleep 2
    lolplay -h $LOLD_HOST -p $LOLD_PORT -D 50 -m "$TRACK" #current track
  fi
done

