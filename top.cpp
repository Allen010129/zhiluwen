/*
#include "top.h"
#define FIFO_DEPTH 16
#include <iostream>

// 用于XY路由的简单示例 (3x3)



// ============ 全局定义 ============



// ------------------- 路由计算阶段 -------------------
// 从每个输入接口读取 flit，转换为内部格式；
// 如果 flit 为 head，则利用 XY 算法计算输出端口并更新 per‑input 状态 active_out；
// 非 head flit 则沿用上次计算结果；
// 将 flit（包含扩展字段 outPort）写入对应输入通道的 FIFO，确保 packet 内 flit 顺序保持不变。
void routing_stage(
    hls::stream<AxiFlit> in_stream[NUM_INPUTS],
    hls::stream<FlitInternal> route_fifo[NUM_INPUTS],
    RouterCfg cfg,
    const bool port_enable[NUM_INPUTS],
    ArbState &arb
) {
#pragma HLS PIPELINE II=1
#pragma HLS ARRAY_PARTITION variable=port_enable complete dim=1

    auto& active_out=arb.active_out;
//#pragma HLS RESET variable=active_out
#pragma HLS ARRAY_PARTITION variable=active_out complete dim=1 
    // 当前节点坐标 (0,0) 和拓扑类型（此处选择 Mesh）

    for (int i = 0; i < NUM_INPUTS; i++) {
    #pragma HLS UNROLL
        if (port_enable[i] && !in_stream[i].empty()) {
            AxiFlit axi_flit = in_stream[i].read();
            FlitInternal flit = axi_to_internal(axi_flit);
            ap_uint<2> flit_type = get_flit_type(flit.data);
                  
            //std::cout << "[ROUTER (" << (int)cfg.cur_x << "," << (int)cfg.cur_y << ")] "
                  //<< "in[" << i << "] got flit, type=" << (int)flit_type;
        

            // 如果是 head flit，计算路由并更新 active_out
            if (flit_type == FLIT_HEAD) {
                ap_uint<2> dest_x = get_dest_x(flit.data);
                ap_uint<2> dest_y = get_dest_y(flit.data);
                Topology topo = static_cast<Topology>( cfg.topo.to_uint() );
                Algo     algo = static_cast<Algo>(     cfg.algo.to_uint() );
                active_out[i] = route_compute(cfg.cur_x, cfg.cur_y, dest_x, dest_y,topo, algo);

                //std::cout << " (HEAD, dest=(" << (int)dest_x << "," << (int)dest_y
                      //<< "), will go to out[" << (int)active_out[i] << "])" << std::endl;

            }
                
            
            // 将计算结果存入扩展字段
            flit.outPort = active_out[i];
            // 写入对应输入通道的 FIFO，保证同一 packet 的 flit 顺序不乱
            route_fifo[i].write(flit);
        }
    }
}

// ───── 本地工具: 5‑bit 优先编码器 (find first‑1) ────────────────────────────
static inline unsigned first_one(ap_uint<NUM_INPUTS> v) {
#pragma HLS INLINE
    // NUM_INPUTS ≤ 5 ≤ 32 → 可用内建 ctz
    return v ? __builtin_ctz((unsigned)v) : NUM_INPUTS; // 若无1则返回5
}
// ------------------- 仲裁阶段 -------------------


void arbitration_stage(
    hls::stream<FlitInternal> route_fifo[NUM_INPUTS],
    hls::stream<FlitInternal> out_buffer[NUM_OUTPUTS],
    ArbState &arb)
{
#pragma HLS PIPELINE II=1

// ---------- Persistent state ---------- 
auto& headBuf = arb.headBuf;
    auto& reqVec_cur = arb.reqVec_cur;
    auto& reqVec_nxt = arb.reqVec_nxt;
    auto& rr_ptr_cur = arb.rr_ptr_cur;
    auto& rr_ptr_nxt = arb.rr_ptr_nxt;
    auto& active_cur = arb.active_cur;
    auto& active_nxt = arb.active_nxt;
    auto& phase = arb.phase;
    auto& init_done = arb.init_done;

#pragma HLS ARRAY_PARTITION variable=headBuf     complete dim=1
#pragma HLS ARRAY_PARTITION variable=reqVec_cur    complete dim=1  // ★ MOD
#pragma HLS ARRAY_PARTITION variable=reqVec_nxt    complete dim=1  // ★ MOD

#pragma HLS ARRAY_PARTITION variable=rr_ptr_cur    complete dim=1  // ★ MOD
#pragma HLS ARRAY_PARTITION variable=rr_ptr_nxt    complete dim=1  // ★ MOD

#pragma HLS ARRAY_PARTITION variable=active_cur    complete dim=1  // ★ MOD
#pragma HLS ARRAY_PARTITION variable=active_nxt    complete dim=1  // ★ MOD

//#pragma HLS RESET variable=headBuf  
//#pragma HLS RESET variable=reqVec_cur  
//#pragma HLS RESET variable=reqVec_nxt  
//#pragma HLS RESET variable=rr_ptr_cur  
//#pragma HLS RESET variable=rr_ptr_nxt  
//#pragma HLS RESET variable=active_cur  
//#pragma HLS RESET variable=active_nxt  
//#pragma HLS RESET variable=init_done  

#pragma HLS DEPENDENCE variable=rr_ptr_nxt inter false distance=2  // ★ MOD
#pragma HLS DEPENDENCE variable=active_nxt inter false distance=2  // ★ MOD
#pragma HLS DEPENDENCE variable=reqVec_cur inter false distance=2
#pragma HLS DEPENDENCE variable=reqVec_nxt inter false distance=2
#pragma HLS DEPENDENCE variable=headBuf inter false distance=2
// ---------- One-time init ---------- 
    if (!init_done) {
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
#pragma HLS UNROLL
            rr_ptr_cur[o] = rr_ptr_nxt[o] = 0;
            reqVec_cur[o] = reqVec_nxt[o] = 0;
            active_cur[o] = active_nxt[o] = -1;
        }
        for (int i = 0; i < NUM_INPUTS; ++i) {
#pragma HLS UNROLL
            headBuf[i].valid = false;
        }
        init_done = true;
        return;
    }

// ---------- Phase FSM ---------- 
//#pragma HLS RESET variable=phase  

//──────── Phase-0 : 收集请求 ────────
    if (!phase) {
        // 翻页：cur ← nxt  (距 = 2 由 pragma 保证) 
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
#pragma HLS UNROLL
            rr_ptr_cur[o]  = rr_ptr_nxt[o];
            active_cur[o]  = active_nxt[o];
            reqVec_cur[o]  = reqVec_nxt[o];
        }
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
        #pragma HLS UNROLL
            reqVec_nxt[o] = 0;
        }     
            #pragma HLS latency min=1 max=1
   
        // 更新 headBuf & 构建 new reqVec_nxt 
        for (int i = 0; i < NUM_INPUTS; ++i) {
#pragma HLS UNROLL
            if (!headBuf[i].valid && !route_fifo[i].empty()) {
                headBuf[i].flit  = route_fifo[i].read();
                headBuf[i].valid = true;
            }
            if (headBuf[i].valid) {
                unsigned dest = headBuf[i].flit.outPort.to_uint();
                //reqVec_nxt[dest] |= (ap_uint<NUM_INPUTS>(1) << i);
                if (dest == 0) reqVec_nxt[0] |= (ap_uint<NUM_INPUTS>(1) << i);
                else if (dest == 1) reqVec_nxt[1] |= (ap_uint<NUM_INPUTS>(1) << i);
                else if (dest == 2) reqVec_nxt[2] |= (ap_uint<NUM_INPUTS>(1) << i);
                else if (dest == 3) reqVec_nxt[3] |= (ap_uint<NUM_INPUTS>(1) << i);
                else                reqVec_nxt[4] |= (ap_uint<NUM_INPUTS>(1) << i);
            }
        }
        
        
        // 打印 reqVec_nxt
       
    }


//──────── Phase-1 : 仲裁 + 发送 ──────
    else {
        // A)  Round-Robin 仲裁 (读 cur，写 nxt) 
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
#pragma HLS UNROLL
            int a = active_cur[o];
            ap_uint<3> r_next = rr_ptr_cur[o];
            if (a == -1 && reqVec_cur[o] != 0) {
                unsigned base = rr_ptr_cur[o];
                ap_uint<NUM_INPUTS> rot =
                    (reqVec_cur[o] >> base) | (reqVec_cur[o] << (NUM_INPUTS - base));
                unsigned rel = first_one(rot);
                if (rel < NUM_INPUTS) {
                    unsigned sel = (base + rel) % NUM_INPUTS;
                    a      = sel;
                    r_next = (sel + 1) % NUM_INPUTS;
                }
            }
            active_nxt[o] = a;        // ★ MOD : 写 nxt
            rr_ptr_nxt[o] = r_next;   // ★ MOD
        }

        // B)  发送 flit 
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
#pragma HLS UNROLL
            int i = active_cur[o];
            if (i != -1 && headBuf[i].valid) {
                FlitInternal f = headBuf[i].flit;
                out_buffer[o].write(f);
                headBuf[i].valid = false;
                reqVec_nxt[o] &= ~(ap_uint<NUM_INPUTS>(1) << i);  // 清下一帧请求
                if (get_flit_type(f.data) == FLIT_TAIL)
                    active_nxt[o] = -1;
            }
        }
    }        

    phase = !phase;
}



// ------------------- 输出阶段 -------------------
// 从每个输出缓冲中读取 flit，转换为 AXI4‑Stream 格式后送出。
void output_stage(
    hls::stream<FlitInternal> out_buffer[NUM_OUTPUTS],
    hls::stream<AxiFlit> out_stream[NUM_OUTPUTS],
    const bool port_enable[NUM_OUTPUTS]
) {
#pragma HLS PIPELINE II=1
#pragma HLS ARRAY_PARTITION variable=port_enable complete dim=1

    for (int o = 0; o < NUM_OUTPUTS; o++) {
    #pragma HLS UNROLL
        if (port_enable[o] && !out_buffer[o].empty()) {
            FlitInternal flit = out_buffer[o].read();
            // 直接写出，不检查 out_stream 是否满
            //std::cout << "[OUTPUT_STAGE] output " << o << " write flit type = "
             //    << get_flit_type(flit.data)
              //    << " dest = (" << (int)get_dest_x(flit.data) << "," << (int)get_dest_y(flit.data) << ")\n";
            //std::cout << "output_stage out_stream[0] address: " << &out_stream[0] << std::endl;
            out_stream[o].write(internal_to_axi(flit));
        }
    }
}

//-------------------------------------------------------------------------------------------------------

//C simulation top function

void noc_router_top(
    hls::stream<AxiFlit> in_stream[NUM_INPUTS],
    hls::stream<AxiFlit> out_stream[NUM_OUTPUTS],
    RouterCfg           &cfg,
    const bool port_enable[NUM_INPUTS],
    ArbState            &arb_state,
    hls::stream<FlitInternal> route_fifo[NUM_INPUTS],
    hls::stream<FlitInternal> out_buffer[NUM_OUTPUTS]
) {
    #pragma HLS DATAFLOW
    #pragma HLS INTERFACE axis      port=in_stream
    #pragma HLS INTERFACE axis      port=out_stream
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    #pragma HLS INTERFACE s_axilite port=cfg    bundle=control
    // 参数FIFO：适配仿真和多router mesh
    #pragma HLS STREAM variable=route_fifo   depth=FIFO_DEPTH 
    #pragma HLS ARRAY_PARTITION variable=route_fifo   complete dim=1
    #pragma HLS STREAM variable=out_buffer   depth=FIFO_DEPTH
    #pragma HLS ARRAY_PARTITION variable=out_buffer   complete dim=1
    routing_stage   (in_stream, route_fifo, cfg, port_enable, arb_state);
    arbitration_stage(route_fifo, out_buffer, arb_state);
    output_stage     (out_buffer, out_stream, port_enable);
}
*/
//---------------------------------------------------------------------------------------------------------

