# *E*Actors is an actor-based programming framework for SGX.

Version 1.1.0

## Overview:

### Why actors? 

Intel SGX has several disadvantages: 
* High transition costs (ECalls are in 50 times slow compared to system calls)
* Limited EPC (~92MiB are available without a paging)  
* Bare-metal style of programming (No dependencies) 

Actors in *E*Actors:
* Use ECall-free messages
* Lightweight (good for  the EPC size) 
* Very flexible (can be trusted or untrusted) 

Compared to other monolithic frameworks (SCONE, Haven, Graphene-SGX, Panoply, etc) *E*Actors offers a multi-enclave, 'distributed' design, where each enclave includes the only minimal set of actors, thereby, has the minimal attack surface. 

### Cite the work: 

The original paper is avaliable [here](https://www.ibr.cs.tu-bs.de/users/sartakov/papers/sartakov18eactors.pdf)

```
@inproceedings{sartakov18eactors,
	author = {Sartakov, Vasily A. and Brenner, Stefan and Ben Mokhtar, Sonia and Bouchenak, Sara and Thomas, Ga\"{e}l and Kapitza, R\"{u}diger},
	title = {\textit{E}Actors: Fast and Flexible Trusted Computing Using SGX},
	booktitle = {Proceedings of the 19th International Middleware Conference},
	year = {2018},
	pages = {187--200},
} 

```


## Design

### Basics

    TBD

### Dependencies

* Intel SGX SDK should be installed (tested with v2.3)
* SGX_SDK ?= /path/to/the/sgxsdk should be updated in parts/App/Makefile and parts/Enclave/Makefile or you can use SGX_SDK=/path/to/the/intel/sgxsdk ./run.sh inside the target directory

### Code-generation process

build.py generates the source tree from actor's sources and a configuration file (XML), which describes the deployment of actors.

* tools/build.py file xml port sys:
    -  file: Actor source file
    - xml: Confguration file
    - port: TCP/IP port (default port for TCP/IP-based examples)
    - sys: source file with system actors

The source directory should also include Enclave.config.xml, which defines heap/code size and some other SGX-related variables.
Optionaly, the extras.cfg file can be provided, where C_FILES is a list of additional C files of the project, H_FILES is a list of headers while A_FILES is a list of libraries which need to be compiled into the project.
See the [http](examples/http) example.


Example:
```
tools/build.py examples/template/template.c examples/template/template-1.xml 0 parts/App/systhreads_factory.cxx
```

The framework also generates  Makefiles and run.sh inside the target folder. You should use this script since it generates MEASUREMENTs, which are necessary for LARSA.

### Messaging 

* Nodes: memory chunks with a header and a payload. Nodes are allocated by the framework.
* Queues: double-linked lists on top of the Hardware-Lock Elision
* API: (Push/pop)-(back/front)
* MBOXes: API-based FIFOs for messaging
* POOLs: API-based LIFOs for empty messages
* Connectors: High-level communication interfaces which hide the implementation of difference between cross/within encrypted/non-encrypted communications. For enclaved actors only (fixable)
* Cargos: compound objects produced by connectors. Used in 1-1 messaging. 

### Encrypted cargos and connectors

The framework supports three encryption scheme for messages (GCM, CTR and MEMCPY) and two key-exchange procedures (SEAL and LARSA).

#### GCM
Full-fledged AES Galois/Counter Mode. Both sides increment IV on each package.

#### CTR
AES Counter Mode. Counters are random but fixed. Used as deterministic encryption in the deprecated POS. Can be used in messaging on your own security risks. Approximately 30% faster compared to GCM.

#### MEMCPY 
For testing purposes only. memcpy instead of encryption 

#### SEAL
Both sides use SEALing for data exchange. Should be considered as vulnerable scheme, because an intruder can unseal all messages (and get an access to all encryption keys) is they gets full access to an enclave.

#### LARSA
Local Attestation with RSA. Combines two procedures: Local attestation is used to attest the slave enclave and send RSA key. Then this key is used to transfer encryption context for the future encryption of the traffic (GCM for example). The most secure approach.

### Networking 

The framework uses system actors for networking. systhreads_factory.cxx contains the FACTORY actor which spawns network actors via requests. Network actors use mboxes for communications and can transparently stitch mboxes over a network.

### Storage

The EOS is an object store. It includes API and the CLEANER actors. Any chunk of memory can be turned into the EOS. Can be trusted and untrusted, an untrusted EOS can be persistent or non-persisyent, as well as encrypted or non-encrypted.
The EOS supports binary and string-based requests (putb/put).

### Examples

* Hellow World (examples/template)
    - Simple hello-world actor

* FIFO test (examples/fifo_test)
    - Developments purpose only

* ping-pong (examples/pingpong)
    - Uses a low-level mbox-based non-encrypted messages
    - Supports trusted and untrusted actors

* ping-pong2 (examples/pingpong2)
    - Uses cargo-based communications
    - Can use private or public cargos
    - Can encrypt or not encrypt messages
    - Can use only trusted actors
    - Supports SEAL and LARSA, various encryption schemes

* ping-pongLA (examples/pingpongLA)
    - Shows how mboxes can be used for the local attestation

* ping-pongDH (examples/pingpongDH)
    - Diffie-Hellman key exchange procedure on top of mboxes

* ping-pongN (examples/pingpongN)
    - Uses non-encrypted cargos over a network.

* tripong (triping.c)
    - Same as ping-pong2, but involves 3 chained actors.
    - Uses LARSA

* Secure Multi-party Computation (smc)
    - Computed by multiple nodes the secure sum
    - Encrypted cargo-based messages between parties
    - Static and dynamic protocols
    - Different vector lengths, number of parties
    - Uses LARSA

* *E*Actors object store (EOS) (examples/eos)
    - Tests put/putb/get/getb
    - Encrypted and non-encrypted

* HTTP (examples/http)
    - Somewhat HTTP server with SSL support
    - Spawns own network actors
    - Supports multiple connections
    - Keys, pages, certificates are inside an enclave

* TCP-echo (examples/tcp_echo)
    - Ping-pong over TCP/IP
    - Uses default network actors

* SSL-echo (examples/ssl_echo)
    - Ping-pong over SSL
    - Uses default network actors


### Documentation

$doxygen doxyfile 
