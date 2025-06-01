module eq_top(
    input wire clk,
    input wire _005_,
    input wire _011_,
    input wire _014_,
    input wire _015_,
    input wire _999_,
    output wire _091_,
    output wire _097_,
    output wire _101_,
    output wire trigger
);

    wire _091__impl;
    wire _091__synth;
    wire _097__impl;
    wire _097__synth;
    wire _101__impl;
    wire _101__synth;
    wire equivalent;

    impl inst_impl (
        .clk(clk),
        ._005_(_005_),
        ._011_(_011_),
        ._014_(_014_),
        ._015_(_015_),
        ._999_(_999_),
        ._091_(_091__impl),
        ._097_(_097__impl),
        ._101_(_101__impl)
    );

    synth inst_synth (
        .clk(clk),
        ._005_(_005_),
        ._011_(_011_),
        ._014_(_014_),
        ._015_(_015_),
        ._999_(_999_),
        ._091_(_091__synth),
        ._097_(_097__synth),
        ._101_(_101__synth)
    );

    assign _091_ = (_091__impl === _091__synth);
    assign _097_ = (_097__impl === _097__synth);
    assign _101_ = (_101__impl === _101__synth);

    assign equivalent = &{ _091_, _097_, _101_ };
    assign trigger = ~equivalent;

endmodule
