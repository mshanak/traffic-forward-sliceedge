# Traffic Forwarder Suite

## Overview

This repository contains multiple projects for **network traffic forwarding and testing**. The goal is to support experimentation, validation, and functional testing of traffic steering mechanisms such as the **SliceEdge** component.

The suite includes both **forwarding implementations** and a **traffic generation application**.

---

## Project Structure

### 1. `traffic_forwarder`

A low-level traffic forwarding application that transfers packets from one network interface to another.

**Key Characteristics**

- Interface-to-interface forwarding  
- Uses **port-based mapping**  
- Suitable for:
  - Basic forwarding experiments  
  - Transport-layer traffic redirection  
  - Network function prototyping  

---

### 2. `traffic_forwarder_service`

An enhanced forwarding service that maps traffic based on **service type** instead of only ports.

**Key Characteristics**

- Service-aware traffic forwarding  
- Designed for **SliceEdge integration**  
- Supports traffic steering based on logical service definitions  
- Suitable for:
  - Network slicing experiments  
  - Service Function Chaining (SFC)  
  - Advanced traffic classification and routing  

---

### 3. `py_client_server`

A **UDP-based client–server traffic generator** used to validate forwarding behavior.

**Purpose**

Simulates controlled traffic from a client to a server through an intermediate forwarding service.

**Key Characteristics**

- Periodic UDP packet transmission  
- Configurable sending interval  
- Implemented in Python  
- Containerized for reproducible experiments  

**Typical Traffic Flow**

```
Client → Forwarder (SliceEdge) → Server
```

---

## Use Cases

This suite can be used for:

- Network slicing validation  
- Forwarding logic testing  
- Service-based traffic steering experiments  
- UDP transport behavior analysis  
- Functional verification of SliceEdge components  

---

## Technology Stack

| Component | Language |
|----------|----------|
| Forwarders | C++ |
| Client/Server | Python |
| Deployment | Docker & Docker Compose |

---

## Notes

- `traffic_forwarder` focuses on **port-based forwarding**  
- `traffic_forwarder_service` focuses on **service-aware forwarding**  
- `py_client_server` generates test traffic for validation  

---