#include "top.h"
#define FIFO_DEPTH 16
#include <iostream>

// 用于XY路由的简单示例 (3x3)



// ============ 全局定义 ============



// ------------------- 路由计算阶段 -------------------
// 从每个输入接口读取 flit，转换为内部格式；
// 如果 flit 为 head，则利用 XY 算法计算输出端口并更新 per‑input 状态 active_out；
// 非 head flit 则沿用上次计算结果；
// 将 flit（包含扩展字段 outPort）写入对应输入通道的 FIFO，确保 packet 内 flit 顺序保持不变。
void routing_stage(
    hls::stream<AxiFlit> in_stream[NUM_INPUTS],
    hls::stream<FlitInternal> route_fifo[NUM_INPUTS],
    RouterCfg cfg,
    const bool port_enable[NUM_INPUTS]
) {
#pragma HLS PIPELINE II=1
#pragma HLS ARRAY_PARTITION variable=port_enable complete dim=1

    static ap_uint<3> active_out[NUM_INPUTS] = {0,0,0,0,0};
#pragma HLS RESET variable=active_out

    // 当前节点坐标 (0,0) 和拓扑类型（此处选择 Mesh）

    for (int i = 0; i < NUM_INPUTS; i++) {
    #pragma HLS UNROLL
        if (port_enable[i] && !in_stream[i].empty()) {
            AxiFlit axi_flit = in_stream[i].read();
            FlitInternal flit = axi_to_internal(axi_flit);
            ap_uint<2> flit_type = get_flit_type(flit.data);
            // 如果是 head flit，计算路由并更新 active_out
            if (flit_type == FLIT_HEAD) {
                ap_uint<2> dest_x = get_dest_x(flit.data);
                ap_uint<2> dest_y = get_dest_y(flit.data);
                Topology topo = static_cast<Topology>( cfg.topo.to_uint() );
                Algo     algo = static_cast<Algo>(     cfg.algo.to_uint() );
                active_out[i] = route_compute(
                    cfg.cur_x, cfg.cur_y, dest_x, dest_y,
                    topo, algo);
            }
            // 将计算结果存入扩展字段
            flit.outPort = active_out[i];
            // 写入对应输入通道的 FIFO，保证同一 packet 的 flit 顺序不乱
            route_fifo[i].write(flit);
        }
    }
}

