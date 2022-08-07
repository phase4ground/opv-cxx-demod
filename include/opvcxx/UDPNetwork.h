// Copyright 2022 Open Research Institute, Inc.

#pragma once

#include <cstring>
#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct UDPNetwork
{
    int udp_socket = 0;
    struct sockaddr_in dest_address;

    void send_packet(const int length, const uint8_t *buffer)
    {
        if (udp_socket > 0)
        {
            if ((sendto(udp_socket, buffer, length, 0, (const struct sockaddr *)&dest_address, (socklen_t)sizeof(dest_address))) < 0)
            {
                std::cerr << "Error sending to network socket" << std::endl;
            }
            else
            {
                std::cerr << "frame out to UDP " << length << " bytes" << std::endl;
            }
        }
    }

    void network_setup(std::string ipaddr, const uint16_t port)
    {
        struct sockaddr_in my_address_;
        char readback[INET_ADDRSTRLEN];

        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0)
        {
            std::cerr << "Failure creating network socket" << std::endl;
        }

        memset(&my_address_, 0, sizeof(my_address_));
        my_address_.sin_family = AF_INET;
        my_address_.sin_port = htons(port);
        my_address_.sin_addr.s_addr = INADDR_ANY;

        if (bind(udp_socket, (const struct sockaddr *)&my_address_, sizeof(my_address_)) < 0)
        {
            std::cerr << "Failure binding to network socket" << std::endl;
            udp_socket = 0;
        }

        dest_address.sin_family = AF_INET;
        dest_address.sin_port = htons(port);
        inet_pton(AF_INET, const_cast<char*>(ipaddr.c_str()), &(dest_address.sin_addr));
        inet_ntop(AF_INET, &(dest_address.sin_addr), readback, INET_ADDRSTRLEN);
        std::cerr << "Output to network IP = " << readback << " port = " << port << std::endl;
    }
};