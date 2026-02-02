# Traffic port_forwarder
This project can be used to forward the traffic from one interface to another interface.
**It use Ports to map the src and dist**


## build
```shell
mkdir build && cd build
cmake ..
make

```

## Build docker image
```shell
docker build -t port_forwarder .
```


## How to use it

### In terminal:
```shell
#Usage: ./port_forwarder  --mapping-method <static|random|round-robin> --bind <ip> [tcp:<lport>=<host>:<rport>:<sport> ...] [udp:<lport>=<host>:<rport>:<sport> ...]


Examples:

./port_forwarder --bind 172.19.0.66 udp:5000=172.19.0.67:5000:5000 udp:5001=172.19.0.67:5001:5001 udp:5002=172.19.0.67:5002:5002 udp:5003=172.19.0.67:5003:5003


./port_forwarder --mapping-method random --bind 172.19.0.66 udp:5000=172.19.0.67:5000:5000 udp:5001=172.19.0.67:5001:5001 udp:5002=172.19.0.67:5002:5002 udp:5003=172.19.0.67:5003:5003


./port_forwarder --mapping-method round-robin --bind 172.19.0.66 udp:5000=172.19.0.67:5000:5000 udp:5001=172.19.0.67:5001:5001 udp:5002=172.19.0.67:5002:5002 udp:5003=172.19.0.67:5003:5003
