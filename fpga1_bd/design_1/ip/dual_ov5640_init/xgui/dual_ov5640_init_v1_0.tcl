# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  ipgui::add_param $IPINST -name "CLK_HZ" -parent ${Page_0}
  ipgui::add_param $IPINST -name "HW_PHASE_MS" -parent ${Page_0}
  ipgui::add_param $IPINST -name "I2C_HZ" -parent ${Page_0}
  ipgui::add_param $IPINST -name "MAX_RETRIES" -parent ${Page_0}
  ipgui::add_param $IPINST -name "SETTLE_MS" -parent ${Page_0}
  ipgui::add_param $IPINST -name "SW_RESET_MS" -parent ${Page_0}
  ipgui::add_param $IPINST -name "TIMEOUT_US" -parent ${Page_0}


}

proc update_PARAM_VALUE.CLK_HZ { PARAM_VALUE.CLK_HZ } {
	# Procedure called to update CLK_HZ when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.CLK_HZ { PARAM_VALUE.CLK_HZ } {
	# Procedure called to validate CLK_HZ
	return true
}

proc update_PARAM_VALUE.HW_PHASE_MS { PARAM_VALUE.HW_PHASE_MS } {
	# Procedure called to update HW_PHASE_MS when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.HW_PHASE_MS { PARAM_VALUE.HW_PHASE_MS } {
	# Procedure called to validate HW_PHASE_MS
	return true
}

proc update_PARAM_VALUE.I2C_HZ { PARAM_VALUE.I2C_HZ } {
	# Procedure called to update I2C_HZ when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.I2C_HZ { PARAM_VALUE.I2C_HZ } {
	# Procedure called to validate I2C_HZ
	return true
}

proc update_PARAM_VALUE.MAX_RETRIES { PARAM_VALUE.MAX_RETRIES } {
	# Procedure called to update MAX_RETRIES when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.MAX_RETRIES { PARAM_VALUE.MAX_RETRIES } {
	# Procedure called to validate MAX_RETRIES
	return true
}

proc update_PARAM_VALUE.SETTLE_MS { PARAM_VALUE.SETTLE_MS } {
	# Procedure called to update SETTLE_MS when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.SETTLE_MS { PARAM_VALUE.SETTLE_MS } {
	# Procedure called to validate SETTLE_MS
	return true
}

proc update_PARAM_VALUE.SW_RESET_MS { PARAM_VALUE.SW_RESET_MS } {
	# Procedure called to update SW_RESET_MS when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.SW_RESET_MS { PARAM_VALUE.SW_RESET_MS } {
	# Procedure called to validate SW_RESET_MS
	return true
}

proc update_PARAM_VALUE.TIMEOUT_US { PARAM_VALUE.TIMEOUT_US } {
	# Procedure called to update TIMEOUT_US when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.TIMEOUT_US { PARAM_VALUE.TIMEOUT_US } {
	# Procedure called to validate TIMEOUT_US
	return true
}


proc update_MODELPARAM_VALUE.CLK_HZ { MODELPARAM_VALUE.CLK_HZ PARAM_VALUE.CLK_HZ } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.CLK_HZ}] ${MODELPARAM_VALUE.CLK_HZ}
}

proc update_MODELPARAM_VALUE.I2C_HZ { MODELPARAM_VALUE.I2C_HZ PARAM_VALUE.I2C_HZ } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.I2C_HZ}] ${MODELPARAM_VALUE.I2C_HZ}
}

proc update_MODELPARAM_VALUE.HW_PHASE_MS { MODELPARAM_VALUE.HW_PHASE_MS PARAM_VALUE.HW_PHASE_MS } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.HW_PHASE_MS}] ${MODELPARAM_VALUE.HW_PHASE_MS}
}

proc update_MODELPARAM_VALUE.SW_RESET_MS { MODELPARAM_VALUE.SW_RESET_MS PARAM_VALUE.SW_RESET_MS } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.SW_RESET_MS}] ${MODELPARAM_VALUE.SW_RESET_MS}
}

proc update_MODELPARAM_VALUE.SETTLE_MS { MODELPARAM_VALUE.SETTLE_MS PARAM_VALUE.SETTLE_MS } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.SETTLE_MS}] ${MODELPARAM_VALUE.SETTLE_MS}
}

proc update_MODELPARAM_VALUE.TIMEOUT_US { MODELPARAM_VALUE.TIMEOUT_US PARAM_VALUE.TIMEOUT_US } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.TIMEOUT_US}] ${MODELPARAM_VALUE.TIMEOUT_US}
}

proc update_MODELPARAM_VALUE.MAX_RETRIES { MODELPARAM_VALUE.MAX_RETRIES PARAM_VALUE.MAX_RETRIES } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.MAX_RETRIES}] ${MODELPARAM_VALUE.MAX_RETRIES}
}

