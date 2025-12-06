bulb broadcasts id
player broadcasts beacon (with duration?, current time?)
player has a list of clients, adds ips based on players broadcasts
when broadcasts init on bootup

possible:

- player streams and buffers 1000 samples at a time to the leds via udp
- bulb connects to the player via http or tcp and asks for 3000 to 4000, 4000 to 5000 etc

possible:

- player sends current frame via unicast to on led at a time, in an interval of 1s
- player broadcasts frame num
-
