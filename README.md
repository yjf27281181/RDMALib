# RDMALib
A C++ library for RDMA. It is built on top of https://github.com/wxdwfc/rlib. 

# Interface
RDMA RC: https://github.com/HaoyuHuang/RDMALib/blob/master/nova/nova_rdma_rc_store.h

Message callback: https://github.com/HaoyuHuang/RDMALib/blob/master/nova/nova_msg_callback.h

# Dependencies
fmt https://fmt.dev/latest/index.html

gflags https://github.com/gflags/gflags

# Build
```
cmake .
make
```

# Example
Prepare two nodes with RDMA capability. In this example, node-0 issues a request to node-1 using RDMA_SEND_WITH_IMM. 

Run the following command on node-0: 
```
./example_main --mem_pool_size_gb=1 --nrdma_workers=1 --rdma_doorbell_batch_size=8 --rdma_max_msg_size=1024 --rdma_max_num_sends=128 --rdma_port=11211 --server_id=0 --servers=node-0:11211,node-1:11211
```

Run the following command on node-1: 
```
./example_main --mem_pool_size_gb=1 --nrdma_workers=1 --rdma_doorbell_batch_size=8 --rdma_max_msg_size=1024 --rdma_max_num_sends=128 --rdma_port=11211 --server_id=1 --servers=node-0:11211,node-1:11211
```

An example C++ code snippet. 
```c++
NovaRDMARCBroker *broker = new NovaRDMARCBroker(circular_buffer_, 0,
                                        endpoints_,
                                        FLAGS_rdma_max_num_sends,
                                        FLAGS_rdma_max_msg_size,
                                        FLAGS_rdma_doorbell_batch_size,
                                        FLAGS_server_id,
                                        rdma_backing_mem_,
                                        FLAGS_mem_pool_size_gb *
                                        1024 *
                                        1024 * 1024,
                                        FLAGS_rdma_port,
                                        new DummyNovaMsgCallback);
broker->Init(ctrl_);

if (FLAGS_server_id == 0) {
int server_id = 1;
char *sendbuf = broker->GetSendBuf(server_id);
// Write a request into the buf.
sendbuf[0] = 'a';
uint64_t wr_id = broker->PostSend(sendbuf, 1, server_id, 1);
RDMA_LOG(INFO) << fmt::format("send one byte 'a' wr:{} imm:1", wr_id);
broker->FlushPendingSends(server_id);
broker->PollSQ(server_id);
broker->PollRQ(server_id);
}

while (true) {
broker->PollRQ();
broker->PollSQ();
}
```

