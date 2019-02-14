# Generate an empty _post_sys_link_gen_constrs.xdc file
# -------------------------------------------------------------------------
set fd [open "./_post_sys_link_gen_constrs.xdc" w]
puts $fd "# No content"
close $fd

# Connect available interrupt pins on compute units to the interrupt vector
# -------------------------------------------------------------------------

set __num_xlconcat 4
set __num_pin_per_xlconcat 32

# Add interrupt controler and concat ips
proc add_interrupt_ctrl_concat { __cu_num } {

  upvar __num_xlconcat __num_xlconcat
  upvar __num_pin_per_xlconcat __num_pin_per_xlconcat

  # Generate interrupt contrlor and interrupt number pair list
  set __inst_intrs_list {} 
  set __remain_cu $__cu_num
  set __inst_num 0
  while { $__remain_cu > 0 } {
    if { $__remain_cu >= $__num_pin_per_xlconcat } {
      lappend __inst_intrs_list "$__inst_num 32"
      set __remain_cu [expr {$__remain_cu - 32}]
    } else {
      lappend __inst_intrs_list "$__inst_num ${__remain_cu}"
      set __remain_cu 0
    }
    incr __inst_num
  }

  #Add IPs
  foreach __pair $__inst_intrs_list {
    lassign $__pair __intc_inst_num __intrs_num
    create_bd_cell -type ip -vlnv xilinx.com:ip:axi_intc axi_intc_${__intc_inst_num}
    set_property -dict [list CONFIG.C_IRQ_IS_LEVEL {0} CONFIG.C_IRQ_CONNECTION {1}] [get_bd_cells axi_intc_${__intc_inst_num}]
    create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat xlconcat_intc_${__intc_inst_num}
    set_property -dict [list CONFIG.NUM_PORTS ${__intrs_num}] [get_bd_cells xlconcat_intc_${__intc_inst_num}]

    connect_bd_net [get_bd_pins xlconcat_intc_${__intc_inst_num}/dout] [get_bd_pins axi_intc_${__intc_inst_num}/intr]

    # Connect concat input to GND for now
    for { set i 0 } { $i < ${__intrs_num} } { incr i } {
      connect_bd_net [get_bd_pins xlconcat_intc_${__intc_inst_num}/In${i}] [get_bd_pins sds_irq_const/dout]
    }

    # Use 4 PL to PS interrupts, they are on xlconcat_0
    set __xlconcat_inst [get_bd_cells -hierarchical -quiet -filter NAME=~xlconcat_0]
    set __xlconcat_pin [get_bd_pins -of_objects $__xlconcat_inst -quiet -filter NAME=~In${__intc_inst_num}]

    # If the xlconcat pin object exists, disconnect it from ground and connect the interrupt controlor's irq to it.
    if {[llength $__xlconcat_pin] == 1} {
      disconnect_bd_net /sds_irq_const_dout $__xlconcat_pin -quiet
      connect_bd_net [get_bd_pins axi_intc_${__intc_inst_num}/irq] $__xlconcat_pin -quiet
      # Connect interrupt controlor to HPM0
      apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/ps_e/M_AXI_HPM0_FPD} Slave {/axi_intc_${__intc_inst_num}/s_axi} intc_ip {Auto} master_apm {0}}  [get_bd_intf_pins axi_intc_${__intc_inst_num}/s_axi]
      # Hard code address for now...
      set offset [expr 0xA0800000 + ${__intc_inst_num} * 0x1000]
      set_property offset $offset [get_bd_addr_segs "ps_e/Data/SEG_axi_intc_${__intc_inst_num}_Reg"]
    } else {
      puts "(Post-linking DSA Tcl hook) No available xlconcat pins found"
    }
  }
}

