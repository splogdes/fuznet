// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2024.2 (lin64) Build 5239630 Fri Nov 08 22:34:34 MST 2024
// Date        : Mon Jun  2 02:11:15 2025
// Host        : SPL running 64-bit unknown
// Command     : write_verilog -rename_top synth -force -mode funcsim bug_dir/synth.v
// Design      : top
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xc7a35ticsg324-1L
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* NotValidForBitStream *)
(* \DesignAttr:ENABLE_NOC_NETLIST_VIEW  *) 
(* \DesignAttr:ENABLE_AIE_NETLIST_VIEW  *) 
module synth
   (_01_,
    _07_,
    _10_,
    clk,
    _53_,
    _73_,
    _75_,
    _77_,
    _79_,
    _81_,
    _83_,
    _85_,
    _87_,
    _89_,
    _91_);
  input _01_;
  input _07_;
  input _10_;
  input clk;
  output _53_;
  output _73_;
  output _75_;
  output _77_;
  output _79_;
  output _81_;
  output _83_;
  output _85_;
  output _87_;
  output _89_;
  output _91_;

  wire _01_;
  wire _03_;
  wire _07_;
  wire _09_;
  wire _10_;
  wire _13_;
  wire _18_;
  wire _20_;
  wire _21_;
  wire _22_;
  wire _24_;
  wire _25_;
  wire _33_;
  wire _53_;
  wire _62_;
  wire _63_;
  wire _64_;
  wire _65_;
  wire _66_;
  wire _67_;
  wire _68_;
  wire _69_;
  wire _70_;
  wire _71_;
  wire _73_;
  wire _75_;
  wire _77_;
  wire _79_;
  wire _81_;
  wire _83_;
  wire _85_;
  wire _87_;
  wire _89_;
  wire _91_;

  IBUF _02_
       (.I(_01_),
        .O(_03_));
  IBUF _08_
       (.I(_07_),
        .O(_09_));
  IBUF _12_
       (.I(_10_),
        .O(_13_));
  CARRY4 _16_
       (.CI(_09_),
        .CO({_62_,_18_,_63_,_20_}),
        .CYINIT(_09_),
        .DI({_13_,_09_,_03_,_09_}),
        .O({_21_,_22_,_64_,_24_}),
        .S({_03_,_03_,_13_,_13_}));
  (* XILINX_LEGACY_PRIM = "MUXF5" *) 
  (* XILINX_TRANSFORM_PINMAP = "S:I2" *) 
  LUT3 #(
    .INIT(8'hCA)) 
    _26_
       (.I0(_24_),
        .I1(_18_),
        .I2(_22_),
        .O(_25_));
  CARRY4 _29_
       (.CI(_22_),
        .CO({_65_,_66_,_67_,_33_}),
        .CYINIT(_13_),
        .DI({_21_,_24_,_25_,_22_}),
        .O({_68_,_69_,_70_,_71_}),
        .S({_20_,_22_,_09_,_25_}));
  OBUF _52_
       (.I(_33_),
        .O(_53_));
  OBUF _72_
       (.I(_62_),
        .O(_73_));
  OBUF _74_
       (.I(_63_),
        .O(_75_));
  OBUF _76_
       (.I(_64_),
        .O(_77_));
  OBUF _78_
       (.I(_65_),
        .O(_79_));
  OBUF _80_
       (.I(_66_),
        .O(_81_));
  OBUF _82_
       (.I(_67_),
        .O(_83_));
  OBUF _84_
       (.I(_68_),
        .O(_85_));
  OBUF _86_
       (.I(_69_),
        .O(_87_));
  OBUF _88_
       (.I(_70_),
        .O(_89_));
  OBUF _90_
       (.I(_71_),
        .O(_91_));
endmodule
`ifndef GLBL
`define GLBL
`timescale  1 ps / 1 ps

module glbl ();

    parameter ROC_WIDTH = 100000;
    parameter TOC_WIDTH = 0;
    parameter GRES_WIDTH = 10000;
    parameter GRES_START = 10000;

//--------   STARTUP Globals --------------
    wire GSR;
    wire GTS;
    wire GWE;
    wire PRLD;
    wire GRESTORE;
    tri1 p_up_tmp;
    tri (weak1, strong0) PLL_LOCKG = p_up_tmp;

    wire PROGB_GLBL;
    wire CCLKO_GLBL;
    wire FCSBO_GLBL;
    wire [3:0] DO_GLBL;
    wire [3:0] DI_GLBL;
   
    reg GSR_int;
    reg GTS_int;
    reg PRLD_int;
    reg GRESTORE_int;

//--------   JTAG Globals --------------
    wire JTAG_TDO_GLBL;
    wire JTAG_TCK_GLBL;
    wire JTAG_TDI_GLBL;
    wire JTAG_TMS_GLBL;
    wire JTAG_TRST_GLBL;

    reg JTAG_CAPTURE_GLBL;
    reg JTAG_RESET_GLBL;
    reg JTAG_SHIFT_GLBL;
    reg JTAG_UPDATE_GLBL;
    reg JTAG_RUNTEST_GLBL;

    reg JTAG_SEL1_GLBL = 0;
    reg JTAG_SEL2_GLBL = 0 ;
    reg JTAG_SEL3_GLBL = 0;
    reg JTAG_SEL4_GLBL = 0;

    reg JTAG_USER_TDO1_GLBL = 1'bz;
    reg JTAG_USER_TDO2_GLBL = 1'bz;
    reg JTAG_USER_TDO3_GLBL = 1'bz;
    reg JTAG_USER_TDO4_GLBL = 1'bz;

    assign (strong1, weak0) GSR = GSR_int;
    assign (strong1, weak0) GTS = GTS_int;
    assign (weak1, weak0) PRLD = PRLD_int;
    assign (strong1, weak0) GRESTORE = GRESTORE_int;

    initial begin
	GSR_int = 1'b1;
	PRLD_int = 1'b1;
	#(ROC_WIDTH)
	GSR_int = 1'b0;
	PRLD_int = 1'b0;
    end

    initial begin
	GTS_int = 1'b1;
	#(TOC_WIDTH)
	GTS_int = 1'b0;
    end

    initial begin 
	GRESTORE_int = 1'b0;
	#(GRES_START);
	GRESTORE_int = 1'b1;
	#(GRES_WIDTH);
	GRESTORE_int = 1'b0;
    end

endmodule
`endif
