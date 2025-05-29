// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2024.2 (lin64) Build 5239630 Fri Nov 08 22:34:34 MST 2024
// Date        : Thu May 29 01:28:36 2025
// Host        : SPL running 64-bit unknown
// Command     : write_verilog -rename_top impl -force -mode funcsim bug_dir/impl.v
// Design      : top
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xc7a35ticsg324-1L
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* ECO_CHECKSUM = "3c1afec1" *) 
(* NotValidForBitStream *)
(* \DesignAttr:ENABLE_NOC_NETLIST_VIEW  *) 
(* \DesignAttr:ENABLE_AIE_NETLIST_VIEW  *) 
module impl
   (clk,
    _01_,
    _02_,
    _20_,
    _21_,
    _22_);
  input clk;
  input _01_;
  input _02_;
  output _20_;
  output _21_;
  output _22_;

  wire _01_;
  wire _02_;
  wire _03_;
  wire _05_;
  wire _06_;
  wire _07_;
  wire _08_;
  wire _09_;
  wire _20_;
  wire _21_;
  wire _22_;
  wire _30_;
  wire clk;
  wire clk_IBUF;

  OBUF _100_
       (.I(_08_),
        .O(_20_));
  OBUF _101_
       (.I(_07_),
        .O(_21_));
  OBUF _102_
       (.I(_09_),
        .O(_22_));
  (* OPT_MODIFIED = "RETARGET" *) 
  LUT1 #(
    .INIT(2'h0)) 
    _103_
       (.I0(_03_),
        .O(_05_));
  (* OPT_MODIFIED = "RETARGET" *) 
  CFGLUT5 #(
    .INIT(32'hF437C998),
    .IS_CLK_INVERTED(1'b1)) 
    _31513453_
       (.CDI(_06_),
        .CDO(_08_),
        .CE(1'b1),
        .CLK(_30_),
        .I0(_05_),
        .I1(_05_),
        .I2(1'b0),
        .I3(1'b0),
        .I4(1'b0),
        .O5(_07_),
        .O6(_09_));
  BUFG _38_
       (.I(clk_IBUF),
        .O(_30_));
  IBUF _63_
       (.I(_01_),
        .O(_03_));
  IBUF _66_
       (.I(_02_),
        .O(_06_));
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
