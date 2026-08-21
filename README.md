# CRANIUM

## Distributed ESP32 Computing Cluster

Cranium is an experimental distributed-computing platform built around an M5Stack ESP32 coordinator and a network of M5Stack Atom Lite nodes.

The goal is to create a small, inexpensive, modular computing cluster in which individual ESP32 devices cooperate over a local WiFi network while a separate management bus provides reliable low-level control and recovery.

> **One brain. Multiple nodes. Distributed computation.**

---

## Overview

Cranium separates the cluster into two communication planes:

```text
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
