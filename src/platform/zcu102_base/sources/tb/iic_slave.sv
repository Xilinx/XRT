`timescale 1ps/1ps
`default_nettype none

module iic_slave (
inout wire sda_io,
inout wire scl_io
);

wire sda_i;
logic sda_o;
logic sda_tri = 1'b1;
wire scl_i,scl_int,scl_o;
wire scl_tri;

logic mode = 'b0;

logic start;
logic add_ack = 'b0;
logic data_ack = 'b0;
logic [7:0] address = 'b0;
logic [7:0] data    = 'b0;
logic add_det = 'b0;
logic data_det = 'b0;

logic [3:0] add_cnt  = 'b0; 
logic [3:0] data_cnt = 'b0; 
logic in = 0;

//Connect to pull up resistor
pullup scl_pullup (scl_io);
pullup sda_pullup (sda_io);


IOBUF scl_inst (
.IO (scl_io),
.I  (scl_o),
.O  (scl_i),
.T  (scl_tri)
);

IOBUF sda_inst (
.IO (sda_io),
.I  (sda_o),
.O  (sda_i),
.T  (sda_tri)
);


///////////////////////////////
//IIC SLAVE to read/write mode 
//0 - MASTER WRITES TO SLV
//1-  MASTER READS FROM SLV 
function void set_slv_mode(bit read_write);
 mode = read_write;
 add_det = 'b0;
 add_ack = 'b0;
 add_cnt = 'b0;
 start   = 'b0;
endfunction:set_slv_mode


assign scl_tri = 'b1;

always @(add_cnt,data_cnt) begin
 if(add_cnt == 4'b1000)   add_det  <= 'b1;
 if(data_cnt  == 4'b1001) data_det <= 'b1;
end


always @(sda_i) begin
 //Detect start condition
 if(scl_i == 'b1 & sda_i == 'b0) start <= 1;
end

always @(posedge scl_i) 
begin
  if(start & !add_det) begin
    //get address after start
    address = address << 1;
    add_cnt <= add_cnt + 1;
    address[0] <= sda_i;
  end else if(add_ack & !data_ack & !mode) begin
    data = data << 1;
    data_cnt <= data_cnt + 1;
    data[0] <=  sda_i; 
  end
end

always @(posedge scl_i) begin
  in <= ~in ;
end


always @(negedge scl_i) begin
 if (add_det & !add_ack) begin
   //Slave ack on address 
   sda_tri <= 0;
   sda_o   <= 0;
   add_ack <= 1;
 end else if (data_det & !data_ack & !mode) begin
   //Slave ack after data transfer
   sda_tri  <= 0;
   sda_o    <= 0;
   data_ack <= 1;
 end else if (mode) begin
   sda_tri <= (~add_det) ? 1 : ~in;
 end else begin
   sda_tri <= 1;
 end
end

endmodule:iic_slave


