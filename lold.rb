#!/usr/bin/env ruby
#LOLd Server - Lolshield Daemon
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License

require "#{File.dirname(__FILE__)}/lollib"

DEBUG=false

#read possible parameters for daemon
auto = ARGV.index('-a') ? true : false     #automatic device detection
device = LolHelper.eval_arg('-d',nil)      #lolshield device, nil=stdout
device = LolHelper.autodetect if auto
port = LolHelper.eval_arg('-p',LoldServer::DEF_PORT) #port for lold to listen
delay = LolHelper.eval_arg('-D',LolTask::DEF_DELAY)  #standard delay between frames

currtask = nil                             #current task being executed

#check device path
if device!=nil
  if !File.exists?(device)
    puts "Device #{device} does not exist!"
    exit 1
  elsif !File.writable?(device)
    puts "Device #{device} has wrong permissions!"
    exit 1
  end
end

#start up server thread accepting clients
$server = LoldServer.new(port,nil)
$server.device = device
$server.start
trap(:INT) { $server.shutting_down = true }

#eternally output to device until stopped
loop do
  #check if shutdown requested
  if $server.shutting_down
    LolHelper.render_frame device, LolHelper::CLEAR_FRAME
    $server.shutdown
    LolHelper.close_ports
    exit
  end

  #do nothing if the streaming process is running
  if $server.streaming
    LolHelper.sleep_ms 100
    next
  end

  if $server.interrupted #new task arrived - check stuff
    $server.interrupted = false

    if !currtask.nil? && $server.queue.first.channel>currtask.channel
      currtask.cancel
      puts "Ani cancelled" if DEBUG
      currtask=nil #forget current task
    end
  end

  #load a new task if neccessary
  if currtask.nil?
    currtask = $server.queue.first
    puts "Ani start" if !currtask.nil? if DEBUG
  end

  if currtask.nil? #nothing in queue
    LolHelper.sleep_ms delay
    next
  else  #animation custom delay
    del = currtask.delay==LolTask::DEF_DELAY ? delay : currtask.delay
    LolHelper.sleep_ms del
  end

  frame = currtask.pop_frame #get next frame

  if frame.nil? #current animation done?
    currtask = nil
    $server.clean_tasks #remove aged and finished tasks
    LolHelper.render_frame device, LolHelper::CLEAR_FRAME
    puts "Ani done" if DEBUG
    next
  end

  #render next frame
  success = LolHelper.render_frame device, frame

  if !success && auto #recover after suspension and device name change if started with -a
    device = LolHelper.autodetect
  end
end
