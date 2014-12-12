require "cbor/version"
begin
  require "cbor/#{RUBY_VERSION[/\d+.\d+/]}/cbor"
rescue LoadError
  require "cbor/cbor"
end