// ───── 本地工具: 5‑bit 优先编码器 (find first‑1) ────────────────────────────
static inline unsigned first_one(ap_uint<NUM_INPUTS> v) {
#pragma HLS INLINE
    // NUM_INPUTS ≤ 5 ≤ 32 → 可用内建 ctz
    return v ? __builtin_ctz((unsigned)v) : NUM_INPUTS; // 若无1则返回5
}
// ------------------- 仲裁阶段 -------------------

void arbitration_stage(
    hls::stream<FlitInternal> route_fifo[NUM_INPUTS],
    hls::stream<FlitInternal> out_buffer[NUM_OUTPUTS])
{
#pragma HLS PIPELINE II=1


    static HeadBuf_t          headBuf     [NUM_INPUTS];
    static ap_uint<NUM_INPUTS> reqVec_cur [NUM_OUTPUTS];
    static ap_uint<NUM_INPUTS> reqVec_nxt [NUM_OUTPUTS];
    static ap_uint<3>         rr_ptr_cur  [NUM_OUTPUTS];
    static ap_uint<3>         rr_ptr_nxt  [NUM_OUTPUTS];
    static int                active_cur [NUM_OUTPUTS];
    static int                active_nxt [NUM_OUTPUTS];
    static bool               init_done = false;

#pragma HLS ARRAY_PARTITION variable=headBuf     complete dim=1
#pragma HLS ARRAY_PARTITION variable=reqVec_cur    complete dim=1  // ★ MOD
#pragma HLS ARRAY_PARTITION variable=reqVec_nxt    complete dim=1  // ★ MOD

#pragma HLS ARRAY_PARTITION variable=rr_ptr_cur    complete dim=1  // ★ MOD
#pragma HLS ARRAY_PARTITION variable=rr_ptr_nxt    complete dim=1  // ★ MOD

#pragma HLS ARRAY_PARTITION variable=active_cur    complete dim=1  // ★ MOD
#pragma HLS ARRAY_PARTITION variable=active_nxt    complete dim=1  // ★ MOD

#pragma HLS RESET variable=headBuf  
#pragma HLS RESET variable=reqVec_cur  
#pragma HLS RESET variable=reqVec_nxt  
#pragma HLS RESET variable=rr_ptr_cur  
#pragma HLS RESET variable=rr_ptr_nxt  
#pragma HLS RESET variable=active_cur  
#pragma HLS RESET variable=active_nxt  
#pragma HLS RESET variable=init_done  

#pragma HLS DEPENDENCE variable=rr_ptr_nxt inter false distance=2  // ★ MOD
#pragma HLS DEPENDENCE variable=active_nxt inter false distance=2  // ★ MOD
#pragma HLS DEPENDENCE variable=reqVec_cur inter false distance=2
#pragma HLS DEPENDENCE variable=reqVec_nxt inter false distance=2
#pragma HLS DEPENDENCE variable=headBuf inter false distance=2

    if (!init_done) {
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
#pragma HLS UNROLL
            rr_ptr_cur[o] = rr_ptr_nxt[o] = 0;
            reqVec_cur[o] = reqVec_nxt[o] = 0;
            active_cur[o] = active_nxt[o] = -1;
        }
        for (int i = 0; i < NUM_INPUTS; ++i) {
#pragma HLS UNROLL
            headBuf[i].valid = false;
        }
        init_done = true;
        return;
    }

