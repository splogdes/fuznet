module top(clk, _01_, _02_, _03_, _04_, _05_, _20_, _21_, _22_);
  input wire clk;
  input wire _01_;
  input wire _02_;
  input wire _03_;
  input wire _04_;
  input wire _05_;
  output wire _20_;
  output wire _21_;
  output wire _22_;

  wire _07_;
  wire _08_;
  wire _09_;
  wire _10_;
  wire _11_;
  wire _12_;
  wire _13_;
  wire _14_;
  wire _15_;

  wire _16_;
  wire _17_;
  wire _18_;

  IBUF _63_ (
    .I(_01_),
    .O(_07_)
  );
  IBUF _73_ (
    .I(_02_),
    .O(_09_)
  );
  IBUF _64_ (
    .I(_03_),
    .O(_10_)
  );
  IBUF _65_ (
    .I(_04_),
    .O(_11_)
  );
  IBUF _72_ (
    .I(_05_),
    .O(_12_)
  );
  BUFG _38_ (
    .I(clk),
    .O(_13_)
  );

  LUT1 #(
    .INIT(2'h3)
  ) _103_ (
    .I0(_07_),
    .O(_15_)
  );
    
  LUT1 #(
    .INIT(2'h1)
  ) _143_ (
    .I0(_15_),
    .O(_14_)
  );

  CFGLUT5 #(
    .INIT(32'hF437C998),
    .IS_CLK_INVERTED(1'b1)
  ) _31513453_ (
    .CDI(_07_),
    .CDO(_16_),
    .CE(_15_),
    .CLK(_13_),
    .O5(_17_),
    .O6(_18_),
    .I0(_14_),
    .I1(_09_),
    .I2(_10_),
    .I3(_11_),
    .I4(_12_)
  );

  OBUF _98_ (
    .I(_17_),
    .O(_20_)
  );

  OBUF _99_ (
    .I(_18_),
    .O(_21_)
  );

  OBUF _100_ (
    .I(_16_),
    .O(_22_)
  );

endmodule
