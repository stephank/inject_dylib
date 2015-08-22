#!/usr/bin/env ruby

# inject_dylib, https://github.com/stephank/inject_dylib
# Copyright (c) 2015 Stéphan Kochen
# See the README.md for license details.

lines = STDIN.readlines.map do |line|
    if line =~ /^\h+\s+(\h+)\s+(.+)\s*$/
        { :t => "  #{$~[2]}", :b => $~[1].scan(/../).map{|s| "0x#{s}," }.join(' ') }
    else
        { :t => line.strip, :b => '' }
    end
end
pad = lines.map{|l| l[:b].length }.max

puts 'static const unsigned char program_code[] = {'
lines.each{|l| puts "#{l[:b].ljust(pad)}  // #{l[:t]}" }
puts '};'
