`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 12/02/2025 09:06:00 PM
// Design Name: 
// Module Name: Hsa
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

/*
Loading stage (opmode 01):
When we receive dataframe in wrapper,
-> put into weight bus
-> x and y on weight col ix and row ix
-> wEn set high

Compute stage (opmode 10):
-> Acts passed in, as well as
-> act column mask to gate!
*/

module Hsa #(parameter SIZE = 2, BIT_WIDTH = 4) (
    // When in MMM mode, pass in weight at 0th index
    input [BIT_WIDTH-1:0] weights [SIZE-1:0],       // buses for loading a weight value
	input [$clog2(SIZE)-1:0] weight_col_ix,		    // holds the weight col write to when wEn = 1 
	input [$clog2(SIZE)-1:0] weight_row_ix,		    // holds the weight row write to when wEn = 1 
	input wEn,
	input en,
	input reset,					// TODO: not like cpp_impl reset, this sets everything to 0
	input clk,
    input hsa_mode,                 // 0 = vector, 1 = matrix
	input [BIT_WIDTH-1:0] activation [SIZE-1:0],    // row of acts coming in from top
    input [$clog2(SIZE)-1:0] act_col_ix,        // for MVM mode
    input [31:0] cycle_ctr,          
    output [BIT_WIDTH-1:0] my_weights [SIZE-1:0][SIZE-1:0],  // for debug

	output [BIT_WIDTH-1:0] result [SIZE-1:0]       // buses for outputting result
);

	/* Create and wire the MAC units and corresponding latches */
	genvar i,j;
	reg [BIT_WIDTH-1:0] left_latches [SIZE-1:0][SIZE-1:0]; 			// store rightwards flow of partial sums
    reg [BIT_WIDTH-1:0] top_latches  [SIZE-1:0][SIZE-1:0];
	wire [BIT_WIDTH-1:0] broadcast_channels [SIZE-1:0];    

	generate
		for (i = 0; i < SIZE; i = i + 1) begin : row
			for (j = 0; j < SIZE; j = j + 1) begin : col
				wire [BIT_WIDTH-1:0] psum;
				wire enable, wEnable;
				wire [BIT_WIDTH-1:0] act;
                wire [BIT_WIDTH-1:0] weight;
                wire [BIT_WIDTH-1:0] cin;

                assign enable = en & (
                            hsa_mode ? ((i+j) <= cycle_ctr && cycle_ctr <= (i+j+SIZE))
                                     : (act_col_ix == j));

                assign wEnable = wEn & (
                            hsa_mode ? ((weight_col_ix == j) & (weight_row_ix == i))
                                     : (weight_col_ix == j));
                assign act = hsa_mode ? (i == 0 ? activation[j] : top_latches[i-1][j])
                                      : broadcast_channels[j];

                assign weight = hsa_mode ? weights[0] : weights[i];

                assign cin = j==0 ? '0 : left_latches[i][j-1];
                 
				WsMac #(.BIT_WIDTH(BIT_WIDTH)) mac(
					.a(act),   // get act from top latch or if first from activation

					.w(weight),                                // not used in computation, but when the weight is passed in,
					.wEn(wEnable),       			
					.en(enable),				               // this signal is actually unused, we clock gate the latches below
					.clk(clk),
					.cin(cin),     // pass in psum from left latch, unless we are first MAC, in which
                                                               // case no previous sum to use
                    .cout(psum)                                // the partial sum being output to be latched to the next latch
				);

				always @(posedge clk & enable) begin
					left_latches[i][j] <= psum;
					top_latches[i][j] <= act;
                end           
			end 
			/* result just grabs from the last layer of psum latches
             * wrapper module must use its cycle counter to synch when
             * it should grab the results
             */
			assign result[i] = left_latches[i][SIZE-1];
		end
	endgenerate

    /* DeMux the input activations to go down the correct broadcast_channels */
    DeMux #(.BIT_WIDTH(BIT_WIDTH), .SIZE(SIZE)) demux(
        .in(activation[0]),
        .sel(act_col_ix),
        .out(broadcast_channels)
	);
endmodule   