//---------- Phase FSM ---------- 
    static bool phase = 0;   // 0 = build, 1 = grant+send
#pragma HLS RESET variable=phase  

//──────── Phase-0 : 收集请求 ────────
    if (!phase) {
        // 翻页：cur ← nxt  (距 = 2 由 pragma 保证) 
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
#pragma HLS UNROLL
            rr_ptr_cur[o]  = rr_ptr_nxt[o];
            active_cur[o]  = active_nxt[o];
            reqVec_cur[o]  = reqVec_nxt[o];
        }
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
        #pragma HLS UNROLL
            reqVec_nxt[o] = 0;
        }  
        #pragma HLS latency min=1 max=1      
        // 更新 headBuf & 构建 new reqVec_nxt 
        for (int i = 0; i < NUM_INPUTS; ++i) {
#pragma HLS UNROLL
            if (!headBuf[i].valid && !route_fifo[i].empty()) {
                headBuf[i].flit  = route_fifo[i].read();
                headBuf[i].valid = true;
            }
            if (headBuf[i].valid) {
                unsigned dest = headBuf[i].flit.outPort.to_uint();
                //reqVec_nxt[dest] |= (ap_uint<NUM_INPUTS>(1) << i);
                if (dest == 0) reqVec_nxt[0] |= (ap_uint<NUM_INPUTS>(1) << i);
                else if (dest == 1) reqVec_nxt[1] |= (ap_uint<NUM_INPUTS>(1) << i);
                else if (dest == 2) reqVec_nxt[2] |= (ap_uint<NUM_INPUTS>(1) << i);
                else if (dest == 3) reqVec_nxt[3] |= (ap_uint<NUM_INPUTS>(1) << i);
                else                reqVec_nxt[4] |= (ap_uint<NUM_INPUTS>(1) << i);
            }
        }
        
        
        // 打印 reqVec_nxt
       
    }


