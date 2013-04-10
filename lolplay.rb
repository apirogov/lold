#!/usr/bin/env ruby
#lolplay.rb - lold interface app
#Copyright (C) 2013 Anton Pirogov
#Licensed under the MIT License
require 'socket'
require "#{File.dirname(__FILE__)}/lollib"

DEF_HOST= "localhost"
DEF_PORT= 10101

DEF_D = 50
DEF_T = 60
DEF_C = 0

if ARGV.length == 0
  puts "Usage: lolplay.rb FLAGS [-f|-p] FILE"
  puts "       lolplay.rb FLAGS -m \"message\""
  puts "FLAGS:"
  puts "  -h: lold host, default: #{DEF_HOST}"
  puts "  -p: lold port, default: #{DEF_PORT}"
  puts "  -D: Frame delay (50-1000),  default: #{DEF_D}"
  puts "  -T: TTL in sec (0-600),     default: #{DEF_T}"
  puts "  -C: Channel/Priority (>=0), default: #{DEF_C}"
  puts
  puts "-F: File is in AsciiFrame format"
  puts "-F: File is PDE file from Lol Shield Theatre Homepage"
  puts "nothing: expecting raw animation file"
  puts
  puts "-m: Send text message"
  exit 1
end

#simple argument evaluation
def eval_arg(arg,default)
  i = ARGV.index(arg)
  return default if i.nil?
  ret = ARGV[i+1]
  2.times{ ARGV.delete_at(i) }
  return ret
end

del = eval_arg "-D", DEF_D
ttl = eval_arg "-T", DEF_T
chl = eval_arg "-C", DEF_C
host = eval_arg "-h", DEF_HOST
port = eval_arg "-p", DEF_PORT

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
        val += led == '.' ? 0 : 2**pos
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
if ARGV.index("-F") #Frame format
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

s = TCPSocket.open(host,port)
s.puts "TASK"
s.puts "DELAY #{del}"
s.puts "TTL #{ttl}"
s.puts "CH #{chl}"

s.puts "DATA"
lines.each do |l|
  s.puts l
end
s.puts "END"
#puts s.gets.chomp #response, we don't care normally
s.close
