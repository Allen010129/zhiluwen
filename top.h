/*
#ifndef NOCRouter_H
#define NOCRouter_H

#include <ap_int.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// ------------------- FLIT 类型宏定义 -------------------
#define FLIT_HEAD 0x0
#define FLIT_BODY 0x1
#define FLIT_TAIL 0x2
// 常量定义
const int NUM_INPUTS = 5;
const int NUM_OUTPUTS = 5;
const int NOC_SIZE    = 3;   // 3×3
// 拓扑类型
enum Topology { TOPO_MESH = 0, TOPO_TORUS = 1 };
enum Algo     { ALGO_XY   = 0, ALGO_WEST_FIRST = 1 };
// ------------------- AXI4-Stream 数据结构 -------------------
// 使用 32 位数据宽度 (TDATA)，其余 side-channel 先不使用 (TKEEP 等)
typedef  ap_axiu<32, 0, 0, 0, AXIS_DISABLE_ALL | AXIS_ENABLE_DATA | AXIS_ENABLE_LAST> AxiFlit;  
// 其中 AxiFlit会包含：.data(32bit), .last(bool), .user, .id, .dest 等字段
// 在 Vivado HLS 中，可以用 flit.last = true/false 来控制 AXI4-Stream 的 TLAST 信号

struct FlitInternal {
    ap_uint<32> data;
    bool  last;
    ap_uint<3>  outPort;  // 用于保存路由计算得到的输出端口

};

// 将 AxiFlit 转换为内部数据结构
static FlitInternal axi_to_internal(AxiFlit axi) {
    FlitInternal internal;
    internal.data = axi.data;
    internal.last = axi.last;
    internal.outPort = 0;
    return internal;
}

// 将内部数据结构转换为 AxiFlit
static AxiFlit internal_to_axi(FlitInternal internal) {
    AxiFlit axi;
    axi.data = internal.data;
    axi.last = internal.last;
    return axi;
}
// ------------------- 获取 flit 类型的函数 -------------------
static ap_uint<2> get_flit_type(ap_uint<32> data) {
    // 假设高2位(31:30)表示 flit 类型
    return data.range(31, 30);
}
// 用于暂存从 in_stream 读出的 flit 等相关信息
struct HeadBuf_t {
    bool valid;         // 是否保存了一个flit
    FlitInternal flit;// 暂存flit本身
};
// ---------- 运行时配置寄存器 ---------- 
struct RouterCfg {
    ap_uint<2>  topo;     // 0-Mesh, 1-Torus
    ap_uint<2>  algo;     // 0-XY, 1-West-First
    ap_uint<2>  cur_x;
    ap_uint<2>  cur_y;
    
};

// ------------------- 获取目标坐标的函数 -------------------
// 假设在 data[29:28] 存 X 坐标, [27:26] 存 Y 坐标，仅示例
static ap_uint<2> get_dest_x(ap_uint<32> data) {
    return data.range(29, 28);
}
static ap_uint<2> get_dest_y(ap_uint<32> data) {
    return data.range(27, 26);
}

//xy算法
static ap_uint<3> xy_route(
    ap_uint<2> cur_x,
    ap_uint<2> cur_y,
    ap_uint<2> dest_x,
    ap_uint<2> dest_y,
    Topology topo
) {
#pragma HLS INLINE
    // 如果目标即本节点, 返回0(LOCAL) 做示例
    if (cur_x == dest_x && cur_y == dest_y) {
        return 0; // local
    }

    // Mesh拓扑
    if (topo == TOPO_MESH) {
        // 先在X方向移动
        if (dest_x > cur_x) return 3; // East
        if (dest_x < cur_x) return 4; // West
        // X相等时再看Y
        if (dest_y > cur_y) return 1; // North
        else return 2;               // South
    }
    else { 
        // TOPO_TORUS 示例, 需考虑环绕(仅示意)
        ap_int<3> dx = (ap_int<3>)dest_x - (ap_int<3>)cur_x;
        ap_int<3> dy = (ap_int<3>)dest_y - (ap_int<3>)cur_y;
        // 处理环绕距离
        if (dx >  1) dx -= NOC_SIZE;
        if (dx < -1) dx += NOC_SIZE;
        if (dy >  1) dy -= NOC_SIZE;
        if (dy < -1) dy += NOC_SIZE;
        // 先修正X
        if (dx > 0) return 3; // East
        if (dx < 0) return 4; // West
        // 再修正Y
        if (dy > 0) return 1; // North
        return 2; // South
    }
}

//west_first 算法
static ap_uint<3> west_first_route(
    ap_uint<2> cur_x, ap_uint<2> cur_y,
    ap_uint<2> dest_x,ap_uint<2> dest_y,
    Topology topo)
{
#pragma HLS INLINE
    if(topo==TOPO_TORUS)   // 简化：Torus 下退化为 XY
        return xy_route(cur_x,cur_y,dest_x,dest_y,topo);

    if(dest_x < cur_x)           // 必须先向西
        return 4;
    else
        return xy_route(cur_x,cur_y,dest_x,dest_y,topo);
}

static ap_uint<3> route_compute(
    ap_uint<2> cur_x, ap_uint<2> cur_y,
    ap_uint<2> dest_x,ap_uint<2> dest_y,
    Topology topo, Algo algo)
{
#pragma HLS INLINE
    return (algo==ALGO_WEST_FIRST) ?
        west_first_route(cur_x,cur_y,dest_x,dest_y,topo) :
        xy_route      (cur_x,cur_y,dest_x,dest_y,topo);
}
struct ArbState {
    HeadBuf_t headBuf[NUM_INPUTS];
    ap_uint<NUM_INPUTS> reqVec_cur[NUM_OUTPUTS], reqVec_nxt[NUM_OUTPUTS];
    ap_uint<3> rr_ptr_cur[NUM_OUTPUTS], rr_ptr_nxt[NUM_OUTPUTS];
    int active_cur[NUM_OUTPUTS], active_nxt[NUM_OUTPUTS];
    bool phase;
    bool init_done;
    ap_uint<3> active_out[NUM_INPUTS];
};

// 仿真模式：参数FIFO

void noc_router_top(
    hls::stream<AxiFlit> in_stream[NUM_INPUTS],
    hls::stream<AxiFlit> out_stream[NUM_OUTPUTS],
    RouterCfg           &cfg,
    const bool port_enable[NUM_INPUTS],
    ArbState            &arb_state,
    hls::stream<FlitInternal> route_fifo[NUM_INPUTS],
    hls::stream<FlitInternal> out_buffer[NUM_OUTPUTS]
);

#endif // NOCRouter_H

*/