//──────── Phase-1 : 仲裁 + 发送 ──────
    else {
        // A)  Round-Robin 仲裁 (读 cur，写 nxt) 
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
#pragma HLS UNROLL
            int a = active_cur[o];
            ap_uint<3> r_next = rr_ptr_cur[o];
            if (a == -1 && reqVec_cur[o] != 0) {
                unsigned base = rr_ptr_cur[o];
                ap_uint<NUM_INPUTS> rot =
                    (reqVec_cur[o] >> base) | (reqVec_cur[o] << (NUM_INPUTS - base));
                unsigned rel = first_one(rot);
                if (rel < NUM_INPUTS) {
                    unsigned sel = (base + rel) % NUM_INPUTS;
                    a      = sel;
                    r_next = (sel + 1) % NUM_INPUTS;
                }
            }
            active_nxt[o] = a;        // ★ MOD : 写 nxt
            rr_ptr_nxt[o] = r_next;   // ★ MOD
        }

        // B)  发送 flit 
        for (int o = 0; o < NUM_OUTPUTS; ++o) {
#pragma HLS UNROLL
            int i = active_cur[o];
            if (i != -1 && headBuf[i].valid) {
                FlitInternal f = headBuf[i].flit;
                out_buffer[o].write(f);
                headBuf[i].valid = false;
                reqVec_nxt[o] &= ~(ap_uint<NUM_INPUTS>(1) << i);  // 清下一帧请求
                if (get_flit_type(f.data) == FLIT_TAIL)
                    active_nxt[o] = -1;
            }
        }
    }        

    phase = !phase;
}



