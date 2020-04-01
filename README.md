# RDMALib
A C++ library for RDMA. It is built on top of https://github.com/wxdwfc/rlib. 

# Interface
RDMA RC: https://github.com/HaoyuHuang/RDMALib/blob/master/nova/nova_rdma_rc_store.h

Message callback: https://github.com/HaoyuHuang/RDMALib/blob/master/nova/nova_msg_callback.h

# Example
```c++
char *rdma_backing_mem = (char *) malloc(
            FLAGS_mem_pool_size_gb * 1024 * 1024 * 1024);
memset(rdma_backing_mem, 0, FLAGS_mem_pool_size_gb * 1024 * 1024 * 1024);
std::vector<Host> hosts = convert_hosts(FLAGS_servers);

RdmaCtrl *ctrl = new RdmaCtrl(FLAGS_server_id, FLAGS_rdma_port);
std::vector<QPEndPoint> endpoints;
for (int i = 0; i < hosts.size(); i++) {
    if (hosts[i].server_id == FLAGS_server_id) {
        continue;
    }
    QPEndPoint endpoint = {};
    endpoint.thread_id = 0;
    endpoint.server_id = hosts[i].server_id;
    endpoint.host = hosts[i];
    endpoints.push_back(endpoint);
}

// Each QP contains nrdma_buf_unit() memory for the circular buffer.
// An RDMA store uses nrdma_buf_unit() * number of servers memory for its circular buffers.
// Each server contains nrdma_buf_unit() * number of servers * number of rdma threads for the circular buffers.

// We register all memory to the RNIC.
// RDMA verbs can only work on the memory registered in RNIC.
// You may use nova mem manager to manage this memory.
char *user_memory = rdma_backing_mem + nrdma_buf_total();
uint32_t partitions = 1;
uint32_t slab_mb = 1;
NovaMemManager *mem_manager = new NovaMemManager(user_memory, partitions,
                                                 FLAGS_mem_pool_size_gb,
                                                 slab_mb);
uint32_t scid = mem_manager->slabclassid(0, 40);
char *buf = mem_manager->ItemAlloc(0, scid);
// Do sth with the buf.
mem_manager->FreeItem(0, buf, scid);

// A thread i at server j connects to thread i of all other servers.
NovaRDMARCStore *store = new NovaRDMARCStore(rdma_backing_mem, 0, endpoints,
                                             FLAGS_rdma_max_num_sends,
                                             FLAGS_rdma_max_msg_size,
                                             FLAGS_rdma_doorbell_batch_size,
                                             FLAGS_server_id,
                                             rdma_backing_mem,
                                             FLAGS_mem_pool_size_gb * 1024 *
                                             1024 * 1024, FLAGS_rdma_port,
                                             new DummyNovaMsgCallback);
store->Init(ctrl);
int server_id = 1;
char *sendbuf = store->GetSendBuf(server_id);
// Write a request into the buf.
buf[0] = 'a';
uint64_t wr_id = store->PostSend(sendbuf, 1, server_id, 0);
store->FlushPendingSends(server_id);
store->PollSQ(server_id);
store->PollRQ(server_id);
```
# Dependencies
fmt https://fmt.dev/latest/index.html

gflags https://github.com/gflags/gflags

# Build
```
cmake .
make
```

