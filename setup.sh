#!/bin/bash

sudo modprobe rdma_rxe
sudo modprobe dummy

sudo ip link add dummy0 type dummy
sudo ip addr add 10.0.0.1/24 dev dummy0
sudo ip link set dummy0 up

sudo rdma link add rxe0 type rxe netdev dummy0