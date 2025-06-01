// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2024.2 (lin64) Build 5239630 Fri Nov 08 22:34:34 MST 2024
// Date        : Sun Jun  1 03:24:06 2025
// Host        : SPL running 64-bit unknown
// Command     : write_verilog -rename_top impl -force -mode funcsim bug_dir/impl.v
// Design      : top
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xc7a35ticsg324-1L
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* ECO_CHECKSUM = "f82b5c02" *) (* POWER_OPT_BRAM_CDC = "0" *) (* POWER_OPT_BRAM_SR_ADDR = "0" *) 
(* POWER_OPT_LOOPED_NET_PERCENTAGE = "0" *) 
(* NotValidForBitStream *)
(* \DesignAttr:ENABLE_NOC_NETLIST_VIEW  *) 
(* \DesignAttr:ENABLE_AIE_NETLIST_VIEW  *) 
module impl
   (_005_,
    _011_,
    _014_,
    _015_,
    _999_,
    clk,
    _091_,
    _097_,
    _101_);
  input _005_;
  input _011_;
  input _014_;
  input _015_;
  input _999_;
  input clk;
  output _091_;
  output _097_;
  output _101_;

  wire _003_;
  wire _005_;
  wire _007_;
  wire _011_;
  wire _013_;
  wire _015_;
  wire _019_;
  wire _066_;
  wire _067_;
  wire _091_;
  wire _097_;
  wire _101_;
  wire clk;
  wire clk_IBUF;
  wire [1:1]NLW__060__065__A_UNCONNECTED;

  IBUF _006_
       (.I(_005_),
        .O(_007_));
  IBUF _012_
       (.I(_011_),
        .O(_013_));
  BUFG _018_
       (.I(clk_IBUF),
        .O(_019_));
  (* OPT_MODIFIED = "RETARGET" *) 
  (* XILINX_LEGACY_PRIM = "SRLC16" *) 
  (* XILINX_REPORT_XFORM = "SRLC32E" *) 
  (* XILINX_TRANSFORM_PINMAP = "VCC:CE" *) 
  SRLC32E #(
    .INIT(32'h07A06655)) 
    _060__065_
       (.A({1'b1,_007_,_003_,NLW__060__065__A_UNCONNECTED[1],_003_}),
        .CE(1'b1),
        .CLK(_019_),
        .D(_013_),
        .Q(_066_),
        .Q31(_067_));
  IBUF _072_
       (.I(_015_),
        .O(_003_));
  OBUF _090_
       (.I(_067_),
        .O(_091_));
  OBUF _096_
       (.I(_066_),
        .O(_097_));
  (* OPT_MODIFIED = "PROPCONST" *) 
  OBUF _100_
       (.I(1'b1),
        .O(_101_));
  (* OPT_INSERTED *) 
  (* OPT_MODIFIED = "MLO" *) 
  IBUF clk_IBUF_inst
       (.I(clk),
        .O(clk_IBUF));
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
