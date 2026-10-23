# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  ipgui::add_param $IPINST -name "CHECK_KEEP" -parent ${Page_0}
  ipgui::add_param $IPINST -name "DATA_BYTES" -parent ${Page_0}
  ipgui::add_param $IPINST -name "EXPECTED_BEATS" -parent ${Page_0}
  ipgui::add_param $IPINST -name "EXPECTED_LINES" -parent ${Page_0}
  ipgui::add_param $IPINST -name "IDLE_TIMEOUT_CYCLES" -parent ${Page_0}
  ipgui::add_param $IPINST -name "USER_WIDTH" -parent ${Page_0}


}

proc update_PARAM_VALUE.CHECK_KEEP { PARAM_VALUE.CHECK_KEEP } {
	# Procedure called to update CHECK_KEEP when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.CHECK_KEEP { PARAM_VALUE.CHECK_KEEP } {
	# Procedure called to validate CHECK_KEEP
	return true
}

proc update_PARAM_VALUE.DATA_BYTES { PARAM_VALUE.DATA_BYTES } {
	# Procedure called to update DATA_BYTES when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.DATA_BYTES { PARAM_VALUE.DATA_BYTES } {
	# Procedure called to validate DATA_BYTES
	return true
}

proc update_PARAM_VALUE.EXPECTED_BEATS { PARAM_VALUE.EXPECTED_BEATS } {
	# Procedure called to update EXPECTED_BEATS when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.EXPECTED_BEATS { PARAM_VALUE.EXPECTED_BEATS } {
	# Procedure called to validate EXPECTED_BEATS
	return true
}

proc update_PARAM_VALUE.EXPECTED_LINES { PARAM_VALUE.EXPECTED_LINES } {
	# Procedure called to update EXPECTED_LINES when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.EXPECTED_LINES { PARAM_VALUE.EXPECTED_LINES } {
	# Procedure called to validate EXPECTED_LINES
	return true
}

proc update_PARAM_VALUE.IDLE_TIMEOUT_CYCLES { PARAM_VALUE.IDLE_TIMEOUT_CYCLES } {
	# Procedure called to update IDLE_TIMEOUT_CYCLES when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.IDLE_TIMEOUT_CYCLES { PARAM_VALUE.IDLE_TIMEOUT_CYCLES } {
	# Procedure called to validate IDLE_TIMEOUT_CYCLES
	return true
}

proc update_PARAM_VALUE.USER_WIDTH { PARAM_VALUE.USER_WIDTH } {
	# Procedure called to update USER_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.USER_WIDTH { PARAM_VALUE.USER_WIDTH } {
	# Procedure called to validate USER_WIDTH
	return true
}


proc update_MODELPARAM_VALUE.DATA_BYTES { MODELPARAM_VALUE.DATA_BYTES PARAM_VALUE.DATA_BYTES } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.DATA_BYTES}] ${MODELPARAM_VALUE.DATA_BYTES}
}

proc update_MODELPARAM_VALUE.USER_WIDTH { MODELPARAM_VALUE.USER_WIDTH PARAM_VALUE.USER_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.USER_WIDTH}] ${MODELPARAM_VALUE.USER_WIDTH}
}

proc update_MODELPARAM_VALUE.EXPECTED_BEATS { MODELPARAM_VALUE.EXPECTED_BEATS PARAM_VALUE.EXPECTED_BEATS } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.EXPECTED_BEATS}] ${MODELPARAM_VALUE.EXPECTED_BEATS}
}

proc update_MODELPARAM_VALUE.EXPECTED_LINES { MODELPARAM_VALUE.EXPECTED_LINES PARAM_VALUE.EXPECTED_LINES } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.EXPECTED_LINES}] ${MODELPARAM_VALUE.EXPECTED_LINES}
}

proc update_MODELPARAM_VALUE.CHECK_KEEP { MODELPARAM_VALUE.CHECK_KEEP PARAM_VALUE.CHECK_KEEP } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.CHECK_KEEP}] ${MODELPARAM_VALUE.CHECK_KEEP}
}

proc update_MODELPARAM_VALUE.IDLE_TIMEOUT_CYCLES { MODELPARAM_VALUE.IDLE_TIMEOUT_CYCLES PARAM_VALUE.IDLE_TIMEOUT_CYCLES } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.IDLE_TIMEOUT_CYCLES}] ${MODELPARAM_VALUE.IDLE_TIMEOUT_CYCLES}
}

