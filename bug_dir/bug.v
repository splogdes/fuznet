module top(clk, _01_, _02_, _20_, _21_, _22_);
  input wire clk;
  input wire _01_;
  input wire _02_;
  output wire _20_;
  output wire _21_;
  output wire _22_;

  wire _30_;

  wire _03_;
  wire _04_;
  wire _05_;
  wire _06_;
  wire _07_;
  wire _08_;
  wire _09_;

  IBUF _63_ (
    .I(_01_),
    .O(_03_)
  );
  IBUF _66_ (
    .I(_02_),
    .O(_06_)
  );
  BUFG _38_ (
    .I(clk),
    .O(_30_)
  );

  LUT1 #(
    .INIT(2'h3)
  ) _103_ (
    .I0(_03_),
    .O(_04_)
  );
    
  LUT1 #(
    .INIT(2'h1)
  ) _143_ (
    .I0(_04_),
    .O(_05_)
  );

  CFGLUT5 #(
    .INIT(32'hF437C998),
    .IS_CLK_INVERTED(1'b1)
  ) _31513453_ (
    .CDI(_06_),
    .CDO(_08_),
    .O5(_07_),
    .O6(_09_),
    .CE(1'b1),
    .CLK(_30_),
    .I0(_04_),
    .I1(_05_),
    .I2(1'b0),
    .I3(1'b0),
    .I4(1'b0)
  );


  OBUF _100_ (
    .I(_08_), 
    .O(_20_)
  );

  OBUF _101_ (
    .I(_07_), 
    .O(_21_)
  );

  OBUF _102_ (
    .I(_09_), 
    .O(_22_)
  );

endmodule
