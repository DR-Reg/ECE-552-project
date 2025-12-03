`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 12/03/2025 04:27:00 PM
// Design Name: 
// Module Name: SynchCDC
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


module SynchCDC(
        input clk_dest,
        input reset_dest,
        input in,
        output out
);
    (* ASYNC_REG = "TRUE" *) reg ff1;
    reg ff2;
    always @(posedge clk_dest) begin
        if (reset_dest) begin
            ff1 <= 0;
            ff2 <= 0;
        end else begin
            ff1 <= in;
            ff2 <= ff1;
        end
    end
    assign out = ff2;
endmodule
