# Daemon Process

Exploring how to create a basic daemon process in C.
This is inspired by the matt daemon project of school 42 (see .pdf file)

## Usage

```
gcc main.c -omatt_daemon
./matt_daemon
```

The pid of the daemon process will be in `./daemon.pid` and the output of the
process is in `./daemon.log`.

The daemon starts a simple TCP server and stops when you send the string `quit`
or when you send an interrupt signal to it.

You can use the `nc` utility to communicate with it:

```
nc localhost 8042
hello
quit
```

Which should result in the following log file:

```
Sat Jan  4 19:08:28 202 - Started
Sat Jan  4 19:08:28 202 - Waiting for a connection
Sat Jan  4 19:08:36 202 - Read hello
Sat Jan  4 19:08:37 202 - Read quit
Sat Jan  4 19:08:37 202 - Cleanup
Sat Jan  4 19:08:37 202 - Quitting after 'quit' command
```
