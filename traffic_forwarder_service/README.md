# Traffic port_forwarder
This project can be used to forward the traffic from one interface to another interface.
**It use service type to map the src and dist**
we use `Differentiated Services Field: (DSCP) ip.dsfield` as a slice ID.
The goal is to map the dst_port field to DSCP value, e.g. `5000:42`


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
#Usage: ./port_service_forwarder  --mapping-method <tatic|random|round-robin> --bind <ip> [tcp:<lport>=<host>:<rport>:<DSCP> ...] [udp:<lport>=<host>:<rport>:<DSCP> ...]

./port_service_forwarder --bind 172.19.0.66 udp:5000=172.19.0.67:5000:40 udp:5001=172.19.0.67:5001:46 udp:5002=172.19.0.67:5002:34 udp:5003=172.19.0.67:5003:26

# --mapping-method round-robin
```
by default, the port_service_forwarder will print to the console when it forward a packet, to disable logs, pass the `--quiet` paramter.
