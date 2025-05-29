module eq_top(
    input wire clk,
    input wire _01_,
    input wire _02_,
    output wire trigger
);

    wire _20__impl;
    wire _20__synth;
    wire _20_;
    wire _21__impl;
    wire _21__synth;
    wire _21_;
    wire _22__impl;
    wire _22__synth;
    wire _22_;
    wire equivalent;

    impl inst_impl (
        .clk(clk),
        ._01_(_01_),
        ._02_(_02_),
        ._20_(_20__impl),
        ._21_(_21__impl),
        ._22_(_22__impl)
    );

    synth inst_synth (
        .clk(clk),
        ._01_(_01_),
        ._02_(_02_),
        ._20_(_20__synth),
        ._21_(_21__synth),
        ._22_(_22__synth)
    );

    assign _20_ = (_20__impl === _20__synth);
    assign _21_ = (_21__impl === _21__synth);
    assign _22_ = (_22__impl === _22__synth);

    assign equivalent = &{ _20_, _21_, _22_ };
    assign trigger = ~equivalent;

endmodule