//----------------------------------------------------------------------------------------------------------------------------------

#ifndef NOCRouter_H
#define NOCRouter_H

#include <ap_int.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// ------------------- FLIT 类型宏定义 -------------------
#define FLIT_HEAD 0x0
#define FLIT_BODY 0x1
#define FLIT_TAIL 0x2
// 常量定义
const int NUM_INPUTS = 5;
const int NUM_OUTPUTS = 5;
const int NOC_SIZE    = 3;   // 3×3
// 拓扑类型
enum Topology { TOPO_MESH = 0, TOPO_TORUS = 1 };
enum Algo     { ALGO_XY   = 0, ALGO_WEST_FIRST = 1 };
// ------------------- AXI4-Stream 数据结构 -------------------
// 使用 32 位数据宽度 (TDATA)，其余 side-channel 先不使用 (TKEEP 等)
typedef  ap_axiu<32, 0, 0, 0, AXIS_DISABLE_ALL | AXIS_ENABLE_DATA | AXIS_ENABLE_LAST> AxiFlit;  
// 其中 AxiFlit会包含：.data(32bit), .last(bool), .user, .id, .dest 等字段
// 在 Vivado HLS 中，可以用 flit.last = true/false 来控制 AXI4-Stream 的 TLAST 信号

struct FlitInternal {
    ap_uint<32> data;
    bool  last;
    ap_uint<3>  outPort;  // 用于保存路由计算得到的输出端口

};

// 将 AxiFlit 转换为内部数据结构
static FlitInternal axi_to_internal(AxiFlit axi) {
    FlitInternal internal;
    internal.data = axi.data;
    internal.last = axi.last;
    internal.outPort = 0;
    return internal;
}