// ------------------- 输出阶段 -------------------
// 从每个输出缓冲中读取 flit，转换为 AXI4‑Stream 格式后送出。
void output_stage(
    hls::stream<FlitInternal> out_buffer[NUM_OUTPUTS],
    hls::stream<AxiFlit> out_stream[NUM_OUTPUTS],
    const bool port_enable[NUM_OUTPUTS]
) {
#pragma HLS PIPELINE II=1
#pragma HLS ARRAY_PARTITION variable=port_enable complete dim=1

    for (int o = 0; o < NUM_OUTPUTS; o++) {
    #pragma HLS UNROLL
        if (port_enable[o] && !out_buffer[o].empty()) {
            FlitInternal flit = out_buffer[o].read();
            // 直接写出，不检查 out_stream 是否满
            out_stream[o].write(internal_to_axi(flit));
        }
    }
}


// ------------------- 顶层函数 -------------------
// 使用 DATAFLOW 架构依次调用路由、仲裁、输出三个阶段。
void noc_router_body(
    hls::stream<AxiFlit>       in_stream[NUM_INPUTS],
    hls::stream<FlitInternal>  route_fifo_arg[NUM_INPUTS],
    RouterCfg                 &cfg,
    const bool port_enable[NUM_INPUTS],
    hls::stream<FlitInternal>  out_buffer_arg[NUM_OUTPUTS],
    hls::stream<AxiFlit>       out_stream[NUM_OUTPUTS]
) {
    #pragma HLS DATAFLOW
    routing_stage   (in_stream,        route_fifo_arg, cfg,port_enable);
    arbitration_stage(route_fifo_arg,  out_buffer_arg);
    output_stage     (out_buffer_arg,  out_stream,port_enable);
}

// —————————— Top-Level Wrapper ——————————
void noc_router_top(
    hls::stream<AxiFlit> in_stream[NUM_INPUTS],
    hls::stream<AxiFlit> out_stream[NUM_OUTPUTS],
    RouterCfg           &cfg,
    const bool port_enable[NUM_INPUTS]
) {
    #pragma HLS INTERFACE axis      port=in_stream
    #pragma HLS INTERFACE axis      port=out_stream
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    #pragma HLS INTERFACE s_axilite port=cfg    bundle=control
static hls::stream<FlitInternal> route_fifo[NUM_INPUTS];
static hls::stream<FlitInternal> out_buffer[NUM_OUTPUTS];
    // 在 Wrapper 中配置全局 FIFO 的属性
    #pragma HLS STREAM          variable=route_fifo   depth=FIFO_DEPTH 
    #pragma HLS ARRAY_PARTITION variable=route_fifo   complete dim=1
    #pragma HLS STREAM          variable=out_buffer    depth=FIFO_DEPTH
    #pragma HLS ARRAY_PARTITION variable=out_buffer    complete dim=1

    // 调用真正做 DATAFLOW 的子函数
    noc_router_body(in_stream, route_fifo, cfg,port_enable, out_buffer, out_stream);
}
