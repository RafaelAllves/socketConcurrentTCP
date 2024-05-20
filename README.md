# socketConcurrentTCP
This repository houses the source code of a music server system designed to accommodate multiple users concurrently. At its core, the project entails the implementation of a concurrent server using TCP sockets for message exchange and files uploading and UDP sockets for files downloading to facilitate client-server communication.

At the current development point, the server is capable of handling the following actions:
- Inclusion of music data
- Music upload
- Removal of music data
- Preview listing by year filter
- Preview listing by year and laguage filter
- Preview listing by genre
- Listing by ID
- Listing of all available songs
- Download of music by UDP protocol with integrity checking

## Compilation and Execution

### Dependencies

To compile the server SQLite3 will be needed, to install it simply run:
```bash
  sudo apt-get install libsqlite3-dev
```

### Compiling the Server

To compile the server, you can use the `gcc` compiler as follows, but keep note that SQLite3 and math libraries will be needed:
```bash
  gcc -o <executable_name>.exe <source_file>.c <dependencies>
```
Replace *<executable_name>* with the desired name of the executable, *<source_file>* with the name of the source file and *<dependencies>* with its necessaries dependencies.

Example:
```bash
  gcc -o server.exe server.c -lsqlite3 -lm
```

This will generate an executable file called server. 

### Running the Server

To run the server, simply type the following command on terminal:

```bash
  ./<executable_name>.exe
```
Replace <executable_name> with the name of the executable

Example:
```bash
  ./server.exe
```
The server will now be listening for connections. It's default ports are 3490 for TCP and 3492 for UDP. On client side this will be set automatically.







### Compiling the Clients

To compile the clients, you can also use the `gcc` compiler as follows:

```bash
  gcc -o <executable_name>.exe client.c
```

Example:
```bash
  gcc -o client.exe client.c -lm
```

This will generate an executable file called `client.exe`. Please keep in note that by default the generated client will be a normal user, who only have reading permissions, for it to have writing permissions (ex: add and upload/remove a file), the flag `admin_flag` inside `client.c` need to be changed from 0 to 1 before it's compilation step.


### Running the Client

To run the client, simply type the following command:

```bash
  ./<executable_name>.exe -i <server_machines_ip>
```
Replace <executable_name> with the name of the executable and `<server_machines_ip>` with the IP where the server will be running. In the case where `-i <server_machines_ip>` is not specified, the `localhost` will be used as its default IP.

Example:
```bash
  ./client.exe -i 127.0.0.1
```


After successful connection, you can interact with the server as intended.

## Tips on music uploading and downloading

- By default on server side the musics are stored on "musics" folder and, by the client side, on "musicas" folder. If any of those folders doesn't exist, by the current version of this doc an exception will occur. If you want to, can modify those paths on `system.c` and `client.c` files on its parameters.

- When running the client and server on different machines remember to open the doors 3490 for tcp and 3492 for udp on firewall, also doing necessary port-forwarding if it is the case.

- A song can only be downloaded if it was uploaded previously by and administrator. This happens because it's filenames are stored on its database.

- 16mb is the maximum file size accepted by the program

And that's it.

Enjoy it! :D