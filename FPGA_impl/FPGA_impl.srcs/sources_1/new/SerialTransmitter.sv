`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 11/14/2025 11:15:08 PM
// Design Name: 
// Module Name: SerialTransmitter
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


module SerialTransmitter #(parameter FRAME_WIDTH = 8) (
	input [FRAME_WIDTH - 1:0] data_frame,      
    input send,                             // this should pulse on for 1 clk_uart cycle to begin transmission
    input clk_uart,                         // expects this to be 16 times the clock rate

    output tx
);
    reg [7:0] cycle_ctr; 
    reg [$clog2(FRAME_WIDTH / 8):0] byte_counter;             // for current_byte, 
    reg sending;
    reg send_signal;
    reg [7:0] current_byte;

    SerialByteTransmitter UART_BYTE_TRANSMITTER(
        .data_frame(current_byte),
        .send(send_signal),
        .clk_uart(clk_uart),
        .tx(tx)
    );

    always @(posedge clk_uart) begin
        if (sending) begin
            current_byte <= data_frame[byte_counter*8 +: 8];
            cycle_ctr <= cycle_ctr + 1;
            if (cycle_ctr == 1) begin
                send_signal <= 0;
            // 16*(1+8+1+1)= 176
            end else if (cycle_ctr == 175) begin
                byte_counter <= byte_counter + 1;
                send_signal <= 1;
                cycle_ctr <= 0;
            end
        end else if (send) begin                        
            sending <= 1;
            current_byte <= data_frame[7:0];
            byte_counter <= 0;
            send_signal <= 1;
            cycle_ctr <= 0;
        end
    end
endmodule
