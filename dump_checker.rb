#!/usr/bin/env ruby

class DumpChecker
  TAGS = ['l:toenc', 'l:enc-d', 'l:todec', 'l:dec-d', 'r:toenc', 'r:enc-d', 'r:todec', 'r:dec-d']
  
  PAIRS = [
    ['l:toenc', 'r:dec-d'],
    ['l:enc-d', 'r:todec'],
    ['l:todec', 'r:enc-d'],
    ['l:dec-d', 'r:toenc']
  ]

  def initialize(log_files)
    @log_files = log_files
    @streams = {}
    @log_lines = {}
    TAGS.each { |tag| @streams[tag] = [] }
  end

  def parse_log_line(line)
    if match = line.match(/^(\d{2}:\d{2}:\d{2}\.\d{6})\s+([^:]+:[^:]+):\s*(.+)$/)
      timestamp = match[1]
      tag = match[2]
      hex_data = match[3]
      
      return nil unless TAGS.include?(tag)
      
      bytes = hex_data.split.map { |hex| hex.to_i(16) }
      { timestamp: timestamp, tag: tag, bytes: bytes, original_line: line.chomp }
    else
      nil
    end
  end

  def load_logs
    @log_files.each do |file_path|
      File.open(file_path, 'r') do |file|
        file.each_line.with_index do |line, line_no|
          parsed = parse_log_line(line)
          next unless parsed
          
          tag = parsed[:tag]
          @streams[tag].concat(parsed[:bytes])
          
          @log_lines[tag] ||= []
          @log_lines[tag] << {
            file: file_path,
            line_no: line_no + 1,
            original_line: parsed[:original_line],
            byte_position: @streams[tag].length - parsed[:bytes].length
          }
        end
      end
    end
  end

  def find_log_line_for_position(tag, position)
    return nil unless @log_lines[tag]
    
    @log_lines[tag].find do |entry|
      entry[:byte_position] <= position && 
      position < entry[:byte_position] + (entry[:original_line].split(':').last.split.length)
    end
  end

  def check_streams
    errors = []
    
    PAIRS.each do |tag1, tag2|
      stream1 = @streams[tag1]
      stream2 = @streams[tag2]
      
      max_len = [stream1.length, stream2.length].max
      
      (0...max_len).each do |i|
        byte1 = stream1[i]
        byte2 = stream2[i]
        
        if byte1 != byte2
          log_entry1 = find_log_line_for_position(tag1, i)
          log_entry2 = find_log_line_for_position(tag2, i)
          
          errors << {
            position: i,
            tag1: tag1,
            tag2: tag2,
            byte1: byte1,
            byte2: byte2,
            log_entry1: log_entry1,
            log_entry2: log_entry2
          }
        end
      end
    end
    
    errors
  end

  def run
    puts "ログファイルを読み込んでいます..."
    load_logs
    
    puts "ストリームを比較しています..."
    errors = check_streams
    
    if errors.empty?
      puts "すべてのストリームペアが一致しています。"
    else
      puts "#{errors.length}個の不一致が見つかりました:"
      puts
      
      errors.each do |error|
        puts "位置 #{error[:position]}: #{error[:tag1]} vs #{error[:tag2]}"
        puts "  #{error[:tag1]}: 0x#{error[:byte1]&.to_s(16)&.upcase || 'N/A'}"
        puts "  #{error[:tag2]}: 0x#{error[:byte2]&.to_s(16)&.upcase || 'N/A'}"
        puts
        
        if error[:log_entry1]
          puts "  #{error[:tag1]} ログ行:"
          puts "    #{error[:log_entry1][:file]}:#{error[:log_entry1][:line_no]}: #{error[:log_entry1][:original_line]}"
        end
        
        if error[:log_entry2]
          puts "  #{error[:tag2]} ログ行:"
          puts "    #{error[:log_entry2][:file]}:#{error[:log_entry2][:line_no]}: #{error[:log_entry2][:original_line]}"
        end
        puts "-" * 80
      end
    end
  end
end

if ARGV.empty?
  puts "使用方法: #{$0} <ログファイル1> [ログファイル2] ..."
  exit 1
end

checker = DumpChecker.new(ARGV)
checker.run