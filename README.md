# CRANIUM

## Distributed ESP32 Computing Cluster

Cranium is an experimental distributed-computing platform built around an M5Stack ESP32 coordinator and a network of M5Stack Atom Lite nodes.

The goal is to create a small, inexpensive, modular computing cluster in which individual ESP32 devices cooperate over a local WiFi network while a separate management bus provides reliable low-level control and recovery.

> One brain. Multiple nodes. Distributed computation.

---

## Overview

Cranium separates the cluster into two communication planes:

                         CRANIUM
                    +---------------+
                    |  COORDINATOR  |
                    | M5Stack ESP32 |
                    +-------+-------+
                            |
              +-------------+-------------+
              |                           |
        MANAGEMENT PLANE             DATA PLANE
             I2C                        WiFi
              |                           |
           PA-HUB                  Cluster Network
              |                           |
      +-------+-------+           +-------+-------+
      |       |       |           |       |       |
     Atom    Atom    Atom        Atom    Atom    Atom
      0       1       2           3       4       5

### I2C - Management Plane

I2C is used for low-level node management.

Responsibilities include:

- Node discovery
- Hardware identification
- Node ID assignment
- Channel management
- Firmware/version information
- Basic health monitoring
- Node identification
- LED control
- Reboot/recovery commands
- WiFi configuration and recovery

I2C is not intended to be the primary data-transfer mechanism.

### WiFi - Cluster/Data Plane

WiFi is the primary communication system for the cluster.

It will eventually carry:

- Jobs
- Job metadata
- Input data
- File shards
- Computational workloads
- Results
- Progress information
- Large data transfers
- Distributed-computing traffic

---

# Hardware

## Coordinator

- 1 x M5Stack ESP32 Core
- PA-HUB I2C multiplexer

## Compute Nodes

- Up to 6 x M5Stack Atom Lite

---

# Architecture

Cranium is being developed in layers.

+---------------------------------------+
|              APPLICATIONS             |
| compression - simulation - search     |
+---------------------------------------+
|          DISTRIBUTED JOB SYSTEM       |
+---------------------------------------+
|             DATA / SHARDING           |
+---------------------------------------+
|             WIFI CLUSTER              |
+---------------------------------------+
|           NODE MANAGEMENT             |
|                 I2C                   |
+---------------------------------------+
|               HARDWARE                |
+---------------------------------------+

---

# Current Development Status

Cranium has reached the point where the coordinator can:

- Communicate with Atom nodes through the PA-HUB
- Select individual I2C channels
- Discover nodes
- Identify nodes by hardware ID
- Assign and manage node IDs
- Track nodes across different PA-HUB channels
- Determine node presence
- Communicate with nodes using the management protocol

Current development milestone:

**v0.6.4**

The next major milestone is **v0.7.0**, which begins the transition to the dual-bus architecture.

---

# Development Roadmap

## Phase 1 - Hardware & Management

### v0.1 - v0.5

Initial hardware, communication, and cluster-management development.

Focus:

- ESP32 coordinator
- Atom nodes
- PA-HUB
- I2C communication
- Node discovery
- Basic management commands

**Status: Complete**

---

## Phase 1.5 - Management Layer Stabilization

### v0.6.x

Focus:

- Reliable node discovery
- Reduced discovery packet size
- Channel management
- Persistent node identity
- Improved error handling
- I2C recovery
- Node management

### v0.6.4

Current stable development milestone.

**Status: Stabilization**

---

# Phase 2 - Dual-Bus Cluster

## v0.7.x

The system will formally separate:

I2C  -> Management
WiFi -> Data / Computing

### v0.7.0

Initial dual-bus architecture.

Planned capabilities:

- WiFi node registration
- Node registry
- WiFi heartbeat
- Node connection status
- I2C health status
- WiFi health status
- Unified node state
- Coordinator display
- Dashboard synchronization

### v0.7.1

WiFi communications protocol.

### v0.7.2

Expanded node management.

### v0.7.3

Communication reliability and recovery.

---

# Phase 3 - Distributed Computing

## v0.8.x

Cranium begins performing actual distributed work.

Planned capabilities:

