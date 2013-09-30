#!/usr/bin/env ruby
#lolplay.rb - lold interface app
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License
require 'socket'
require "#{File.dirname(__FILE__)}/lollib"

DEF_HOST = "localhost"

if ARGV.length == 0
  puts "Usage: lolplay.rb FLAGS [-f|-p] FILE"
  puts "       lolplay.rb FLAGS -m \"message\""
  puts "FLAGS:"
  puts "  -h: lold host, default: #{DEF_HOST}"
  puts "  -p: lold port, default: #{LolHelper::DEF_PORT}"
  puts "  -D: Frame delay (50-1000),  default: #{LolTask::DEF_DELAY}"
  puts "  -T: TTL in sec (0-600),     default: #{LolTask::DEF_TTL}"
  puts "  -C: Channel/Priority (>=0), default: #{LolTask::DEF_PRI}"
  puts
  puts "-A: File is in AsciiFrame format"
  puts "-P: File is PDE file from Lol Shield Theatre Homepage"
  puts "nothing: expecting raw animation file"
  puts
  puts "-m: Send text message"
  exit 1
end

del = LolHelper.eval_arg "-D", nil
ttl = LolHelper.eval_arg "-T", nil
chl = LolHelper.eval_arg "-C", nil
port = LolHelper.eval_arg "-p", LolHelper::DEF_PORT
host = LolHelper.eval_arg "-h", DEF_HOST

#conversion functions

def frames2raw(lines)
  result = []
  chars = lines.map{|l| l[0..13].split(//)}

  while chars.length>=9 do
    strframe = chars.shift(9)
    chars.shift #drop empty line (separator)
    frame = strframe.map do |line| #convert to ints
      val = 0
      line.each_with_index do |led,pos|
        val += led == '.' ? 0 : 1<<pos
      end
      val
    end
    result << frame.join(",")
  end
  return result
end

def pde2raw(lines)
  delay2 = lines.find{|l| l.match("delay")}.gsub(/.*\(/,'').to_i
  result = []
  nums = lines.select{|l| l.match(/^\s+DisplayBitMap/)}.map{|l| l.gsub(/.*\(/,'').to_i}

  while nums.length>0
    curr = nums.shift(9)
    result << curr.join(",")
  end

  return [result,delay2]
end

########
lines=[]

if ARGV.index("-m")
  str = ARGV[ARGV.index("-m")+1]
  l = LolFactory.new
  l.banner str
  lines = l.frames
else
  lines = File.readlines(ARGV[-1])
end

#format flags
if ARGV.index("-A") #Frame format
#reads a text file with an animation
#file shall have frames separated by newlines,
#9 rows, 14 cols per frame, . = off, everything else = on
#first argument: input file, second optional: prints source
  lines = frames2raw lines
elsif ARGV.index("-P") #lol shield theatre Pde format
  #which can be downloaded on the lol shield theatre web site
  lines,del = pde2raw lines
end
#otherwise - do nothing, assume "raw" correct input

#send animation task
params={host:host, port:port, del:del, ttl:ttl, pri:chl, frames:lines}
LolHelper.send params
