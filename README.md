This is my Attempt of the "Build Your Own Redis" Challange.

This Redis clone includes simple functionality such as GET and SET commands, 
But it is being expanded upon with features like Replication, RDB Persistance, and more.

To compile, run:

```sh
make server
```

The server can take the following arguments:
1. --port <PORT> to specify a port for the server to listen on
2. --replicaof <MASTER_HOST> <MASTER_PORT> to specify that it is a replica and the master

With more features to be added.

