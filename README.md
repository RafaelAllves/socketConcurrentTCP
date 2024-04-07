# socketConcurrentTCP
This repository houses the source code of a music server system designed to accommodate multiple users concurrently. At its core, the project entails the implementation of a concurrent server using TCP sockets to facilitate client-server communication.

At the current development point, the server is capable of handling the following actions at the server side:
- Inclusion of music data
- Removal of music data
- Preview listing by year filter
- Preview listing by year and laguage filter
- Preview listing by genre
- Listing by ID
- Listing of all available songs

## Compilation and Execution

### Dependencies

To compile the server SQLite3 will be needed, to install it simply run:
```bash
  sudo apt-get install libsqlite3-dev
```

### Compiling the Server

To compile the server, you can use the `gcc` compiler as follows:
```bash
  gcc -o <executable_name>.exe <source_file>.c <dependencies>
```
Replace *<executable_name>* with the desired name of the executable, *<source_file>* with the name of the source file and *<dependencies>* with its necessaries dependencies.

Example:
```bash
  gcc -o server.exe system.c -lsqlite3
```

This will generate an executable file called server. 

Further modifications can be made for it to run as a local standalone prgram too. For it, just set `DEBUG` variable to 1, unncomment the <b>main</b> function and comment the 3 lines who refers to the call of <b>send</b> function inside the <b>send_to_client</b> function.

### Running the Server
To run the server, simply type the following command:

```bash
  ./<executable_name>.exe
```
Replace <executable_name> with the name of the executable

Example:
```bash
  ./server.exe
```
The server will now be listening for connections on the specified port.

## Customer Connection
To connect to the server, you can use any TCP client. Here is an example using telnet:

```bash
  telnet <IP_DO_SERVIDOR> <PORTA_DO_SERVIDOR>
```
Replace <SERVER_IP> with the IP address of the server and <SERVER_PORT> with the port the server is listening on.

Example:
```bash
  telnet 127.0.0.1 3490
```
This will connect to the server running on the local machine on port 3490.

After successful connection, you can interact with the server as needed.