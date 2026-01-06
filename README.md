# Quizzie

Quizzie is an online multiple-choice quiz system consisting of a C-based Server and a GTK+ Client.

## Structure

- **client/**: Source code for the GTK+ client application.
- **server/**: Source code for the TCP server application.
- **data/**: Storage for questions and user data.
    - `questions/`: CSV files containing question banks.
    - `users/`: User account data.
- **docs/**: Documentation including SRS.

## Building

To build both client and server:

```bash
make
```

Binaries will be placed in the `bin/` directory.

## Running

Start the server:
```bash
./bin/server [port]
```

Start the client (requires X server/display):
```bash
./bin/client
```
