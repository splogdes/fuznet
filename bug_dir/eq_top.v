module eq_top(
    input wire clk,
    input wire _04_,
    input wire _12_,
    output wire _31_,
    output wire _48_,
    output wire _50_,
    output wire _52_,
    output wire _54_,
    output wire _56_,
    output wire _58_,
    output wire _60_,
    output wire trigger
);

    wire _31__impl;
    wire _31__synth;
    wire _48__impl;
    wire _48__synth;
    wire _50__impl;
    wire _50__synth;
    wire _52__impl;
    wire _52__synth;
    wire _54__impl;
    wire _54__synth;
    wire _56__impl;
    wire _56__synth;
    wire _58__impl;
    wire _58__synth;
    wire _60__impl;
    wire _60__synth;
    wire equivalent;

    impl inst_impl (
        .clk(clk),
        ._04_(_04_),
        ._12_(_12_),
        ._31_(_31__impl),
        ._48_(_48__impl),
        ._50_(_50__impl),
        ._52_(_52__impl),
        ._54_(_54__impl),
        ._56_(_56__impl),
        ._58_(_58__impl),
        ._60_(_60__impl)
    );

    synth inst_synth (
        .clk(clk),
        ._04_(_04_),
        ._12_(_12_),
        ._31_(_31__synth),
        ._48_(_48__synth),
        ._50_(_50__synth),
        ._52_(_52__synth),
        ._54_(_54__synth),
        ._56_(_56__synth),
        ._58_(_58__synth),
        ._60_(_60__synth)
    );

    assign _31_ = (_31__impl === _31__synth);
    assign _48_ = (_48__impl === _48__synth);
    assign _50_ = (_50__impl === _50__synth);
    assign _52_ = (_52__impl === _52__synth);
    assign _54_ = (_54__impl === _54__synth);
    assign _56_ = (_56__impl === _56__synth);
    assign _58_ = (_58__impl === _58__synth);
    assign _60_ = (_60__impl === _60__synth);

    assign equivalent = &{ _31_, _48_, _50_, _52_, _54_, _56_, _58_, _60_ };
    assign trigger = ~equivalent;

endmodule
