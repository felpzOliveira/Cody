# Cody
This is my personal code editor I'm writting in my spare time. It is mostly for my own use however if you want to, feel free to grab it.
The editor is mostly for C/C++ coding with support for glsl, I do however intend to add support for python/javascript and other languages once
I get enough motivation to re-write the lex part.

![Alt text](images/editor2.png?raw=true "Cody")

Current features:
 * Basic synthax highlight based on raw parsing;
 * General editing of text files, it doesn't however support binary files (will crash);
 * Several different tools for handling tabing/re-tabing/spaces;
 * Multi-thread searching and edits;
 * Minimal git integration, i.e.: diff and status;
 * Remote file editing using a custom server application (builds with the editor);
 * Dedicated AES-CBC implementation for encryption/decryption using PKCS#7 (asymetric key encryption is a wip);
 * SHA-1 and SHA-3 implementation for basic hashing.

Work in progress:
 * Editor currently already has a widget layer but no component uses it yet;
 * Editor already has integration with gdb debugger to run under linux, however needs more interfaces for displaying execution status;
 * Editor already has integration with libgit2 and is able to do basic git but needs more (history/graphs/etc...);
 * More robust encryption/decryption and protocols, i.e.: Frodo and EC support.

Known issues:
 * See todo file.

## Building
The editor was tested on Ubuntu and Windows 11. Most of the code is self-contained with exception of libgit2
that is a submodule.

Clone it recursively:
```bash
git clone --recursive <cody>
```
Simply build it:
```bash
mkdir build
cd build
cmake ..
make -j4
```
## Running remote:
You can build the server on the target machine without compiling the main editor and remove a lot of code from cody itself with:
```bash
cmake -DCODY_BUILD=OFF ..
make -j4
```
this makes it easy to compile the server in different machines without many troubles, resulting in the binary `server` only.

Run it informing which port to use with:
```bash
./bin/server --port 20000 --newkey
******************************************************************
* Started server with the key:                                   *
 d2432f0a0bad6cc83ad220f40e4bb30212457e8adcacbad6ff32086c757d24c0 
******************************************************************
29 Jun 12:04:09 : [Server] Setting /home/felipe as starting path
29 Jun 12:04:09 : [Server] Waiting for connection on port 20000
```
the flag `--newkey` generates an AES-256 key that should be used for connection (while key exchange is not implemented).

On the other machine run cody passing this key:
```bash
./bin/cody --remote <ip> 20000 --key d2432f0a0bad6cc83ad220f40e4bb30212457e8adcacbad6ff32086c757d24c0
```
If everything went well, cody should simply start and you are now editing remote files.

![Alt text](images/editor1.png?raw=true "Cody")
