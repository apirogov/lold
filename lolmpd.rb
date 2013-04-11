#!/usr/bin/env ruby
#lolmpd.rb - simple lold MPD track notification
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License
#requires mpc, grep and awk to be installed to work

require "#{File.dirname(__FILE__)}/lollib"

host = LolHelper.eval_arg('-h','localhost')
port = LolHelper.eval_arg('-p',6600).to_i
MPC = "mpc -h #{host} -p #{port} "

showp = ARGV.index('--show-progress')
showv = ARGV.index('--show-volume')

def get_frames(str)
  l = LolFactory.new
  l.banner str
  return l.frames
end

def perc_to_row(percent)
  i = (14.to_f/100*percent).to_i
  i=13 if i==14
  (0..i).map{|x| 2**x}.inject(:+)
end

def mod_frames(frames, volume, progress)
  frames.map do |line|
    l = line.split(',')
    l[-2]=perc_to_row(volume).to_s if volume
    l[-1]=perc_to_row(progress).to_s if progress
    l.join(',')
  end
end

oldtrack = ""
duration = 0
sleepcount = 0
vol = nil
prg = nil
while true
  track = `#{MPC} | grep -v volume | head -n 1`
  track = track.gsub(/^.*\//,'').gsub(/\.mp3$/,'')
  track = track.gsub(/\P{ASCII}/, '')

  if oldtrack==track && sleepcount < duration
    sleep 5
    sleepcount += 5
  else
    oldtrack = track
    sleepcount = 0

    #optional
    vol = `#{MPC} | grep volume | awk "{print$2}"`.match(/\d+/).to_s.to_i if showv
    prg = `#{MPC} | grep playing | awk '{print$4}'`.match(/\d+/).to_s.to_i if showp

    #output track
    frames = mod_frames get_frames(track), vol, prg
    duration = (frames.length*LolTask::DEF_DELAY).to_f/1000+10
    LolHelper.send :frames=>get_frames(" "), :ch=>1
    sleep 2
    LolHelper.send :frames=>frames
  end
end
