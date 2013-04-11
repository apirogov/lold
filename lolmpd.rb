#!/usr/bin/env ruby
#lolmpd.rb - simple lold MPD track notification
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License
require "#{File.dirname(__FILE__)}/lollib"

def get_frames(str)
  l = LolFactory.new
  l.banner str
  return l.frames
end

oldtrack = ""
duration = 0
sleepcount = 0
while true
  track = `mpc -h mediacenter | grep -v volume | head -n 1`
  track = track.gsub(/^.*\//,'').gsub(/\.mp3$/,'')
  track = track.gsub(/\P{ASCII}/, '')
  if oldtrack==track && sleepcount < duration
    sleep 5
    sleepcount += 5
  else
    oldtrack = track
    sleepcount = 0

    #output track
    frames = get_frames(track)
    duration = (frames.length*LolTask::DEF_DELAY).to_f/1000+10
    LolHelper.send :frames=>get_frames(" "), :ch=>1
    sleep 2
    LolHelper.send :frames=>frames
  end
end
