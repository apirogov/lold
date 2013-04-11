#Lolshield Daemon library
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License

require 'socket'
require 'gserver'

#Class with commonly used methods
class LolHelper

  CLEAR_FRAME="0,0,0,0,0,0,0,0,0"

  #instead of sending a frame to the shield
  #output to console (for testing / no shield given)
  def self.dummy_output(frame)
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

  def self.render_frame(device,frame)
    if device.nil?
      dummy_output frame
    else
      File.write(device,frame+"\n")
    end
  end

  def self.sleep_ms(ms)
    sleep ms.to_f/1000
  end

  #send an animation task to a server. returns true on success
  def self.send(params={})
    frames = params[:frames]

    host = params[:host]
    port = params[:port]
    delay = params[:delay]
    ttl = params[:ttl]
    ch = params[:ch]

    host = "localhost" if !host
    port = LoldServer::DEF_PORT if !port

    s = TCPSocket.open(host,port)
    if s.gets.chomp==LoldServer::SYM_BSY
      s.close
      return false
    end

    s.puts LoldServer::SYM_TSK
    s.puts LoldServer::SYM_DEL+" #{delay}" if delay
    s.puts LoldServer::SYM_TTL+" #{ttl}" if ttl
    s.puts LoldServer::SYM_CHL+" #{ch}" if ch

    s.puts LoldServer::SYM_DAT
    frames.each do |f|
      s.puts f
    end

    s.puts LoldServer::SYM_END
    if s.gets.chomp==LoldServer::SYM_ERR
      s.close
      return false
    end

    s.close
    return true
  rescue
    return false
  end

  #simple argument evaluation
  def self.eval_arg(arg,default)
    i = ARGV.index(arg)
    return default if i.nil?
    ret = ARGV[i+1]
    2.times{ ARGV.delete_at(i) }
    return ret
  end
end

#Class to simplify interactive applications on the LolShield
class LolApp
  attr_reader :host,:port,:delay,:running

  #Params: String host, int port, int delay (50-1000)
  def initialize(host, port=LoldServer::DEF_PORT, delay=LolTask::DEF_DELAY)
    @host = host
    @port = port
    @delay = delay

    @socket = TCPSocket.open(host,port)
    ret = @socket.gets.chomp
    raise "Busy" if @socket.gets.chomp==LoldServer::SYM_BSY

    @socket.puts LoldServer::SYM_STM
  end

  #returns whether the app is alive
  def running?
    @running
  end

  #stops app
  def stop
    @socket.puts LoldServer::SYM_END
    @running = false
  end
end

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

  #get next frame
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
  DEF_PORT = 10101 #default lold port

  #defined tokens for protocol
  SYM_TSK = "TASK"
  SYM_STM = "STREAM"

  SYM_TTL = "TTL"
  SYM_CHL = "CH"
  SYM_DEL = "DELAY"

  SYM_DAT = "DATA"
  SYM_END = "END"

  SYM_OK  = "OK"
  SYM_BSY  = "BUSY"
  SYM_ERR = "ERR"

  @valid_syms = [SYM_TSK,SYM_TTL,SYM_CHL,SYM_DEL,SYM_DAT,SYM_END]

  attr_accessor :device,:streaming,:interrupted,:shutting_down,:queue

  def initialize(*args)
    super(*args)

    @device = nil

    @streaming = false
    @interrupted = false
    @shutting_down = false
    @queue = []
  end

  def serve(io)
    err = Proc.new{ io.puts SYM_ERR; return }

    if @streaming #locked state? -> tell
      io.puts SYM_BSY
      return
    else
      io.puts SYM_OK
    end

    #read head
    line = io.gets.chomp
    err.call if line != SYM_TSK && line != SYM_STM

    #read content
    if line==SYM_TSK
      read_task(io)
    else
      read_stream(io)
    end
  end

  #read an async animation task
  def read_task(io)
    err = Proc.new{ io.puts SYM_ERR; return }

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
    @interrupted = true #notify other thread

    puts "Ani added on channel #{task.channel}" if DEBUG
    io.puts SYM_OK  #Response
  end

  #read a realtime stream of frames
  def read_stream(io)
    err = Proc.new{ @streaming=false; io.puts SYM_ERR; return }

    puts "Stream started" if DEBUG
    @streaming = true #lock

    #read frames
    while true
      line = io.gets.chomp
      break if line==SYM_END
      err.call if line.split(',').length != 9 #invalid frame
      LolHelper.render_frame @device, line
    end

    @streaming = false  #unlock
    LolHelper.render_frame $device, LolHelper::CLEAR_FRAME

    puts "Stream finished" if DEBUG
    io.puts SYM_OK  #Response
  end

  ####

  #insert task into queue on correct position
  def insert_task(task)
    0.upto(@queue.length) do |i|
      t = @queue[i]
      if t.nil? || (t.channel<=task.channel && t.timestamp<=task.timestamp)
        @queue.insert i, task
        return
      end
    end
  end

  #remove expired tasks
  def clean_tasks
    time = Time.now.to_i
    @queue.map! do |t|
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
    @queue.delete nil
  end
