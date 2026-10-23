# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  ipgui::add_param $IPINST -name "GT_HOLD_CYCLES" -parent ${Page_0}
  ipgui::add_param $IPINST -name "USER_RESET_CYCLES" -parent ${Page_0}


}

proc update_PARAM_VALUE.GT_HOLD_CYCLES { PARAM_VALUE.GT_HOLD_CYCLES } {
	# Procedure called to update GT_HOLD_CYCLES when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.GT_HOLD_CYCLES { PARAM_VALUE.GT_HOLD_CYCLES } {
	# Procedure called to validate GT_HOLD_CYCLES
	return true
}

proc update_PARAM_VALUE.USER_RESET_CYCLES { PARAM_VALUE.USER_RESET_CYCLES } {
	# Procedure called to update USER_RESET_CYCLES when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.USER_RESET_CYCLES { PARAM_VALUE.USER_RESET_CYCLES } {
	# Procedure called to validate USER_RESET_CYCLES
	return true
}


proc update_MODELPARAM_VALUE.GT_HOLD_CYCLES { MODELPARAM_VALUE.GT_HOLD_CYCLES PARAM_VALUE.GT_HOLD_CYCLES } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.GT_HOLD_CYCLES}] ${MODELPARAM_VALUE.GT_HOLD_CYCLES}
}

proc update_MODELPARAM_VALUE.USER_RESET_CYCLES { MODELPARAM_VALUE.USER_RESET_CYCLES PARAM_VALUE.USER_RESET_CYCLES } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.USER_RESET_CYCLES}] ${MODELPARAM_VALUE.USER_RESET_CYCLES}
}

