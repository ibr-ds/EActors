# Release notes

## 1.1.0

* Old bugs fixed, new added
* Internals:
    - body functions return values, same as constructors
    - FACTORY can spawn enclaves and various network actors
* Codegen process:
    - Now you can add new source (.c), headers (.h) and precompiled libraries (.a) into your project (see examples/http)
    - Enclave limits are project-defined
* New examples:
    - tcp_echo	-- ping-pong via TCP
    - ssl_echo	-- ping-pong via SSL.
    - eos	-- Simple test for the EOS
    - http	-- somewhat HTTP server
