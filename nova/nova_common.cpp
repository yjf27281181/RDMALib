
//
// Created by Haoyu Huang on 4/1/19.
// Copyright (c) 2019 University of Southern California. All rights reserved.
//

#include <sys/stat.h>
#include "nova_common.h"

namespace nova {
    std::string ibv_wr_opcode_str(ibv_wr_opcode code) {
        switch (code) {
            case IBV_WR_RDMA_WRITE:
                return "WRITE";
            case IBV_WR_RDMA_WRITE_WITH_IMM:
                return "WRITE_IMM";
            case IBV_WR_SEND:
                return "SEND";
            case IBV_WR_SEND_WITH_IMM:
                return "SEND_IMM";
            case IBV_WR_RDMA_READ:
                return "READ";
            case IBV_WR_ATOMIC_CMP_AND_SWP:
                return "CAS";
            case IBV_WR_ATOMIC_FETCH_AND_ADD:
                return "FA";
        }
    }

    std::string ibv_wc_opcode_str(ibv_wc_opcode code) {
        switch (code) {
            case IBV_WC_SEND:
                return "SEND";
            case IBV_WC_RDMA_WRITE:
                return "WRITE";
            case IBV_WC_RDMA_READ:
                return "READ";
            case IBV_WC_COMP_SWAP:
                return "CAS";
            case IBV_WC_FETCH_ADD:
                return "FA";
            case IBV_WC_BIND_MW:
                return "MW";
            case IBV_WC_RECV:
                return "RECV";
            case IBV_WC_RECV_RDMA_WITH_IMM:
                return "RECV_IMM";
        }
    }

    vector<Host> convert_hosts(string hosts_str) {
        RDMA_LOG(INFO) << hosts_str;
        vector<Host> hosts;
        std::stringstream ss_hosts(hosts_str);
        uint32_t host_id = 0;
        while (ss_hosts.good()) {
            string host_str;
            getline(ss_hosts, host_str, ',');

            if (host_str.empty()) {
                continue;
            }
            std::vector<std::string> ip_port;
            std::stringstream ss_ip_port(host_str);
            while (ss_ip_port.good()) {
                std::string substr;
                getline(ss_ip_port, substr, ':');
                ip_port.push_back(substr);
            }
            Host host = {};
            host.server_id = host_id;
            host.ip = ip_port[0];
            host.port = atoi(ip_port[1].c_str());
            hosts.push_back(host);
            host_id++;
        }
        return hosts;
    }
}