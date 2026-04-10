/*
#include "top.h"
#include <iostream>
#include <queue>
#include <vector>
#include <iomanip>

const int SIM_CYCLES    = 100;     // 主仿真时钟
const int INJECT_PERIOD = 10;      // 每隔多少周期注入新包
const int DRAIN_CYCLES  = 200;     // 排空窗口
const int TEST_INPUTS   = 2;       // 本次只测in[0], in[1]

// 两个输入的目标一致，都走EAST口
const ap_uint<2> DST_X[TEST_INPUTS] = {2, 2};
const ap_uint<2> DST_Y[TEST_INPUTS] = {0, 0};
const ap_uint<16> PAYLOAD[TEST_INPUTS] = {10, 20}; // 不同payload便于追踪

struct InFlight {
    int inject_cycle;
    ap_uint<16> payload;
    int input_port;
};

struct Result {
    int input_port;
    ap_uint<16> payload;
    int latency;
    int inject_cycle;
    int recv_cycle;
};

int main() {
    hls::stream<AxiFlit> in [NUM_INPUTS];
    hls::stream<AxiFlit> out[NUM_OUTPUTS];

    RouterCfg cfg;
    cfg.topo  = TOPO_MESH;
    cfg.algo  = ALGO_XY;
    cfg.cur_x = 0;
    cfg.cur_y = 0;

    std::queue<InFlight> inflight[TEST_INPUTS];
    std::vector<Result> result_list;

    int pkt_sent[TEST_INPUTS] = {0};
    int pkt_recv = 0, sum_latency = 0, max_latency = 0;

    // ==== 1. 主仿真周期 ====
    for(int cycle = 0; cycle < SIM_CYCLES; ++cycle) {
        for(int p=0; p<TEST_INPUTS; ++p) {
            // 单次发包（或可多发）
            if(cycle == 0 && pkt_sent[p] == 0) {
                // head
                AxiFlit f_head;
                f_head.data = 0;
                f_head.data.range(31,30) = FLIT_HEAD;
                f_head.data.range(29,28) = DST_X[p];
                f_head.data.range(27,26) = DST_Y[p];
                f_head.data.range(15,0)  = PAYLOAD[p];
                f_head.last = 0;
                in[p].write(f_head);
                // body
                AxiFlit f_body = f_head;
                f_body.data.range(31,30) = FLIT_BODY;
                f_body.last = 0;
                in[p].write(f_body);
                // tail
                AxiFlit f_tail = f_head;
                f_tail.data.range(31,30) = FLIT_TAIL;
                f_tail.last = 1;
                in[p].write(f_tail);

                inflight[p].push({cycle, PAYLOAD[p], p});
                pkt_sent[p]++;
            }
        }
        noc_router_top(in, out, cfg);

        // drain所有输出口，判断flit实际去哪
        for(int outport=0; outport<NUM_OUTPUTS; ++outport) {
            while(!out[outport].empty()) {
                AxiFlit f = out[outport].read();
                ap_uint<2> flit_type = f.data.range(31,30);
                ap_uint<16> payload  = f.data.range(15,0);
                for(int p=0; p<TEST_INPUTS; ++p) {
                    if(!inflight[p].empty() && flit_type==FLIT_TAIL && inflight[p].front().payload==payload) {
                        int latency = cycle - inflight[p].front().inject_cycle + 1;
                        result_list.push_back({p, payload, latency, inflight[p].front().inject_cycle, cycle});
                        if(latency > max_latency) max_latency = latency;
                        sum_latency += latency;
                        pkt_recv++;
                        inflight[p].pop();
                        std::cout << "OUT[" << outport << "] Input " << p
                                  << " Payload " << payload
                                  << " Latency=" << latency << " cycles"
                                  << " Inject@" << cycle-latency+1
                                  << " Recv@" << cycle << std::endl;
                    }
                }
            }
        }
    }

    // ==== 2. DRAIN阶段 ====
    for(int dc=0; dc<DRAIN_CYCLES; ++dc) {
        noc_router_top(in, out, cfg);
        for(int outport=0; outport<NUM_OUTPUTS; ++outport) {
            while(!out[outport].empty()) {
                AxiFlit f = out[outport].read();
                ap_uint<2> flit_type = f.data.range(31,30);
                ap_uint<16> payload  = f.data.range(15,0);
                for(int p=0; p<TEST_INPUTS; ++p) {
                    if(!inflight[p].empty() && flit_type==FLIT_TAIL && inflight[p].front().payload==payload) {
                        int latency = SIM_CYCLES + dc - inflight[p].front().inject_cycle + 1;
                        result_list.push_back({p, payload, latency, inflight[p].front().inject_cycle, SIM_CYCLES + dc});
                        if(latency > max_latency) max_latency = latency;
                        sum_latency += latency;
                        pkt_recv++;
                        inflight[p].pop();
                        std::cout << "DRAIN OUT[" << outport << "] Input " << p
                                  << " Payload " << payload
                                  << " Latency=" << latency << " cycles"
                                  << " Inject@" << SIM_CYCLES+dc-latency+1
                                  << " Recv@" << SIM_CYCLES+dc << std::endl;
                    }
                }
            }
        }
    }

    // ==== 3. 输出统计 ====
    std::cout << "\n=== Conflict Arbitration Test Report ===\n";
    for(const auto& r : result_list) {
        std::cout << "Input " << r.input_port
                  << " Payload " << r.payload
                  << " Inject @" << r.inject_cycle
                  << " Recv @" << r.recv_cycle
                  << " Latency=" << r.latency << " cycles\n";
    }
    std::cout << "Packets sent: " << pkt_sent[0]+pkt_sent[1]
              << ", received: " << pkt_recv << std::endl;
    if(pkt_recv > 0) {
        std::cout << "Average latency: " << (double)sum_latency/pkt_recv << " cycles\n";
        std::cout << "Max latency    : " << max_latency << " cycles\n";
    }
    if(pkt_recv < pkt_sent[0]+pkt_sent[1])
        std::cout << "Warning: Some packets lost or stuck in FIFO!\n";
    return 0;
}
*/
//----------------------------------------------------------------------------------------------------------------------
/*
#include "top.h"
#include <iostream>
#include <queue>
#include <vector>
#include <iomanip>

const int SIM_CYCLES    = 500;    // 仿真总周期
const int INJECT_PERIOD = 10;     // 每N周期每端口注入一个新包
const int DRAIN_CYCLES  = 200;    // drain阶段周期数（可按需加大）

// 自定义目标和payload
const ap_uint<2> DST_X[NUM_INPUTS] = {2, 0, 1, 1, 1};    // 各输入端的目标x
const ap_uint<2> DST_Y[NUM_INPUTS] = {0, 0, 2, 0, 1};    // 各输入端的目标y
const ap_uint<16> PAYLOAD[NUM_INPUTS] = {10, 20, 30, 40, 50}; // payload示例

struct InFlight {
    int inject_cycle;
    ap_uint<16> payload;
};

int main() {
    hls::stream<AxiFlit> in [NUM_INPUTS];
    hls::stream<AxiFlit> out[NUM_OUTPUTS];

    RouterCfg cfg;
    cfg.topo  = TOPO_MESH;
    cfg.algo  = ALGO_XY;
    cfg.cur_x = 1;
    cfg.cur_y = 1;

    // 每个端口一个 in-flight 队列，存 head 注入时刻+payload
    std::vector<std::queue<InFlight>> inflight(NUM_OUTPUTS);

    int pkt_sent[NUM_INPUTS] = {0}; // 每端口注入包数
    int pkt_recv = 0;               // 接收到的包数
    uint64_t sum_latency = 0;
    int max_latency = 0;

    // ========== 1. 主仿真循环 ==========
    for(int cycle = 0; cycle < SIM_CYCLES; ++cycle) {
        // 1. 数据注入
        for(int p=0; p<NUM_INPUTS; ++p) {
            // 每隔 INJECT_PERIOD 周期注入新包
            if(cycle % INJECT_PERIOD == 0) {
                // head
                AxiFlit f_head;
                f_head.data = 0;
                f_head.data.range(31,30) = FLIT_HEAD;
                f_head.data.range(29,28) = DST_X[p];
                f_head.data.range(27,26) = DST_Y[p];
                f_head.data.range(15,0)  = PAYLOAD[p];
                f_head.last = 0;
                in[p].write(f_head);

                // body
                AxiFlit f_body = f_head;
                f_body.data.range(31,30) = FLIT_BODY;
                f_body.last = 0;
                in[p].write(f_body);

                // tail
                AxiFlit f_tail = f_head;
                f_tail.data.range(31,30) = FLIT_TAIL;
                f_tail.last = 1;
                in[p].write(f_tail);

                // 记录注入信息
                inflight[p].push({cycle, PAYLOAD[p]});
                pkt_sent[p]++;
            }
        }

        // 2. DUT 运行
        noc_router_top(in, out, cfg);

        // 3. Drain输出并统计延迟
        for(int p=0; p<NUM_OUTPUTS; ++p) {
            while(!out[p].empty()) {
                AxiFlit f = out[p].read();
                ap_uint<2> flit_type = f.data.range(31,30);
                ap_uint<16> payload  = f.data.range(15,0);
                if(flit_type == FLIT_TAIL) {
                    if(!inflight[p].empty()) {
                        int inject_cycle = inflight[p].front().inject_cycle;
                        int lat = cycle - inject_cycle + 1;
                        sum_latency += lat;
                        if(lat > max_latency) max_latency = lat;
                        pkt_recv++;
                        std::cout << "Out[" << p << "] "
                                  << "payload=" << payload
                                  << " latency=" << lat
                                  << " cycles" << std::endl;
                        inflight[p].pop();
                    }
                }
            }
        }
    }

    // ========== 2. DRAIN 阶段 ==========
    for(int dc=0; dc<DRAIN_CYCLES; ++dc) {
        noc_router_top(in, out, cfg);

        for(int p=0; p<NUM_OUTPUTS; ++p) {
            while(!out[p].empty()) {
                AxiFlit f = out[p].read();
                ap_uint<2> flit_type = f.data.range(31,30);
                ap_uint<16> payload  = f.data.range(15,0);
                if(flit_type == FLIT_TAIL) {
                    if(!inflight[p].empty()) {
                        int inject_cycle = inflight[p].front().inject_cycle;
                        int lat = SIM_CYCLES + dc - inject_cycle + 1;
                        sum_latency += lat;
                        if(lat > max_latency) max_latency = lat;
                        pkt_recv++;
                        std::cout << "DRAIN Out[" << p << "] "
                                  << "payload=" << payload
                                  << " latency=" << lat
                                  << " cycles" << std::endl;
                        inflight[p].pop();
                    }
                }
            }
        }
    }

    // ========== 3. 性能统计 ==========
    int total_sent = 0;
    for(int p=0;p<NUM_INPUTS;++p) total_sent += pkt_sent[p];

    std::cout << "\n=== Router Performance Report ===\n";
    std::cout << "Total cycles   : " << SIM_CYCLES << " + " << DRAIN_CYCLES << " (drain)\n";
    std::cout << "Packets sent   : " << total_sent << std::endl;
    std::cout << "Packets received: " << pkt_recv << std::endl;
    if(pkt_recv > 0) {
        std::cout << "Average latency: " << (double)sum_latency/pkt_recv << " cycles\n";
        std::cout << "Max latency    : " << max_latency << " cycles\n";
        std::cout << "Throughput     : " << (double)pkt_recv/(SIM_CYCLES+DRAIN_CYCLES) << " pkt/cycle\n";
    } else {
        std::cout << "No packet delivered!\n";
    }
    if(pkt_recv < total_sent) {
        std::cout << "Warning: " << (total_sent-pkt_recv) << " packets lost or stuck in FIFO!\n";
    }
    return 0;
}
*/
//---------------------------------------------------------------------------------------------------------------------

