#!/usr/bin/env ruby
#lolmpd.rb - simple lold MPD track notification
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License

oldtrack = ""
sleepcount = 0
while true
  track = `mpc -h mediacenter | grep -v volume | head -n 1`
  track = track.gsub(/^.*\//,'').gsub(/\.mp3$/,'')
  track = track.gsub(/\P{ASCII}/, '')
  if oldtrack==track && sleepcount < 6
    sleep 5
    sleepcount += 1
  else
    oldtrack = track
    sleepcount = 0
    #puts track
    `./lolplay.rb -m -C 1 " "`
    sleep 2
    `./lolplay.rb -m "#{track}"`
  end
end
