# FT_IRC

Here is the work I have done so far. The MODE with options is not implement yet.
I didn't ran heavy tests.

How to test it? :
  - From one terminal:
    - make
    - ./ircserv 'port' 'password' (e.g: ./ircserv 80 password)
  - From another Terminal (correspond to a client to the server):
    - nc 127.0.0.1 'port' (e.g: nc 127.0.0.1 80) (nc correspond to netcat)
    - PASS 'password' (e.g: PASS pass) (just to connect to our server)

