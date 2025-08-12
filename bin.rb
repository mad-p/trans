#!/usr/bin/env ruby
sleep(2)
(0..255).each do |ch|
  puts "%02x |%s|" % [ch, ch.chr('binary')]
end
sleep(2)
20.times { puts " " * 6 }
