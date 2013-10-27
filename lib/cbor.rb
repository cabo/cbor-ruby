here = File.expand_path(File.dirname(__FILE__))
require File.join(here, 'cbor', 'version')
begin
  m = /(\d+.\d+)/.match(RUBY_VERSION)
  ver = m[1]
  require File.join(here, 'cbor', ver, 'cbor')
rescue LoadError
  require File.join(here, 'cbor', 'cbor')
end