// 将内部数据结构转换为 AxiFlit
static AxiFlit internal_to_axi(FlitInternal internal) {
    AxiFlit axi;
    axi.data = internal.data;
    axi.last = internal.last;
    return axi;
}
// ------------------- 获取 flit 类型的函数 -------------------
static ap_uint<2> get_flit_type(ap_uint<32> data) {
    // 假设高2位(31:30)表示 flit 类型
    return data.range(31, 30);
}
// 用于暂存从 in_stream 读出的 flit 等相关信息
struct HeadBuf_t {
    bool valid;         // 是否保存了一个flit
    FlitInternal flit;// 暂存flit本身
};
struct RouterCfg {
    ap_uint<2>  topo;     // 0-Mesh, 1-Torus
    ap_uint<2>  algo;     // 0-XY, 1-West-First
    ap_uint<2>  cur_x;
    ap_uint<2>  cur_y;
    
};

// ------------------- 获取目标坐标的函数 -------------------
// 假设在 data[29:28] 存 X 坐标, [27:26] 存 Y 坐标，仅示例
static ap_uint<2> get_dest_x(ap_uint<32> data) {
    return data.range(29, 28);
}
static ap_uint<2> get_dest_y(ap_uint<32> data) {
    return data.range(27, 26);
}

//xy算法
static ap_uint<3> xy_route(
    ap_uint<2> cur_x,
    ap_uint<2> cur_y,
    ap_uint<2> dest_x,
    ap_uint<2> dest_y,
    Topology topo
) {
#pragma HLS INLINE
    // 如果目标即本节点, 返回0(LOCAL) 做示例
    if (cur_x == dest_x && cur_y == dest_y) {
        return 0; // local
    }

    // Mesh拓扑
    if (topo == TOPO_MESH) {
        // 先在X方向移动
        if (dest_x > cur_x) return 3; // East
        if (dest_x < cur_x) return 4; // West
        // X相等时再看Y
        if (dest_y > cur_y) return 1; // North
        else return 2;               // South
    }
    else { 
        // TOPO_TORUS 示例, 需考虑环绕(仅示意)
        ap_int<3> dx = (ap_int<3>)dest_x - (ap_int<3>)cur_x;
        ap_int<3> dy = (ap_int<3>)dest_y - (ap_int<3>)cur_y;
        // 处理环绕距离
        if (dx >  1) dx -= NOC_SIZE;
        if (dx < -1) dx += NOC_SIZE;
        if (dy >  1) dy -= NOC_SIZE;
        if (dy < -1) dy += NOC_SIZE;
        // 先修正X
        if (dx > 0) return 3; // East
        if (dx < 0) return 4; // West
        // 再修正Y
        if (dy > 0) return 1; // North
        return 2; // South
    }
}

//west_first 算法
static ap_uint<3> west_first_route(
    ap_uint<2> cur_x, ap_uint<2> cur_y,
    ap_uint<2> dest_x,ap_uint<2> dest_y,
    Topology topo)
{
#pragma HLS INLINE
    if(topo==TOPO_TORUS)   // 简化：Torus 下退化为 XY
        return xy_route(cur_x,cur_y,dest_x,dest_y,topo);

    if(dest_x < cur_x)           // 必须先向西
        return 4;
    else
        return xy_route(cur_x,cur_y,dest_x,dest_y,topo);
}

static ap_uint<3> route_compute(
    ap_uint<2> cur_x, ap_uint<2> cur_y,
    ap_uint<2> dest_x,ap_uint<2> dest_y,
    Topology topo, Algo algo)
{
#pragma HLS INLINE
    return (algo==ALGO_WEST_FIRST) ?
        west_first_route(cur_x,cur_y,dest_x,dest_y,topo) :
        xy_route      (cur_x,cur_y,dest_x,dest_y,topo);
}


void noc_router_top(
    hls::stream<AxiFlit> in_stream[NUM_INPUTS],
    hls::stream<AxiFlit> out_stream[NUM_OUTPUTS],
    RouterCfg            &cfg,
    const bool port_enable[NUM_INPUTS]
);

#endif // NOCRouter_H