end

#usage: create new object, modify @frame as you wish directly or set/get/toggle
#.banner renders a frameset with scrolling text automatically
#.draw uses current frame
#use .flip to "render" virtually (finish up frame), .clear to blank
#get frameset with .to_s (with linebreaks) or access @frames
class LolFactory
  @@ascii = [
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x0 => Blank
		[0x33,0x00,0x00,0x00,0x00], # ASCII-Code 0x1 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x2 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x3 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x4 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x5 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x6 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x7 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x8 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x9 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0xA => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0xB => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0xC => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0xD => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0xE => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0xF => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x10 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x11 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x12 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x13 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x14 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x15 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x16 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x17 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x18 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x19 => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x1A => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x1B => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x1C => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x1D => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x1E => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x1F => Blank
		[0x00,0x00,0x00,0x00,0x00], # ASCII-Code 0x20 => Space
		[0x00,0x00,0x4F,0x00,0x00], # ASCII-Code 0x21 => !
		[0x00,0x07,0x00,0x07,0x00], # ASCII-Code 0x22 => "
		[0x14,0x7F,0x14,0x7F,0x14], # ASCII-Code 0x23 => #
		[0x24,0x2A,0x7F,0x2A,0x12], # ASCII-Code 0x24 => $
		[0x23,0x13,0x08,0x64,0x62], # ASCII-Code 0x25 => %
		[0x36,0x49,0x55,0x52,0x50], # ASCII-Code 0x26 => &
		[0x00,0x05,0x03,0x00,0x00], # ASCII-Code 0x27 => ´
		[0x00,0x1C,0x22,0x81,0x00], # ASCII-Code 0x28 => {
		[0x00,0x81,0x22,0x1C,0x00], # ASCII-Code 0x29 => }
		[0x14,0x08,0x3E,0x08,0x14], # ASCII-Code 0x2A => *
		[0x08,0x08,0x3E,0x08,0x08], # ASCII-Code 0x2B => +
		[0x00,0x50,0x30,0x00,0x00], # ASCII-Code 0x2C => ,
		[0x08,0x08,0x08,0x08,0x08], # ASCII-Code 0x2D => -
		[0x00,0x60,0x60,0x00,0x00], # ASCII-Code 0x2E => .
		[0x20,0x10,0x08,0x04,0x02], # ASCII-Code 0x2F => /
		[0x3E,0x51,0x49,0x45,0x3E], # ASCII-Code 0x30 =>  0
		[0x00,0x42,0x7F,0x40,0x00], # ASCII-Code 0x31 =>  1
		[0x42,0x61,0x51,0x49,0x46], # ASCII-Code 0x32 =>  2
		[0x21,0x41,0x45,0x4B,0x31], # ASCII-Code 0x33 =>  3
		[0x18,0x14,0x12,0x7F,0x10], # ASCII-Code 0x34 =>  4
		[0x27,0x45,0x45,0x45,0x39], # ASCII-Code 0x35 =>  5
		[0x3C,0x4A,0x49,0x49,0x30], # ASCII-Code 0x36 =>  6
		[0x01,0x71,0x09,0x05,0x03], # ASCII-Code 0x37 =>  7
		[0x36,0x49,0x49,0x49,0x36], # ASCII-Code 0x38 =>  8
		[0x06,0x49,0x49,0x29,0x1E], # ASCII-Code 0x39 =>  9
		[0x00,0x36,0x36,0x00,0x00], # ASCII-Code 0x3A => :
		[0x00,0x56,0x36,0x00,0x00], # ASCII-Code 0x3B => ;
		[0x08,0x14,0x22,0x41,0x00], # ASCII-Code 0x3C => <
		[0x14,0x14,0x14,0x14,0x14], # ASCII-Code 0x3D => =
		[0x41,0x22,0x14,0x08,0x00], # ASCII-Code 0x3E => >
		[0x02,0x01,0x51,0x09,0x06], # ASCII-Code 0x3F => ?
		[0x32,0x49,0x79,0x41,0x3E], # ASCII-Code 0x40 => @
		[0x7E,0x11,0x11,0x11,0x7E], # ASCII-Code 0x41 => A
		[0x7F,0x49,0x49,0x49,0x36], # ASCII-Code 0x42 => B
		[0x3E,0x41,0x41,0x41,0x22], # ASCII-Code 0x43 => C
		[0x7F,0x41,0x41,0x22,0x1C], # ASCII-Code 0x44 => D
		[0x7F,0x49,0x49,0x49,0x41], # ASCII-Code 0x45 => E
		[0x7F,0x09,0x09,0x09,0x01], # ASCII-Code 0x46 => F
		[0x3E,0x41,0x49,0x49,0x7A], # ASCII-Code 0x47 => G
		[0x7F,0x08,0x08,0x08,0x7F], # ASCII-Code 0x48 => H
		[0x00,0x41,0x7F,0x41,0x00], # ASCII-Code 0x49 => I
		[0x20,0x40,0x41,0x3F,0x01], # ASCII-Code 0x4A => J
		[0x7F,0x08,0x14,0x22,0x41], # ASCII-Code 0x4B => K
		[0x7F,0x40,0x40,0x40,0x40], # ASCII-Code 0x4C => L
		[0x7F,0x02,0x0C,0x02,0x7F], # ASCII-Code 0x4D => M
		[0x7F,0x04,0x08,0x10,0x7F], # ASCII-Code 0x4E => N
		[0x3E,0x41,0x41,0x41,0x3E], # ASCII-Code 0x4F => O
		[0x7F,0x09,0x09,0x09,0x06], # ASCII-Code 0x50 => P
		[0x3E,0x41,0x51,0x21,0x5E], # ASCII-Code 0x51 => Q
		[0x7F,0x09,0x19,0x29,0x46], # ASCII-Code 0x52 => R
		[0x46,0x49,0x49,0x49,0x31], # ASCII-Code 0x53 => S
		[0x01,0x01,0x7F,0x01,0x01], # ASCII-Code 0x54 => T
		[0x3F,0x40,0x40,0x40,0x3F], # ASCII-Code 0x55 => U
		[0x1F,0x20,0x40,0x20,0x1F], # ASCII-Code 0x56 => V
		[0x3F,0x40,0x38,0x40,0x3F], # ASCII-Code 0x57 => W
		[0x63,0x14,0x08,0x14,0x63], # ASCII-Code 0x58 => X
		[0x07,0x08,0x70,0x08,0x07], # ASCII-Code 0x59 => Y
		[0x61,0x51,0x49,0x45,0x43], # ASCII-Code 0x5A => Z
		[0x00,0x7F,0x41,0x41,0x00], # ASCII-Code 0x5B => [
		[0x15,0x16,0x7C,0x16,0x15], # ASCII-Code 0x5C => \
		[0x00,0x41,0x41,0x7F,0x00], # ASCII-Code 0x5D => ]
		[0x04,0x02,0x01,0x02,0x04], # ASCII-Code 0x5E => ^
		[0x40,0x40,0x40,0x40,0x40], # ASCII-Code 0x5F => _
		[0x00,0x01,0x02,0x04,0x00], # ASCII-Code 0x60 => `
		[0x20,0x54,0x54,0x54,0x78], # ASCII-Code 0x61 => a
		[0x7F,0x48,0x44,0x44,0x38], # ASCII-Code 0x62 => b
		[0x38,0x44,0x44,0x44,0x20], # ASCII-Code 0x63 => c
		[0x38,0x44,0x44,0x48,0x7F], # ASCII-Code 0x64 => d
		[0x38,0x54,0x54,0x54,0x18], # ASCII-Code 0x65 => e
		[0x08,0x7E,0x09,0x01,0x02], # ASCII-Code 0x66 => f
		[0x0C,0x52,0x52,0x52,0x3E], # ASCII-Code 0x67 => g
		[0x7F,0x08,0x04,0x04,0x78], # ASCII-Code 0x68 => h
		[0x00,0x44,0x7D,0x40,0x00], # ASCII-Code 0x69 => i
		[0x20,0x40,0x44,0x3D,0x00], # ASCII-Code 0x6A => j
		[0x7F,0x10,0x28,0x44,0x00], # ASCII-Code 0x6B => k
		[0x00,0x41,0x7F,0x40,0x00], # ASCII-Code 0x6C => l
		[0x7C,0x04,0x18,0x04,0x78], # ASCII-Code 0x6D => m
		[0x7C,0x08,0x04,0x04,0x78], # ASCII-Code 0x6E => n
		[0x38,0x44,0x44,0x44,0x38], # ASCII-Code 0x6F => o
		[0x7C,0x14,0x14,0x14,0x08], # ASCII-Code 0x70 => p
		[0x08,0x14,0x14,0x18,0x7C], # ASCII-Code 0x71 => q
		[0x7C,0x08,0x04,0x04,0x08], # ASCII-Code 0x72 => r
		[0x48,0x54,0x54,0x54,0x20], # ASCII-Code 0x73 => s
		[0x04,0x3F,0x44,0x40,0x20], # ASCII-Code 0x74 => t
		[0x3C,0x40,0x40,0x20,0x7C], # ASCII-Code 0x75 => u
		[0x1C,0x20,0x40,0x20,0x1C], # ASCII-Code 0x76 => v
		[0x3C,0x40,0x30,0x40,0x3C], # ASCII-Code 0x77 => w
		[0x44,0x28,0x10,0x28,0x44], # ASCII-Code 0x78 => x
		[0x0C,0x50,0x50,0x50,0x3C], # ASCII-Code 0x79 => y
		[0x44,0x64,0x54,0x4C,0x44], # ASCII-Code 0x7A => z
		[0x00,0x08,0x36,0x41,0x00], # ASCII-Code 0x7B => {
		[0x00,0x00,0x7F,0x00,0x00], # ASCII-Code 0x7C => |
		[0x00,0x41,0x36,0x08,0x00], # ASCII-Code 0x7D => }
		[0x08,0x08,0x2A,0x1C,0x08], # ASCII-Code 0x7E => ->
		[0x08,0x1C,0x2A,0x08,0x08]]  # ASCII-Code 0x7F => <- */

    attr_accessor :frame  #current frame worked on
    attr_reader :frames,:stream   #animation frameset

    def initialize(stream=false)
      @frame = new_frame
      @frames = []
      @stream = stream
    end

    #methods to access in x,y order
    def set(x,y,v)
      @frame[y][x]=v
    end

    def get(x,y)
      @frame[y][x]
    end

    def toggle(x,y)
      @frame[y][x] = !@frame[y][x]
    end
    ########

    #finish one frame
    def flip
      frame = compile_frame(@frame)
      @frames << frame if !@stream
      return frame
    end

    def clear
      @frame = new_frame
    end

    def empty!
      @frames = []
    end

    #stringify animation frameset
    def to_s
      @frames.join("\n")
    end

    #port from MyFont.cpp of Lolshield Lib
    #see original for comments
    def draw(x,char)
      char = char[0].ord

      0.upto(4) do |i|
        mask = 0x01

        0.upto(6) do |j|
          tmps = @@ascii[char][i] & mask
          if tmps == mask
            tmps = true
          else
            tmps = false
          end
          @frame[j][x+i] = tmps if (x+i)<14 && x+i>(-1)
          mask <<= 1
        end

      end

      return true
    end

    def banner(text)
      xoff=14
      0.upto(text.length*5 +52) do |i|
        clear
        0.upto(text.length-1) do |j|
          draw xoff+j*6, text[j]
        end
        xoff-=1
        flip
        @frames.pop if @frames[-1]=="0,0,0,0,0,0,0,0,0" #fix
      end
      return true
    end
    ########

    private
    #returns clean frame
    def new_frame
      Array.new(9){ Array.new(14,false)}
    end

    #converts boolean array to a frame line
    def compile_frame(frame)
      ret = ""
      frame.each{ |line|
        v = 0
        line.each_with_index{ |led,i|
          v += 2**i if led==true
        }
        ret += v.to_s+','
      }
      return ret[0..-2]
    end
end

