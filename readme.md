## Build


Use make :
`make`


## Run


Server usage :
	`./server [port]`
  `./parserver [port]`
Client usage :
	`./client`


## Tips


1-Server will bind at 0.0.0.0 and clients will be connect to 127.0.0.1:1403

2-If you ran a command and expect more output simply pass it a # as a command so you will
receive them.

3-Clients will be automatically connect again after disconnecting by typing exit. This is for 
preventing unexpected disconnections. After 4 or 5 times (10s) client would never connect again.

4-Sometimes after running server and client over and over on same port, client may not connect 
to server. So easily change the port or try the code after 2 or 3 minutes.

5-If you want to run `dmesg` you should run client as super user.

6-Run `shutdown` for stopping the server.

**for parserver please run help to see helps**


