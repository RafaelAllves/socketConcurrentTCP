# socketConcurrentTCP
This repository houses the source code of a music server system designed to accommodate multiple users concurrently. At its core, the project entails the implementation of a concurrent server using TCP sockets to facilitate client-server communication.

At the current development point, the server is capable of handling the following actions:
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
  gcc -o server.exe server.c -lsqlite3
```

This will generate an executable file called server. 

<b>PS.</b> Further modifications can be made in its system for it to run as a standalone program in local mode too. For it, just go to `system.c` file, set the `DEBUG` flag to 1 to activate its local output, comment the code sections marked as "Comment for standalone" and write a <b>main</b> function to call `serve_client_admin` with a mock ip and port. Then compile it with `gcc -o local_system.exe system.c -lsqlite3`.

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
The server will now be listening for connections on the specified port. The default port is set to 3490.







### Compiling the Clients

To compile the clients, you can use the `gcc` compiler as follows:

For normal users (read only):
```bash
  gcc -o <executable_name>.exe client.c
```

For admin (read and write): 
```bash
  gcc -o <executable_name>.exe admin.c
```
Replace *<executable_name>* with the desired name of the executable.

Example:
```bash
  gcc -o client.exe client.c
```
```bash
  gcc -o admin.exe admin.c
```
This will generate an executable file called client.exe for normal users (read only) and admin.exe for admin users (read and write permissions).


### Running the Clients

To run the clients, simply type the following command:

```bash
  ./<executable_name>.exe -i <server_machines_ip>
```
Replace <executable_name> with the name of the executable and `<server_machines_ip>` with the IP where the server will be running. In the case where `-i <server_machines_ip>` is not specified `localhost` will be used as IP.

Example:
```bash
  ./client.exe -i 127.0.0.1
```
```bash
  ./admin.exe -i 127.0.0.1
```


After successful connection, you can interact with the server as intended.