/*──────────────── tb_basic_v2_enable.cpp ───────────────
 * 功能   : 单Router端口使能测试（全enable，输入0->输出0，3-flit）
 * 作者   : ChatGPT – 2025-05-10
 std::cout << "main b_out[0] address: " << &router2_out[0] << std::endl;
 *------------------------------------------------*/

 /*
#include "top.h"
#include <iostream>
#include <cassert>

static AxiFlit make_flit(bool last, ap_uint<2> flit_type, ap_uint<2> dx = 0, ap_uint<2> dy = 0)
{
    AxiFlit f;
    f.data = 0;
    f.data.range(31, 30) = flit_type;
    f.data.range(29, 28) = dx;
    f.data.range(27, 26) = dy;
    f.last = last;
    return f;
}

int main()
{
    // ============ 分配所有流对象 ============
    hls::stream<AxiFlit> a_in[NUM_INPUTS], a_out[NUM_OUTPUTS];
    hls::stream<AxiFlit> b_in[NUM_INPUTS], b_out[NUM_OUTPUTS];

    // 每台router的局部FIFO也要在testbench里分配
    hls::stream<FlitInternal> a_route_fifo[NUM_INPUTS], a_out_buffer[NUM_OUTPUTS];
    hls::stream<FlitInternal> b_route_fifo[NUM_INPUTS], b_out_buffer[NUM_OUTPUTS];
    ArbState a_arb, b_arb;
    memset(&a_arb, 0, sizeof(ArbState));
    memset(&b_arb, 0, sizeof(ArbState));
    // ===== 端口使能 =====
    bool a_port_enable[NUM_INPUTS] = {true, false, false, true, false};  // (0,0) local+east
    bool b_port_enable[NUM_INPUTS] = {true, false, false, false, true};  // (1,0) local+west

    // ===== 路由配置 =====
    RouterCfg a_cfg, b_cfg;
    a_cfg.topo = TOPO_MESH; a_cfg.algo = ALGO_XY; a_cfg.cur_x = 0; a_cfg.cur_y = 0;
    b_cfg.topo = TOPO_MESH; b_cfg.algo = ALGO_XY; b_cfg.cur_x = 1; b_cfg.cur_y = 0;

    // ===== Router1 (0,0) 向 Router2 (1,0) 注入 3-flit packet =====
    a_in[0].write(make_flit(false, FLIT_HEAD, 1, 0));  // dest = (1,0)
    a_in[0].write(make_flit(false, FLIT_BODY));
    a_in[0].write(make_flit(true,  FLIT_TAIL));

    const int SIM_CYCLES = 100;
    int rx_cnt = 0;

    for (int t = 0; t < SIM_CYCLES; ++t)
    {
        // 两台router并行模拟（FIFO参数必须是自己的！）
        noc_router_top(a_in, a_out, a_cfg, a_port_enable, a_arb, a_route_fifo, a_out_buffer);
        noc_router_top(b_in, b_out, b_cfg, b_port_enable, b_arb, b_route_fifo, b_out_buffer);

        // mesh 1x2互连：Router1的east连Router2的west，Router2的west连Router1的east
        while (!a_out[3].empty()) b_in[4].write(a_out[3].read()); // east -> west

        // 读取Router2本地输出
        while (!b_out[0].empty()) {
            AxiFlit f = b_out[0].read();
            ap_uint<2> flit_type = f.data.range(31, 30);
            const char* type_str = (flit_type == FLIT_HEAD) ? "HEAD" :
                                   (flit_type == FLIT_BODY) ? "BODY" :
                                   (flit_type == FLIT_TAIL) ? "TAIL" : "UNKNOWN";
            std::cout << "Cycle " << t
                      << " | Router2 local out[0] received: 0x"
                      << std::hex << f.data.to_uint() << std::dec
                      << " (Type: " << type_str << ", TLAST: " << f.last << ")\n";
            ++rx_cnt;
        }
    }
    
    
    if (rx_cnt == 3)
        std::cout << "PASS: Flit delivered from Router1 to Router2 local port, flits=" << rx_cnt << std::endl;
    else
        std::cerr << "FAIL: No flit delivered to Router2, flits=" << rx_cnt << std::endl;

    return rx_cnt == 3 ? 0 : 1;
}
*/
//----------------------------------------------------------------------------------------------------------------------------
//backpresurre test3x3
/*
#include "top.h"
#include <iostream>
#include <cstring>

const int NOC_X = 3, NOC_Y = 3;
const int SIM_CYCLES = 100;
const int PKT_LEN = 20; // 大于FIFO_DEPTH以观测backpressure

// 构造flit
static AxiFlit make_flit(bool last, ap_uint<2> flit_type, ap_uint<2> dx = 0, ap_uint<2> dy = 0) {
    AxiFlit f;
    f.data = 20;
    f.data.range(31, 30) = flit_type;
    f.data.range(29, 28) = dx;
    f.data.range(27, 26) = dy;
    f.last = last;
    return f;
}

int main() {
    // 1. 全网router资源分配
    hls::stream<AxiFlit> in[NOC_X][NOC_Y][NUM_INPUTS], out[NOC_X][NOC_Y][NUM_OUTPUTS];
    hls::stream<FlitInternal> route_fifo[NOC_X][NOC_Y][NUM_INPUTS], out_buffer[NOC_X][NOC_Y][NUM_OUTPUTS];
    ArbState arb[NOC_X][NOC_Y];
    RouterCfg cfg[NOC_X][NOC_Y];
    bool port_enable[NOC_X][NOC_Y][NUM_INPUTS];

    // 2. 配置所有router
    for (int x = 0; x < NOC_X; ++x) {
        for (int y = 0; y < NOC_Y; ++y) {
            cfg[x][y].topo = TOPO_MESH;
            cfg[x][y].algo = ALGO_XY;
            cfg[x][y].cur_x = x;
            cfg[x][y].cur_y = y;
            memset(port_enable[x][y], 0, sizeof(port_enable[x][y]));
            port_enable[x][y][0] = true; // local
            if (y < NOC_Y-1) port_enable[x][y][1] = true; // north (y+1)
            if (y > 0)       port_enable[x][y][2] = true; // south (y-1)
            if (x < NOC_X-1) port_enable[x][y][3] = true; // east  (x+1)
            if (x > 0)       port_enable[x][y][4] = true; // west  (x-1)
            memset(&arb[x][y], 0, sizeof(ArbState));
        }
    }

    for (int i = 0; i < PKT_LEN; ++i) {
        if (i == 0)
            in[0][0][0].write(make_flit(false, FLIT_HEAD, 2, 2));
        else if (i == PKT_LEN - 1)
            in[0][0][0].write(make_flit(true, FLIT_TAIL, 2, 2));
        else
            in[0][0][0].write(make_flit(false, FLIT_BODY, 2, 2));
    }
    int src_blocked = 0, rx_cnt = 0;

    // 4. 主仿真循环
    for (int t = 0; t < SIM_CYCLES; ++t) {
        // 推进所有router
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y)
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]);

        // mesh互连
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y) {
                if (y < NOC_Y-1) while (!out[x][y][1].empty()) in[x][y+1][2].write(out[x][y][1].read());
                if (y > 0)       while (!out[x][y][2].empty()) in[x][y-1][1].write(out[x][y][2].read());
                if (x < NOC_X-1) while (!out[x][y][3].empty()) in[x+1][y][4].write(out[x][y][3].read());
                if (x > 0)       while (!out[x][y][4].empty()) in[x-1][y][3].write(out[x][y][4].read());
            }

        // 只在部分仿真周期“堵塞”out[2][2][0]，前半段不读，后半段恢复读
        if (t > 50) {
            while (!out[2][2][0].empty()) {
                AxiFlit f = out[2][2][0].read();
                ++rx_cnt;
                // ...打印输出
                ap_uint<2> flit_type = f.data.range(31, 30);
                const char* type_str = (flit_type == FLIT_HEAD) ? "HEAD" :
                                       (flit_type == FLIT_BODY) ? "BODY" :
                                       (flit_type == FLIT_TAIL) ? "TAIL" : "UNKNOWN";
                std::cout << "Cycle " << t << " | Router(2,2) local out[0] received: 0x"
                          << std::hex << f.data.to_uint() << std::dec
                          << " (Type: " << type_str << ", TLAST: " << f.last << ")\n";
            }
        }

        // 检查入口源端是否出现堵塞（若支持 .full() 可用，否则用流深度估算）
        // HLS stream 默认不会抛出full/阻塞，但RTL可用
        // if (in[0][0][0].full()) src_blocked++;
        // 或者用stream size观察(如果有支持)
    }

    // 5. drain收尾
    for (int t = 0; t < 20; ++t) {
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y)
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]);
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y) {
                if (y < NOC_Y-1) while (!out[x][y][1].empty()) in[x][y+1][2].write(out[x][y][1].read());
                if (y > 0)       while (!out[x][y][2].empty()) in[x][y-1][1].write(out[x][y][2].read());
                if (x < NOC_X-1) while (!out[x][y][3].empty()) in[x+1][y][4].write(out[x][y][3].read());
                if (x > 0)       while (!out[x][y][4].empty()) in[x-1][y][3].write(out[x][y][4].read());
            }
        while (!out[2][2][0].empty()) {
            AxiFlit f = out[2][2][0].read();
            ++rx_cnt;
            // ...打印输出
            ap_uint<2> flit_type = f.data.range(31, 30);
            const char* type_str = (flit_type == FLIT_HEAD) ? "HEAD" :
                                   (flit_type == FLIT_BODY) ? "BODY" :
                                   (flit_type == FLIT_TAIL) ? "TAIL" : "UNKNOWN";
            std::cout << "Drain phase | Router(2,2) local out[0] received: 0x"
                      << std::hex << f.data.to_uint() << std::dec
                      << " (Type: " << type_str << ", TLAST: " << f.last << ")\n";
        }
    }

    std::cout << "Total flits delivered: " << rx_cnt << std::endl;
    if (rx_cnt == PKT_LEN)
        std::cout << "PASS: All flits delivered to Router(2,2) local port." << std::endl;
    else
        std::cerr << "FAIL: Only " << rx_cnt << " flits delivered to Router(2,2)!" << std::endl;

    return rx_cnt == PKT_LEN ? 0 : 1;
}

*/
//------------------------------------------------------------------------------------------------------
//3x3noc mesh (0,0)go to (2,2)
/*
#include "top.h"
#include <iostream>
#include <cstring>

const int NOC_X = 3, NOC_Y = 3;
const int SIM_CYCLES = 60;

// 构造flit
static AxiFlit make_flit(bool last, ap_uint<2> flit_type, ap_uint<2> dx = 0, ap_uint<2> dy = 0) {
    AxiFlit f;
    f.data = 20;
    f.data.range(31, 30) = flit_type;
    f.data.range(29, 28) = dx;
    f.data.range(27, 26) = dy;
    f.last = last;
    return f;
}

int main() {
    // 1. 全网router资源分配
    hls::stream<AxiFlit> in[NOC_X][NOC_Y][NUM_INPUTS], out[NOC_X][NOC_Y][NUM_OUTPUTS];
    hls::stream<FlitInternal> route_fifo[NOC_X][NOC_Y][NUM_INPUTS], out_buffer[NOC_X][NOC_Y][NUM_OUTPUTS];
    ArbState arb[NOC_X][NOC_Y];
    RouterCfg cfg[NOC_X][NOC_Y];
    bool port_enable[NOC_X][NOC_Y][NUM_INPUTS];

    // 2. 配置所有router
    for (int x = 0; x < NOC_X; ++x) {
        for (int y = 0; y < NOC_Y; ++y) {
            cfg[x][y].topo = TOPO_MESH;
            cfg[x][y].algo = ALGO_XY;
            cfg[x][y].cur_x = x;
            cfg[x][y].cur_y = y;
            memset(port_enable[x][y], 0, sizeof(port_enable[x][y]));
            port_enable[x][y][0] = true; // local
            if (y < NOC_Y-1) port_enable[x][y][1] = true; // north (y+1)
            if (y > 0)       port_enable[x][y][2] = true; // south (y-1)
            if (x < NOC_X-1) port_enable[x][y][3] = true; // east  (x+1)
            if (x > 0)       port_enable[x][y][4] = true; // west  (x-1)
            memset(&arb[x][y], 0, sizeof(ArbState));
        }
    }

    // 3. (0,0)注入一个3-flit包，目标(2,2)
    in[0][0][0].write(make_flit(false, FLIT_HEAD, 2, 2));
    in[0][0][0].write(make_flit(false, FLIT_BODY));
    in[0][0][0].write(make_flit(true,  FLIT_TAIL));

    int rx_cnt = 0;

    // 4. 主仿真循环
    for (int t = 0; t < SIM_CYCLES; ++t) {
        // 推进所有router
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]
                );
            }
        }

        // mesh互连，方向严格对应坐标系
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                // north: y+1
                if (y < NOC_Y-1) while (!out[x][y][1].empty()) in[x][y+1][2].write(out[x][y][1].read());
                // south: y-1
                if (y > 0)       while (!out[x][y][2].empty()) in[x][y-1][1].write(out[x][y][2].read());
                // east:  x+1
                if (x < NOC_X-1) while (!out[x][y][3].empty()) in[x+1][y][4].write(out[x][y][3].read());
                // west:  x-1
                if (x > 0)       while (!out[x][y][4].empty()) in[x-1][y][3].write(out[x][y][4].read());
            }
        }

        // (2,2) router local output口读取flit并打印
        while (!out[2][2][0].empty()) {
            AxiFlit f = out[2][2][0].read();
            ap_uint<2> flit_type = f.data.range(31, 30);
            const char* type_str = (flit_type == FLIT_HEAD) ? "HEAD" :
                                   (flit_type == FLIT_BODY) ? "BODY" :
                                   (flit_type == FLIT_TAIL) ? "TAIL" : "UNKNOWN";
            std::cout << "Cycle " << t
                      << " | Router(2,2) local out[0] received: 0x"
                      << std::hex << f.data.to_uint() << std::dec
                      << " (Type: " << type_str << ", TLAST: " << f.last << ")\n";
            ++rx_cnt;
        }
    }

    // 5. drain扫尾
    for (int t = 0; t < 10; ++t) {
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]
                );
            }
        }
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                if (y < NOC_Y-1) while (!out[x][y][1].empty()) in[x][y+1][2].write(out[x][y][1].read());
                if (y > 0)       while (!out[x][y][2].empty()) in[x][y-1][1].write(out[x][y][2].read());
                if (x < NOC_X-1) while (!out[x][y][3].empty()) in[x+1][y][4].write(out[x][y][3].read());
                if (x > 0)       while (!out[x][y][4].empty()) in[x-1][y][3].write(out[x][y][4].read());
            }
        }
        while (!out[2][2][0].empty()) {
            AxiFlit f = out[2][2][0].read();
            ap_uint<2> flit_type = f.data.range(31, 30);
            const char* type_str = (flit_type == FLIT_HEAD) ? "HEAD" :
                                   (flit_type == FLIT_BODY) ? "BODY" :
                                   (flit_type == FLIT_TAIL) ? "TAIL" : "UNKNOWN";
            std::cout << "Drain phase | Router(2,2) local out[0] received: 0x"
                      << std::hex << f.data.to_uint() << std::dec
                      << " (Type: " << type_str << ", TLAST: " << f.last << ")\n";
            ++rx_cnt;
        }
    }

    if (rx_cnt == 3)
        std::cout << "PASS: All flits delivered to Router(2,2) local port, flits=" << rx_cnt << std::endl;
    else
        std::cerr << "FAIL: Only " << rx_cnt << " flits delivered to Router(2,2)!" << std::endl;

    return rx_cnt == 3 ? 0 : 1;
}
*/
//----------------------------------------------------------------------------------------------
//3x3 noc torus (0,0)go to (2,2)
/*
#include "top.h"
#include <iostream>
#include <cstring>

const int NOC_X = 3, NOC_Y = 3;
const int SIM_CYCLES = 60;

static AxiFlit make_flit(bool last, ap_uint<2> flit_type, ap_uint<2> dx = 0, ap_uint<2> dy = 0) {
    AxiFlit f;
    f.data = 0;
    f.data.range(31, 30) = flit_type;
    f.data.range(29, 28) = dx;
    f.data.range(27, 26) = dy;
    f.last = last;
    return f;
}

inline int wrap_x(int x) { return (x + NOC_X) % NOC_X; }
inline int wrap_y(int y) { return (y + NOC_Y) % NOC_Y; }

int main() {
    // 1. 全网router资源分配
    hls::stream<AxiFlit> in[NOC_X][NOC_Y][NUM_INPUTS], out[NOC_X][NOC_Y][NUM_OUTPUTS];
    hls::stream<FlitInternal> route_fifo[NOC_X][NOC_Y][NUM_INPUTS], out_buffer[NOC_X][NOC_Y][NUM_OUTPUTS];
    ArbState arb[NOC_X][NOC_Y];
    RouterCfg cfg[NOC_X][NOC_Y];
    bool port_enable[NOC_X][NOC_Y][NUM_INPUTS];

    // 2. 配置所有router（左下角为(0,0)，x右增，y上增，Torus环回）
    for (int x = 0; x < NOC_X; ++x) {
        for (int y = 0; y < NOC_Y; ++y) {
            cfg[x][y].topo = TOPO_TORUS; // 别忘了让路由算法支持torus
            cfg[x][y].algo = ALGO_XY;
            cfg[x][y].cur_x = x;
            cfg[x][y].cur_y = y;
            memset(port_enable[x][y], 0, sizeof(port_enable[x][y]));
            port_enable[x][y][0] = true; // local
            port_enable[x][y][1] = true; // north (y+1, wrap)
            port_enable[x][y][2] = true; // south (y-1, wrap)
            port_enable[x][y][3] = true; // east  (x+1, wrap)
            port_enable[x][y][4] = true; // west  (x-1, wrap)
            memset(&arb[x][y], 0, sizeof(ArbState));
        }
    }

    // 3. (0,0)注入一个3-flit包，目标(1,2)
    in[0][0][0].write(make_flit(false, FLIT_HEAD, 2, 2));
    in[0][0][0].write(make_flit(false, FLIT_BODY));
    in[0][0][0].write(make_flit(true,  FLIT_TAIL));

    int rx_cnt = 0;

    // 4. 主仿真循环
    for (int t = 0; t < SIM_CYCLES; ++t) {
        // 推进所有router
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]
                );
            }
        }

        // torus互连（所有边界 wrap）
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                // north: y+1, wrap
                int yn = wrap_y(y+1);
                while (!out[x][y][1].empty()) in[x][yn][2].write(out[x][y][1].read());
                // south: y-1, wrap
                int ys = wrap_y(y-1);
                while (!out[x][y][2].empty()) in[x][ys][1].write(out[x][y][2].read());
                // east:  x+1, wrap
                int xe = wrap_x(x+1);
                while (!out[x][y][3].empty()) in[xe][y][4].write(out[x][y][3].read());
                // west:  x-1, wrap
                int xw = wrap_x(x-1);
                while (!out[x][y][4].empty()) in[xw][y][3].write(out[x][y][4].read());
            }
        }

        // (1,2) router local output口读取flit并打印
        while (!out[2][2][0].empty()) {
            AxiFlit f = out[2][2][0].read();
            ap_uint<2> flit_type = f.data.range(31, 30);
            const char* type_str = (flit_type == FLIT_HEAD) ? "HEAD" :
                                   (flit_type == FLIT_BODY) ? "BODY" :
                                   (flit_type == FLIT_TAIL) ? "TAIL" : "UNKNOWN";
            std::cout << "Cycle " << t
                      << " | Router(2,2) local out[0] received: 0x"
                      << std::hex << f.data.to_uint() << std::dec
                      << " (Type: " << type_str << ", TLAST: " << f.last << ")\n";
            ++rx_cnt;
        }
    }

    // 5. drain扫尾
    for (int t = 0; t < 10; ++t) {
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]
                );
            }
        }
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                int yn = wrap_y(y+1);
                while (!out[x][y][1].empty()) in[x][yn][2].write(out[x][y][1].read());
                int ys = wrap_y(y-1);
                while (!out[x][y][2].empty()) in[x][ys][1].write(out[x][y][2].read());
                int xe = wrap_x(x+1);
                while (!out[x][y][3].empty()) in[xe][y][4].write(out[x][y][3].read());
                int xw = wrap_x(x-1);
                while (!out[x][y][4].empty()) in[xw][y][3].write(out[x][y][4].read());
            }
        }
        while (!out[2][2][0].empty()) {
            AxiFlit f = out[2][2][0].read();
            ap_uint<2> flit_type = f.data.range(31, 30);
            const char* type_str = (flit_type == FLIT_HEAD) ? "HEAD" :
                                   (flit_type == FLIT_BODY) ? "BODY" :
                                   (flit_type == FLIT_TAIL) ? "TAIL" : "UNKNOWN";
            std::cout << "Drain phase | Router(2,2) local out[0] received: 0x"
                      << std::hex << f.data.to_uint() << std::dec
                      << " (Type: " << type_str << ", TLAST: " << f.last << ")\n";
            ++rx_cnt;
        }
    }

    if (rx_cnt == 3)
        std::cout << "PASS: All flits delivered to Router(1,2) local port, flits=" << rx_cnt << std::endl;
    else
        std::cerr << "FAIL: Only " << rx_cnt << " flits delivered to Router(1,2)!" << std::endl;

    return rx_cnt == 3 ? 0 : 1;
}
*/
//------------------------------------------------------------------------------------------------------------------------------
/*
//简单测试单个router五个输入去往五个不同的输出端口
#include "top.h"
#include <iostream>
#include <cassert>

static AxiFlit make_flit(bool last, ap_uint<2> flit_type, ap_uint<2> dx = 0, ap_uint<2> dy = 0, uint32_t payload = 0)
{
    AxiFlit f;
    f.data = 0;
    f.data.range(31, 30) = flit_type;
    f.data.range(29, 28) = dx;
    f.data.range(27, 26) = dy;
    f.data.range(15, 0)  = payload; // 唯一payload
    f.last = last;
    return f;
}

int main()
{
    hls::stream<AxiFlit> in_stream [NUM_INPUTS];
    hls::stream<AxiFlit> out_stream[NUM_OUTPUTS];

    // 配置 router 在 mesh (1,1)
    RouterCfg cfg;
    cfg.topo = TOPO_MESH;
    cfg.algo = ALGO_XY;
    cfg.cur_x = 1;
    cfg.cur_y = 1;

    // 全部端口enable
    bool port_enable[NUM_INPUTS] = {1, 1, 1, 1, 1};

    // 目的坐标设定
    // 0:本地 1:东 2:南 3:西 4:北
    const ap_uint<2> DX[NUM_INPUTS] = {1, 2, 1, 0, 1}; // East, South, Local, West, North
    const ap_uint<2> DY[NUM_INPUTS] = {1, 1, 2, 1, 0};

    // 统计每个输出端口flit数量
    int rx_port_cnt[NUM_OUTPUTS] = {0};

    // 写入5个3-flit packet，每个唯一payload
    for (int i = 0; i < NUM_INPUTS; ++i) {
        in_stream[i].write(make_flit(false, FLIT_HEAD, DX[i], DY[i], 0x1000 | i));
        in_stream[i].write(make_flit(false, FLIT_BODY, 0, 0, 0x2000 | i));
        in_stream[i].write(make_flit(true , FLIT_TAIL, 0, 0, 0x3000 | i));
    }

    const int SIM_CYCLES = 15;
    int rx_cnt = 0;
    int cycles = 0;

    for (int t = 0; t < SIM_CYCLES; ++t)
    {
        noc_router_top(in_stream, out_stream, cfg, port_enable);

        for (int p = 0; p < NUM_OUTPUTS; ++p)
        {
            while (!out_stream[p].empty())
            {
                AxiFlit f = out_stream[p].read();
                ++rx_cnt;
                ++rx_port_cnt[p];
                ap_uint<2> flit_type = f.data.range(31, 30);
                uint32_t payload = f.data.range(15, 0);
                const char* type_str = (flit_type == FLIT_HEAD) ? "HEAD" :
                                       (flit_type == FLIT_BODY) ? "BODY" :
                                       (flit_type == FLIT_TAIL) ? "TAIL" : "UNKNOWN";
                std::cout << "Cycle " << cycles
                          << " | Output[" << p << "] received flit: 0x"
                          << std::hex << f.data.to_uint() << std::dec
                          << " (Type: " << type_str << ", TLAST: " << f.last
                          << ", Payload: 0x" << std::hex << payload << std::dec << ")\n";
                if (f.last)
                    assert(flit_type == FLIT_TAIL);
            }
        }
        ++cycles;
    }

    // 检查每个输出收到3个flit
    bool pass = true;
    for (int i = 0; i < NUM_OUTPUTS; ++i)
    {
        if (rx_port_cnt[i] != 3)
        {
            std::cerr << "FAILED: Output port " << i << " received " << rx_port_cnt[i] << " flits (expect 3)\n";
            pass = false;
        }
    }
    if (pass && rx_cnt == 15)
    {
        std::cout << "tb_basic_v3 PASSED. flits = " << rx_cnt << std::endl;
        return 0;
    }
    else
    {
        std::cerr << "tb_basic_v3 FAILED! total flits = " << rx_cnt << std::endl;
        return 1;
    }
}
*/
//随机注入数据包在3x3中，测试延迟和吞吐量
//--------------------------------------------------------------------------------------------------------------------------
/*
#include "top.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <iomanip>

// Mesh大小
const int NOC_X = 3, NOC_Y = 3;
const int SIM_CYCLES = 300;
const double INJECTION_RATE = 0.4; // 每端口每cycle注入新包的概率，建议多轮 sweep

// flit的payload加注入cycle戳
static AxiFlit make_flit(bool last, ap_uint<2> flit_type, ap_uint<2> dx, ap_uint<2> dy, ap_uint<24> payload) {
    AxiFlit f;
    f.data = 0;
    f.data.range(31, 30) = flit_type;
    f.data.range(29, 28) = dx;
    f.data.range(27, 26) = dy;
    f.data.range(25, 2)  = payload;   // payload部分（可存注入cycle），24位足够
    f.last = last;
    return f;
}

// 随机生成0~1的浮点数
inline double rand_01() { return rand() / double(RAND_MAX); }

// 随机选择一个不等于自身的节点
void random_dst(int x, int y, int& dst_x, int& dst_y) {
    do {
        dst_x = rand() % NOC_X;
        dst_y = rand() % NOC_Y;
    } while (dst_x == x && dst_y == y);
}

int main() {
    srand(time(0));
    // 1. 全网资源分配
    hls::stream<AxiFlit> in[NOC_X][NOC_Y][NUM_INPUTS], out[NOC_X][NOC_Y][NUM_OUTPUTS];
    hls::stream<FlitInternal> route_fifo[NOC_X][NOC_Y][NUM_INPUTS], out_buffer[NOC_X][NOC_Y][NUM_OUTPUTS];
    ArbState arb[NOC_X][NOC_Y];
    RouterCfg cfg[NOC_X][NOC_Y];
    bool port_enable[NOC_X][NOC_Y][NUM_INPUTS];

    // 2. 配置所有router
    for (int x = 0; x < NOC_X; ++x)
        for (int y = 0; y < NOC_Y; ++y) {
            cfg[x][y].topo = TOPO_MESH;
            cfg[x][y].algo = ALGO_XY;
            cfg[x][y].cur_x = x;
            cfg[x][y].cur_y = y;
            memset(port_enable[x][y], 0, sizeof(port_enable[x][y]));
            port_enable[x][y][0] = true; // local
            if (y < NOC_Y-1) port_enable[x][y][1] = true; // north
            if (y > 0)       port_enable[x][y][2] = true; // south
            if (x < NOC_X-1) port_enable[x][y][3] = true; // east
            if (x > 0)       port_enable[x][y][4] = true; // west
            memset(&arb[x][y], 0, sizeof(ArbState));
        }

    // 统计变量
    struct PacketStat { int inject_cycle, dst_x, dst_y; };
    std::vector<PacketStat> injected_packets; // 只保存tail的延迟统计
    std::vector<int> latency_list; // 记录所有packet的延迟

    int total_recv_flits = 0, total_recv_packets = 0;

    // 3. 主仿真循环
    for (int t = 0; t < SIM_CYCLES; ++t) {
        // 3.1 各终端按概率决定是否注入新packet
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y) {
                if (rand_01() < INJECTION_RATE) {
                    int dst_x, dst_y;
                    random_dst(x, y, dst_x, dst_y);
                    // 注入新packet（3个flit），payload里存注入cycle
                    in[x][y][0].write(make_flit(false, FLIT_HEAD, dst_x, dst_y, t));
                    in[x][y][0].write(make_flit(false, FLIT_BODY,  dst_x, dst_y, t));
                    in[x][y][0].write(make_flit(true,  FLIT_TAIL,  dst_x, dst_y, t));
                    injected_packets.push_back({t, dst_x, dst_y});
                }
            }

        // 3.2 推进所有router
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y)
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]
                );

        // 3.3 Mesh互连
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y) {
                if (y < NOC_Y-1) while (!out[x][y][1].empty()) in[x][y+1][2].write(out[x][y][1].read());
                if (y > 0)       while (!out[x][y][2].empty()) in[x][y-1][1].write(out[x][y][2].read());
                if (x < NOC_X-1) while (!out[x][y][3].empty()) in[x+1][y][4].write(out[x][y][3].read());
                if (x > 0)       while (!out[x][y][4].empty()) in[x-1][y][3].write(out[x][y][4].read());
            }

        // 3.4 检查所有终端local口的输出，统计tail flit延迟
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y)
                while (!out[x][y][0].empty()) {
                    AxiFlit f = out[x][y][0].read();
                    ++total_recv_flits;
                    ap_uint<2> flit_type = f.data.range(31, 30);
                    ap_uint<24> inject_cycle = f.data.range(25, 2);
                    if (flit_type == FLIT_TAIL) {
                        int latency = t - inject_cycle.to_uint();
                        latency_list.push_back(latency);
                        ++total_recv_packets;
                        std::cout << "Cycle " << t << " | Node(" << x << "," << y << ") received packet tail, latency = " << latency << std::endl;
                    }
                }
    }
    for (int t = 0; t < 1200; ++t) {
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
            // 推进每个router
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]
                );
        }
    }
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y) {
                if (y < NOC_Y-1) while (!out[x][y][1].empty()) in[x][y+1][2].write(out[x][y][1].read());
                if (y > 0)       while (!out[x][y][2].empty()) in[x][y-1][1].write(out[x][y][2].read());
                if (x < NOC_X-1) while (!out[x][y][3].empty()) in[x+1][y][4].write(out[x][y][3].read());
                if (x > 0)       while (!out[x][y][4].empty()) in[x-1][y][3].write(out[x][y][4].read());
            }
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                for (int p = 0; p < NUM_OUTPUTS; ++p) {
                    while (!out[x][y][p].empty()) {
                        out[x][y][p].read();
                }
            }
        }
    }

    }
    // 4. 结果统计与输出
    double avg_latency = 0;
    for (int v : latency_list) avg_latency += v;
    avg_latency /= latency_list.size();
    double throughput = 1.0 * total_recv_flits / SIM_CYCLES / (NOC_X * NOC_Y);

    std::cout << "\n[SUMMARY] Injection rate: " << INJECTION_RATE
              << ", Packets delivered: " << total_recv_packets
              << ", Avg latency (cycles): " << std::setprecision(3) << avg_latency
              << ", Throughput (flit/cycle/node): " << throughput << std::endl;

    return 0;
}
*/
//-----------------------------------------------------------------------------------------------------
/*
#include "top.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <iomanip>

// Mesh大小
const int NOC_X = 3, NOC_Y = 3;
const int SIM_CYCLES = 300;
const double INJECTION_RATE = 0.05; // 每端口每cycle注入新包的概率，建议多轮 sweep

// flit的payload加注入cycle戳
static AxiFlit make_flit(bool last, ap_uint<2> flit_type, ap_uint<2> dx, ap_uint<2> dy, ap_uint<24> payload) {
    AxiFlit f;
    f.data = 0;
    f.data.range(31, 30) = flit_type;
    f.data.range(29, 28) = dx;
    f.data.range(27, 26) = dy;
    f.data.range(25, 2)  = payload;   // payload部分（可存注入cycle），24位足够
    f.last = last;
    return f;
}

// 随机生成0~1的浮点数
inline double rand_01() { return rand() / double(RAND_MAX); }

// 随机选择一个不等于自身的节点
void random_dst(int x, int y, int& dst_x, int& dst_y) {
    do {
        dst_x = rand() % NOC_X;
        dst_y = rand() % NOC_Y;
    } while (dst_x == x && dst_y == y);
}
inline int wrap_x(int x) { return (x + NOC_X) % NOC_X; }
inline int wrap_y(int y) { return (y + NOC_Y) % NOC_Y; }

int main() {
    srand(time(0));
    // 1. 全网资源分配
    hls::stream<AxiFlit> in[NOC_X][NOC_Y][NUM_INPUTS], out[NOC_X][NOC_Y][NUM_OUTPUTS];
    hls::stream<FlitInternal> route_fifo[NOC_X][NOC_Y][NUM_INPUTS], out_buffer[NOC_X][NOC_Y][NUM_OUTPUTS];
    ArbState arb[NOC_X][NOC_Y];
    RouterCfg cfg[NOC_X][NOC_Y];
    bool port_enable[NOC_X][NOC_Y][NUM_INPUTS];

    // 2. 配置所有router
    for (int x = 0; x < NOC_X; ++x)
        for (int y = 0; y < NOC_Y; ++y) {
            cfg[x][y].topo = TOPO_TORUS;
            cfg[x][y].algo = ALGO_XY;
            cfg[x][y].cur_x = x;
            cfg[x][y].cur_y = y;
            memset(port_enable[x][y], 0, sizeof(port_enable[x][y]));
            port_enable[x][y][0] = true; // local
            // 北南东西全部使能（环回在互联阶段处理）
            port_enable[x][y][1] = true; // north
            port_enable[x][y][2] = true; // south
            port_enable[x][y][3] = true; // east
            port_enable[x][y][4] = true; // west
            memset(&arb[x][y], 0, sizeof(ArbState));
        }

    // 统计变量
    struct PacketStat { int inject_cycle, dst_x, dst_y; };
    std::vector<PacketStat> injected_packets; // 只保存tail的延迟统计
    std::vector<int> latency_list; // 记录所有packet的延迟

    int total_recv_flits = 0, total_recv_packets = 0;

    // 3. 主仿真循环
    for (int t = 0; t < SIM_CYCLES; ++t) {
        // 3.1 各终端按概率决定是否注入新packet
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y) {
                if (rand_01() < INJECTION_RATE) {
                    int dst_x, dst_y;
                    random_dst(x, y, dst_x, dst_y);
                    // 注入新packet（3个flit），payload里存注入cycle
                    in[x][y][0].write(make_flit(false, FLIT_HEAD, dst_x, dst_y, t));
                    in[x][y][0].write(make_flit(false, FLIT_BODY,  dst_x, dst_y, t));
                    in[x][y][0].write(make_flit(true,  FLIT_TAIL,  dst_x, dst_y, t));
                    injected_packets.push_back({t, dst_x, dst_y});
                }
            }

        // 3.2 推进所有router
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y)
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]
                );

        // 3.3 Mesh互连
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
            int yn = wrap_y(y + 1);
            while (!out[x][y][1].empty())
                in[x][yn][2].write(out[x][y][1].read()); // north -> south of wrapped node

            int ys = wrap_y(y - 1);
            while (!out[x][y][2].empty())
                in[x][ys][1].write(out[x][y][2].read()); // south -> north

            int xe = wrap_x(x + 1);
            while (!out[x][y][3].empty())
                in[xe][y][4].write(out[x][y][3].read()); // east -> west

            int xw = wrap_x(x - 1);
            while (!out[x][y][4].empty())
                in[xw][y][3].write(out[x][y][4].read()); // west -> east
            }
      }

        // 3.4 检查所有终端local口的输出，统计tail flit延迟
        for (int x = 0; x < NOC_X; ++x)
            for (int y = 0; y < NOC_Y; ++y)
                while (!out[x][y][0].empty()) {
                    AxiFlit f = out[x][y][0].read();
                    ++total_recv_flits;
                    ap_uint<2> flit_type = f.data.range(31, 30);
                    ap_uint<24> inject_cycle = f.data.range(25, 2);
                    if (flit_type == FLIT_TAIL) {
                        int latency = t - inject_cycle.to_uint();
                        latency_list.push_back(latency);
                        ++total_recv_packets;
                        std::cout << "Cycle " << t << " | Node(" << x << "," << y << ") received packet tail, latency = " << latency << std::endl;
                    }
                }
    }
    for (int t = 0; t < 1200; ++t) {
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
            // 推进每个router
                noc_router_top(
                    in[x][y], out[x][y], cfg[x][y], port_enable[x][y], arb[x][y],
                    route_fifo[x][y], out_buffer[x][y]
                );
        }
    }
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
            int yn = wrap_y(y + 1);
            while (!out[x][y][1].empty())
                in[x][yn][2].write(out[x][y][1].read()); // north -> south of wrapped node

            int ys = wrap_y(y - 1);
            while (!out[x][y][2].empty())
                in[x][ys][1].write(out[x][y][2].read()); // south -> north

            int xe = wrap_x(x + 1);
            while (!out[x][y][3].empty())
                in[xe][y][4].write(out[x][y][3].read()); // east -> west

            int xw = wrap_x(x - 1);
            while (!out[x][y][4].empty())
                in[xw][y][3].write(out[x][y][4].read()); // west -> east
            }
      }
        for (int x = 0; x < NOC_X; ++x) {
            for (int y = 0; y < NOC_Y; ++y) {
                for (int p = 0; p < NUM_OUTPUTS; ++p) {
                    while (!out[x][y][p].empty()) {
                        out[x][y][p].read();
                }
            }
        }
    }

    }
    // 4. 结果统计与输出
    double avg_latency = 0;
    for (int v : latency_list) avg_latency += v;
    avg_latency /= latency_list.size();
    double throughput = 1.0 * total_recv_flits / SIM_CYCLES / (NOC_X * NOC_Y);

    std::cout << "\n[SUMMARY] Injection rate: " << INJECTION_RATE
              << ", Packets delivered: " << total_recv_packets
              << ", Avg latency (cycles): " << std::setprecision(3) << avg_latency
              << ", Throughput (flit/cycle/node): " << throughput << std::endl;

    return 0;
}
*/