#/bin/bash
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License
#
#Demonstration of lold streaming interface in pure bash
#Move the dot around the shield with hjkl, quit with q
#Change host and port of lold if neccessary.

exec 3<>/dev/tcp/localhost/10101
head -n 1 <&3 >/dev/null
echo -en "STREAM\n" >&3

echo "Press hjkl to move dot, q to quit"
stty -echo #hide input

posx=0
posy=0
while [[ "$REPLY" != "q" ]]; do
  read -r -n 1 -t 0.1
  if [[ "$?" -eq "0" ]]; then
    if [[ "$REPLY" == "h" ]]; then
      posx=$(($posx-1))
    elif [[ "$REPLY" == "l" ]]; then
      posx=$(($posx+1))
    elif [[ "$REPLY" == "k" ]]; then
      posy=$(($posy-1))
    elif [[ "$REPLY" == "j" ]]; then
      posy=$(($posy+1))
    fi
    #Wrap around
    [ "$posx" -lt 0 ] && posx=13
    [ "$posy" -lt 0 ] && posy=8
    [ "$posx" -ge 14 ] && posx=0
    [ "$posy" -ge 9 ] && posy=0
  fi

  #generate ascii frame
  string=""
  for y in {0..8}; do
    for x in {0..13}; do
      if [ "$posx" -eq "$x" ] && [ "$posy" -eq "$y" ]; then
        string+="x"
      else
        string+="."
      fi
    done
    string+="\n"
  done
  #pipe to lolplay to generate a frame in correct format and send to lold over open socket
  echo -e $string | ./lolplay -A -o >&3
  sleep 0.1
done

#close connection
echo -e "END" >&3
head -n 1 <&3 >/dev/null

stty echo #show input feedback again
echo #new line