- Job creation
- Job scheduling
- Work units
- Node assignment
- Progress reporting
- Result reporting
- Dynamic scheduling
- Fault recovery

Example:

                 JOB 001
                    |
              Coordinator
                    |
       +------------+------------+
       |            |            |
    Unit 0       Unit 1       Unit 2
       |            |            |
     Atom 0       Atom 1       Atom 2

---

# Phase 4 - Distributed Data

## v0.9.x

Introduce larger-scale data movement.

Planned capabilities:

- File transfer
- Data blocks
- Sharding
- Distributed processing
- Result reassembly
- Checksums
- Retry/recovery
- Parallel data processing

---

# Phase 5 - Cranium 1.0

The eventual goal is a general-purpose distributed-computing platform.

Potential capabilities:

- Cluster management
- Distributed job scheduler
- Data distribution
- Fault tolerance
- Node discovery
- Dynamic workload allocation
- Firmware management
- Web dashboard
- Plugin/task architecture
- Distributed compression
- Distributed simulation
- Distributed search
- Performance monitoring

---

# Design Philosophy

Cranium is being developed around several principles.

### 1. Keep the hardware inexpensive

The cluster should be built from inexpensive, readily available microcontrollers.

### 2. Separate management from computation

If WiFi fails, the coordinator should still be able to identify and recover a node through I2C.

### 3. Use the right communication system for the job

I2C is intended for short management transactions.

WiFi is intended for moving data and coordinating computation.

### 4. Distributed by design

Tasks should be divided into independent units whenever possible.

### 5. Failure is expected

A node may disappear, reboot, lose WiFi, or fail during a computation.

The cluster should eventually be able to detect the failure and recover the work.

### 6. Modular architecture

The communication layer, scheduler, data layer, and applications should be independently replaceable.

---

# Potential Applications

## Distributed Compression

Break a search space or transformation space into independent problems and have multiple ESP32 nodes explore it simultaneously.

## Monte Carlo Simulation

Distribute independent simulations across nodes.

## Distributed Mathematical Computation

Examples include:

- Pi estimation
- Numerical experiments
- Optimization
- Search problems

## Distributed File Processing

Split a file into independent regions and process those regions concurrently.

## Experimental Algorithms

Cranium is intended to provide a physical platform for experimenting with unconventional distributed algorithms.

---

# Repository Structure

A planned structure is:

cranium/
|
+-- coordinator/
|   +-- v0.6.4/
|   +-- v0.7.0/
|
+-- node/
|   +-- v0.6.4/
|   +-- v0.7.0/
|
+-- protocol/
|   +-- management/
|   +-- wifi/
|
+-- dashboard/
|
+-- documentation/
|
+-- experiments/
|
+-- README.md

---

# Versioning

Cranium uses semantic-style development versions:

MAJOR.MINOR.PATCH

Examples:

0.6.4
0.7.0
0.8.0
1.0.0

Major architectural changes generally increment the minor version.

Bug fixes and compatibility changes increment the patch version.

The 0.x releases represent experimental development and may contain breaking protocol changes.

Version 1.0.0 will represent the first intentionally stable cluster architecture.

---

# Current Hardware Configuration

The reference prototype currently uses:

1 x M5Stack ESP32 Core
1 x PA-HUB
6 x M5Stack Atom Lite

Not every node must be populated during development.

The system is designed to function with one node and scale upward.

---

# Getting Started

The project is currently under active development.

The recommended development sequence is:

1. Build coordinator
2. Build one Atom node
3. Verify I2C management
4. Verify node identity
5. Verify WiFi registration
6. Add additional nodes
7. Verify cluster communications
8. Implement job scheduling
9. Implement distributed computation
10. Implement distributed data

---

# Project Status

**Experimental / Active Development**

Cranium is not yet a production cluster platform.

The architecture, communication protocols, APIs, and firmware are expected to change during the 0.x development cycle.

---

# License

License: TBD

---

# Name

Cranium refers to the project's central coordinator and the collection of distributed nodes that form a single computational system.

The objective is simple:

> Make a collection of inexpensive microcontrollers behave like one programmable machine.
