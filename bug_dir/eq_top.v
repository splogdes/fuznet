module eq_top(
    input wire clk,
    input wire _01_,
    input wire _07_,
    input wire _10_,
    output wire _53_,
    output wire _73_,
    output wire _75_,
    output wire _77_,
    output wire _79_,
    output wire _81_,
    output wire _83_,
    output wire _85_,
    output wire _87_,
    output wire _89_,
    output wire _91_,
    output wire trigger
);

    wire _53__impl;
    wire _53__synth;
    wire _73__impl;
    wire _73__synth;
    wire _75__impl;
    wire _75__synth;
    wire _77__impl;
    wire _77__synth;
    wire _79__impl;
    wire _79__synth;
    wire _81__impl;
    wire _81__synth;
    wire _83__impl;
    wire _83__synth;
    wire _85__impl;
    wire _85__synth;
    wire _87__impl;
    wire _87__synth;
    wire _89__impl;
    wire _89__synth;
    wire _91__impl;
    wire _91__synth;
    wire equivalent;

    impl inst_impl (
        .clk(clk),
        ._01_(_01_),
        ._07_(_07_),
        ._10_(_10_),
        ._53_(_53__impl),
        ._73_(_73__impl),
        ._75_(_75__impl),
        ._77_(_77__impl),
        ._79_(_79__impl),
        ._81_(_81__impl),
        ._83_(_83__impl),
        ._85_(_85__impl),
        ._87_(_87__impl),
        ._89_(_89__impl),
        ._91_(_91__impl)
    );

    synth inst_synth (
        .clk(clk),
        ._01_(_01_),
        ._07_(_07_),
        ._10_(_10_),
        ._53_(_53__synth),
        ._73_(_73__synth),
        ._75_(_75__synth),
        ._77_(_77__synth),
        ._79_(_79__synth),
        ._81_(_81__synth),
        ._83_(_83__synth),
        ._85_(_85__synth),
        ._87_(_87__synth),
        ._89_(_89__synth),
        ._91_(_91__synth)
    );

    assign _53_ = (_53__impl === _53__synth);
    assign _73_ = (_73__impl === _73__synth);
    assign _75_ = (_75__impl === _75__synth);
    assign _77_ = (_77__impl === _77__synth);
    assign _79_ = (_79__impl === _79__synth);
    assign _81_ = (_81__impl === _81__synth);
    assign _83_ = (_83__impl === _83__synth);
    assign _85_ = (_85__impl === _85__synth);
    assign _87_ = (_87__impl === _87__synth);
    assign _89_ = (_89__impl === _89__synth);
    assign _91_ = (_91__impl === _91__synth);

    assign equivalent = &{ _53_, _73_, _75_, _77_, _79_, _81_, _83_, _85_, _87_, _89_, _91_ };
    assign trigger = ~equivalent;

endmodule
