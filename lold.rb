#!/usr/bin/env ruby
#LOLd Server - Lolshield Daemon
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License

require "#{File.dirname(__FILE__)}/lollib"

DEBUG=false

#simple argument evaluation
def eval_arg(arg,default)
  i = ARGV.index(arg)
  return default if i.nil?
  ret = ARGV[i+1]
  2.times{ ARGV.delete_at(i) }
  return ret
end

#automatic lolshield detection (serial USB device search)
def autodetect
  devs=Dir.new('/dev').entries
  devs = devs.select{|s| s.match /tty(USB|ACM).*/}
  dev = devs.first
  return '/dev/'+dev if dev
  return nil
end

#read possible parameters for daemon
auto = ARGV.index('-a') ? true : false     #automatic device detection
device = eval_arg('-d',nil)                #lolshield device, nil=stdout
device = autodetect if auto
port = eval_arg('-p',LoldServer::DEF_PORT) #port for lold to listen
delay = eval_arg('-D',LolTask::DEF_DELAY)  #standard delay between frames

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
$server = LoldServer.new(port)
$server.device = device
$server.start
trap(:INT) { $server.shutting_down = true }

#eternally output to device until stopped
loop do
  #check if shutdown requested
  if $server.shutting_down
    LolHelper.render_frame device, LolHelper::CLEAR_FRAME
    $server.shutdown
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
    LolHelper.sleep_ms currtask.delay
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
  LolHelper.render_frame device, frame
end
