# UDP Client–Server Test Application

## Overview

This project provides a **UDP-based client–server traffic generator** used to validate the behavior of the **SliceEdge forwarding component**. It simulates controlled traffic from a client to a server through an intermediate forwarding service.

The system is designed for **network slicing and traffic steering experiments**, where SliceEdge performs packet forwarding between network endpoints.

The client periodically transmits UDP packets to the server at a configurable interval. This enables:

- Traffic generation for performance evaluation  
- Functional testing of SliceEdge forwarding logic  
- Validation of UDP transport behavior in a containerized environment  

The client and server applications are implemented in **Python**, while the forwarding service is implemented in **C++**.

---

## System Architecture

**Traffic Path**

```
Client (172.28.0.4)
   │ UDP:5000
   ▼
Forwarder / SliceEdge (172.28.0.3)
   │ forwards
   ▼
Server (172.28.0.2:9000)
```

The forwarder performs **port and protocol mapping**:

| Protocol | Incoming Port | Destination |
|----------|---------------|-------------|
| TCP      | 8080          | 172.28.0.2:9000 |
| UDP      | 5000          | 172.28.0.2:9000 |

---

## Components

| Component | Language | Description |
|----------|----------|-------------|
| Client | Python | Sends periodic UDP messages |
| Server | Python | Receives and logs UDP packets |
| SliceEdge Forwarder | C++ | Performs traffic forwarding |

---

## Network Configuration

A custom Docker bridge network is created:

| Container | IP Address |
|-----------|------------|
| Server | 172.28.0.2 |
| Forwarder | 172.28.0.3 |
| Client | 172.28.0.4 |

**Subnet:** `172.28.0.0/16`

---

## Prerequisites

- Docker ≥ 20.x  
- Docker Compose plugin enabled  

Verify installation:

```bash
docker --version
docker compose version
```

---

## Build and Run

The recommended way to run the system is via Docker Compose. This automatically:

- Builds the client image  
- Builds the server image  
- Compiles the C++ SliceEdge forwarder  
- Creates the custom network  
- Launches all services in the correct order  

```bash
docker compose up --build
```

Run in background:

```bash
docker compose up -d --build
```

---

## Logs

View logs for all services:

```bash
docker compose logs -f
```

Per container:

```bash
docker logs pfw_client
docker logs pfw_forwarder
docker logs pfw_server
```

---

## Stopping the System

```bash
docker compose down
```

Remove networks and volumes:

```bash
docker compose down -v
```

---

## Use Case

This setup is intended for:

- Network slicing validation  
- Traffic forwarding experiments  
- UDP transport testing in virtualized environments  
- SliceEdge functional verification  

---

## Notes

- The server must listen on **port 9000**  
- The client sends traffic to the forwarder, **not directly to the server**  
- Static IP assignment requires the defined subnet not to conflict with existing Docker networks  
- Ensure the `traffic_forwarder_service` directory exists at the correct relative path for Docker build  

---
