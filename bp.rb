#!/usr/bin/env ruby
# frozen_string_literal: true

require 'nokogiri'

doc = File.open('/home/fagci/Downloads/BandPlan.xml') { |f| Nokogiri::XML(f) }

entries = doc.xpath('//RangeEntry')

entries.each do |e|
  min = e['minFrequency'].to_i
  max = e['maxFrequency'].to_i
  mode = {
    NFM: 'FM',
    WFM: 'FM',
    AM: 'AM',
    USB: 'USB',
    SSB: 'USB'
  }[e['mode'].to_sym]
  step = e['step'].to_i
  next unless step.positive?

  step_t = (e['step'].to_f / 1000).to_s.sub '.', '_'

  range = (max - min) / 1000
  steps_total = range / step
  steps = 16
  steps = 128 if steps_total > 64
  steps = 64 if steps_total > 32
  steps = 32 if steps_total > 16
  name = e.text
  next unless !name.empty? && min >= 16_000_000 && max <= 1_300_000_000

  ln = <<~TEXT
    {"#{name}", #{(min / 10).round}, #{(max / 10).round}, STEPS_#{steps}, S_STEP_#{step_t}kHz, MODE_#{mode} },
  TEXT
  puts ln
end
