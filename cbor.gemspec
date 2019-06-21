$:.push File.expand_path("../lib", __FILE__)
require 'cbor/version'

Gem::Specification.new do |s|
  s.name = "cbor"
  s.version = CBOR::VERSION
  s.summary = "CBOR, Concise Binary Object Representation."
  s.description = %q{CBOR is a library for the CBOR binary object representation format, based on Sadayuki Furuhashi's MessagePack library.}
  s.author = "Carsten Bormann, standing on the tall shoulders of Sadayuki Furuhashi"
  s.email = "cabo@tzi.org"
  s.license = "Apache-2.0"
  s.homepage = "http://cbor.io/"
#  s.has_rdoc = false
  s.files = `git ls-files`.split("\n")
  s.test_files = `git ls-files -- {test,spec}/*`.split("\n")
  s.require_paths = ["lib"]
  s.extensions = ["ext/cbor/extconf.rb"]

  s.add_development_dependency 'bundler', RUBY_VERSION.to_f < 2.3 ? ['~> 1.0'] : ['~> 2.0']
  s.add_development_dependency 'rake', ['~> 0.9.2']
  s.add_development_dependency 'rake-compiler', ['~> 0.8.3']
  s.add_development_dependency 'rspec', ['~> 2.11']
  s.add_development_dependency 'json', ['~> 1.7']
  s.add_development_dependency 'yard', ['~> 0.9.11']
end
