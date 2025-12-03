`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 12/03/2025 06:34:59 PM
// Design Name: 
// Module Name: SpMac
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module SpMac #(parameter BIT_WIDTH = 4) (
	input [BIT_WIDTH-1:0] a0,		// activation value to be passed in when en=1
    input [BIT_WIDTH-1:0] a1,
	input [BIT_WIDTH-1:0] w, 		// weight value to be passed in when wEn=1 (will be written to inner mem)
    input wix,
	input wEn,
	input en,                       // not needed here since we gate the latches in the SA, not the unit
	input clk,
	input [BIT_WIDTH-1:0] cin,		// partial sum to be passed in
    output [BIT_WIDTH-1:0] my_weight,               // For dbg
	output [BIT_WIDTH-1:0] cout		// a*w+cin
    );
	reg [BIT_WIDTH-1:0] weight;
    reg weight_ix;

    assign my_weight = weight;

	/* falling edge to avoid collision from incoming weight */
	always @(negedge clk) begin
		if (wEn) begin
			weight <= w;
            weight_ix <= wix;
		end
	end

    wire [BIT_WIDTH-1:0] a;
    assign a = wix ? a1 : a0;

	/* en signal acts as a clock gate for MAC ops ? */
	// always @(posedge clk & en)
	assign cout = a*weight + cin;
endmodule

