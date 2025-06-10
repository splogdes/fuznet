// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2024.2 (lin64) Build 5239630 Fri Nov 08 22:34:34 MST 2024
// Date        : Tue Jun 10 03:02:23 2025
// Host        : SPL running 64-bit unknown
// Command     : write_verilog -rename_top impl -force -mode funcsim bug_dir/impl.v
// Design      : top
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xc7a35ticsg324-1L
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* ECO_CHECKSUM = "65e2d8b8" *) (* POWER_OPT_BRAM_CDC = "0" *) (* POWER_OPT_BRAM_SR_ADDR = "0" *) 
(* POWER_OPT_LOOPED_NET_PERCENTAGE = "0" *) 
(* NotValidForBitStream *)
(* \DesignAttr:ENABLE_NOC_NETLIST_VIEW  *) 
(* \DesignAttr:ENABLE_AIE_NETLIST_VIEW  *) 
module impl
   (_04_,
    clk,
    _12_,
    _31_,
    _48_,
    _50_,
    _52_,
    _54_,
    _56_,
    _58_,
    _60_);
  input _04_;
  input clk;
  input _12_;
  output _31_;
  output _48_;
  output _50_;
  output _52_;
  output _54_;
  output _56_;
  output _58_;
  output _60_;

  wire _04_;
  wire _07_;
  wire _12_;
  wire _14_;
  wire _31_;
  wire _40_;
  wire _41_;
  wire _42_;
  wire _43_;
  wire _44_;
  wire _45_;
  wire _46_;
  wire _48_;
  wire _50_;
  wire _52_;
  wire _54_;
  wire _56_;
  wire _58_;
  wire _60_;
  wire [0:0]NLW__15__CO_UNCONNECTED;

  IBUF _06_
       (.I(_04_),
        .O(_07_));
  IBUF _13_
       (.I(_12_),
        .O(_14_));
  (* OPT_MODIFIED = "PROPCONST" *) 
  CARRY4 _15_
       (.CI(1'b1),
        .CO({_40_,_41_,_42_,NLW__15__CO_UNCONNECTED[0]}),
        .CYINIT(_14_),
        .DI({_07_,_07_,_07_,_14_}),
        .O({_43_,_44_,_45_,_46_}),
        .S({_07_,_07_,_07_,_07_}));
  (* OPT_MODIFIED = "PROPCONST" *) 
  OBUF _30_
       (.I(_14_),
        .O(_31_));
  OBUF _47_
       (.I(_40_),
        .O(_48_));
  OBUF _49_
       (.I(_41_),
        .O(_50_));
  OBUF _51_
       (.I(_42_),
        .O(_52_));
  OBUF _53_
       (.I(_43_),
        .O(_54_));
  OBUF _55_
       (.I(_44_),
        .O(_56_));
  OBUF _57_
       (.I(_45_),
        .O(_58_));
  OBUF _59_
       (.I(_46_),
        .O(_60_));
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