# The wiring proc takes in the CU's interrupt BD pin and the overall interrupt index
proc wire_cu_to_xlconcat_intr {__cu_inst_intr_pin __intr_pin_num} {
  # Set number of xlconcat blocks and number of interrupts per block

  upvar __num_xlconcat __num_xlconcat
  upvar __num_pin_per_xlconcat __num_pin_per_xlconcat

  # Get the xlconcat instance and pin number to work on now
  set __xlconcat_inst_num [expr {$__intr_pin_num / $__num_pin_per_xlconcat}]
  set __xlconcat_pin_num [expr {$__intr_pin_num - ($__xlconcat_inst_num * $__num_pin_per_xlconcat)}]

  # Ensure that the xlconcat instance and its pin exist, then get those objects
  if {($__xlconcat_pin_num < $__num_pin_per_xlconcat) && ($__xlconcat_inst_num < $__num_xlconcat)} {
    set __xlconcat_inst [get_bd_cells -hierarchical -quiet -filter NAME=~xlconcat_intc_${__xlconcat_inst_num}]
    set __xlconcat_pin [get_bd_pins -of_objects $__xlconcat_inst -quiet -filter NAME=~In${__xlconcat_pin_num}]

    # If the xlconcat pin object exists, disconnect it from ground and connect the CU's interrupt BD pin to it
    if {[llength $__xlconcat_pin] == 1} {
      disconnect_bd_net /sds_irq_const_dout $__xlconcat_pin -quiet
      connect_bd_net $__cu_inst_intr_pin $__xlconcat_pin -quiet
    } else {
      puts "(Post-linking DSA Tcl hook) No available xlconcat_intc pins found"
    }
  } else {
    puts "(Post-linking DSA Tcl hook) No remaining xlconcat_intc pins to connect to"
  }
}

# Make sure the kernel key in the config_info dict exists
if {[dict exists $config_info kernels]} {
  # Make sure that list of kernels is populated
  set __k_list [dict get $config_info kernels]
  if {[llength $__k_list] > 0} {
    # Translate the list of kernels to a list of BD cells and their AXI-Lite address offsets
    set __cu_inst_addr_list {}
    # Iterate over each kernel
    foreach __k_inst $__k_list {
      set __cu_bd_cell_list [get_bd_cells -quiet -filter "VLNV=~*:*:${__k_inst}:*"]
      # Iterate over each compute unit for the current kernel
      foreach __cu_bd_cell $__cu_bd_cell_list {
        set __cu_bd_cell_sub [string range $__cu_bd_cell 1 [string length $__cu_bd_cell]]
        set __cu_bd_cell_segs [get_bd_addr_segs -of_objects [get_bd_addr_spaces ps_e*] -filter "NAME =~ *${__cu_bd_cell_sub}_*"]
        if {[llength ${__cu_bd_cell_segs}] > 0} {
          set __cu_offset [get_property OFFSET [get_bd_addr_segs -of_objects [get_bd_addr_spaces ps_e*] -filter "NAME =~ *${__cu_bd_cell_sub}_*"]]
          lappend __cu_inst_addr_list "$__cu_bd_cell $__cu_offset"
        }
      }
    }
    # Make sure the list of BD cells and their AXI-Lite address offsets is populated
    if {[llength $__cu_inst_addr_list] > 0} {
      # Order the list by increasing AXI-Lite address offsets, then extract just ordered BD cells
      set __cu_inst_list {}
      unset __cu_inst_list
      set __cu_inst_addr_list_ordered [lsort -index 1 $__cu_inst_addr_list]
      foreach __cu_pair $__cu_inst_addr_list_ordered {
        lappend __cu_inst_list [lindex $__cu_pair 0]
      }

      # Insert interrupt controler and concat IPs
      add_interrupt_ctrl_concat [llength $__cu_inst_list]

      # Of the BD cells, iterate through those with an interrupt BD pin
      set __intr_pin_num 0
      foreach __cu_inst_intr $__cu_inst_list {
        set __cu_inst_intr_pin [get_bd_pins -of_objects [get_bd_cells $__cu_inst_intr] -quiet -filter "TYPE=~intr"]
        if {[llength $__cu_inst_intr_pin] == 1} {
          # When a BD cell has an interrupt BD pin, wire it to the next available xlconcat pin
          wire_cu_to_xlconcat_intr $__cu_inst_intr_pin $__intr_pin_num
          incr __intr_pin_num
        }
      }
    } else {
      puts "(Post-linking DSA Tcl hook) No BD cells found for interrupt wiring"
    }
  } else {
    puts "(Post-linking DSA Tcl hook) No CUs found for interrupt wiring"
  }
} else {
  puts "(Post-linking DSA Tcl hook) No kernels key in config_info dict for interrupt wiring"
}
