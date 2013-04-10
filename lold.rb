#!/usr/bin/env ruby
#LOLd Server - Lolshield Daemon
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License

require 'gserver'
DEBUG=false

#instance of an animation task
class LolTask
  attr_accessor :delay, :ttl, :channel
  attr_reader :timestamp

  DEF_DELAY = 50    #standard. (valid: 50-1000, NOTE: below 50 may glitch often)
  DEF_TTL = 60      #60 sec (valid: 0-600)
  DEF_CH = 0        #valid: 0+
  #Channel work this way: higher channel=higher priority
  #same channel = in order of arrival
  #new message on higher channel = discard current message

  def initialize
    @delay    = DEF_DELAY
    @ttl      = DEF_TTL
    @channel  = DEF_CH

    @frames   = []  #empty frame
    @index    = 0   #current position
    @timestamp = Time.now.to_i #creation timestamp
  end

  #enqueue a frame
  def push_frame(f)
    @frames.push f
  end

  def pop_frame
    f = @frames[@index]
    @index += 1
    cancel if @index>=@frames.length
    return f
  end

  #empty current message -> dropped
  def cancel
    @frames = []
  end

  def done?
    return @frames.empty?
  end
end

#server accepting clients
class LoldServer < GServer
  #defined tokens for protocol
  SYM_TSK = "TASK"

  SYM_TTL = "TTL"
  SYM_CHL = "CH"
  SYM_DEL = "DELAY"

  SYM_DAT = "DATA"
  SYM_END = "END"

  SYM_OK  = "OK"
  SYM_ERR = "ERR"

  @valid_syms = [SYM_TSK,SYM_TTL,SYM_CHL,SYM_DEL,SYM_DAT,SYM_END]

  def serve(io)
    err = Proc.new{ io.puts SYM_ERR; return }

    #read head
    line = io.gets.chomp
    err.call if line != SYM_TSK

    task = LolTask.new

    #read parameters
    while true
      line = io.gets.chomp.split(/\s+/)
      p = line[0]
      v = line[1].to_i

      break if p==SYM_DAT # -> data section

      #read parameter and throw error if out of range
      case p
      when SYM_TTL
        err.call if v<0 || v>600
        task.ttl=v
      when SYM_CHL
        err.call if v<0
        task.channel=v
      when SYM_DEL
        err.call if v<50 || v>1000
        task.delay=v
      else #invalid parameter?
        err.call
      end
    end

    #read frames
    while true
      line = io.gets.chomp
      break if line==SYM_END
      err.call if line.split(',').length != 9 #invalid frame
      task.push_frame line
    end

    insert_task task
    $interrupted = true #notify other thread

    io.puts SYM_OK
    puts "Ani added on channel #{task.channel}" if DEBUG
  end
end

#START PROGRAM FLOW HERE
DEF_PORT = 10101
CLEAR="0,0,0,0,0,0,0,0,0"

$port = DEF_PORT          #port for lold to listen
$delay = LolTask::DEF_DELAY #delay between frames, if not overridden
$device = nil             #lolshield device, nil=stdout

$shutdown_server = false  #when the process is killed
$interrupted = false      #when a new task is incoming

$queue = []               #queue of animation tasks
$currtask = nil           #current task being executed

#simple argument evaluation
def eval_arg(arg,default)
  i = ARGV.index(arg)
  return default if i.nil?
  ret = ARGV[i+1]
  2.times{ ARGV.delete_at(i) }
  return ret
end

#instead of sending a frame to the shield
#output to console (for testing / no shield given)
def dummy_output(frame)
  lines = frame.split(',').map(&:to_i)
  lines.each do |l|
    0.upto(13) do |i|
      if (l & (1<<i))!=0
        print "\e[31;1mO\e[0m" #ON (ANSI red)
      else
        print "O" #OFF
      end
    end
    puts
  end
  puts "\e[A"*10 #ANSI cursor up
end


#push a frame to the shield
def render_frame(device, frame)
  if device.nil?
    dummy_output frame
  else
    `echo "#{frame}" > #{$device}`
  end
end

def sleep_ms(ms)
  sleep ms.to_f/1000
end

#insert task into queue on correct position
def insert_task(task)
  0.upto($queue.length) do |i|
    t = $queue[i]
    if t.nil? || (t.channel<=task.channel && t.timestamp<=task.timestamp)
      $queue.insert i, task
      return
    end
  end
end

#remove expired tasks
def clean_tasks
  time = Time.now.to_i
  $queue.map! do |t|
    if t.done?
      puts "Task cleaned (done)" if DEBUG
      nil
    elsif t.ttl>0 && time > t.timestamp+t.ttl
      puts "Task cleaned (age)" if DEBUG
      nil
    else
      t
    end
  end
  $queue.delete nil
end

#read possible parameters for daemon
$device = eval_arg('-d',nil)
$port = eval_arg('-p',DEF_PORT)
$delay = eval_arg('-D',LolTask::DEF_DELAY)

#start up server thread accepting clients
server = LoldServer.new($port)
server.start
trap(:INT) { $shutdown_server = true }

#eternally output to device until stopped
loop do
  #check if shutdown requested
  if $shutdown_server
    render_frame $device, CLEAR
    server.shutdown
    exit
  end

  if $interrupted #new task arrived - check stuff
    $interrupted = false

    if !$currtask.nil? && $queue.first.channel>$currtask.channel
      $currtask.cancel
      puts "Ani cancelled" if DEBUG
      $currtask=nil #forget current task
    end
  end

  #load a new task if neccessary
  if $currtask.nil?
    $currtask = $queue.first
    puts "Ani start" if !$currtask.nil? if DEBUG
  end

  if $currtask.nil? #nothing in queue
    sleep_ms $delay
    next
  else  #animation custom delay
    sleep_ms $currtask.delay
  end

  frame = $currtask.pop_frame #get next frame

  if frame.nil? #current animation done?
    $currtask = nil
    clean_tasks #remove aged and finished tasks
    render_frame $device, CLEAR
    puts "Ani done" if DEBUG
    next
  end

  #render next frame
  render_frame $device, frame
end
