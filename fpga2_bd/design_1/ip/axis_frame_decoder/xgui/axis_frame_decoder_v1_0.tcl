# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  ipgui::add_param $IPINST -name "PREAMBLE_0" -parent ${Page_0}
  ipgui::add_param $IPINST -name "PREAMBLE_1" -parent ${Page_0}
  ipgui::add_param $IPINST -name "ROUTE_ID_0" -parent ${Page_0}
  ipgui::add_param $IPINST -name "ROUTE_ID_1" -parent ${Page_0}


}

proc update_PARAM_VALUE.PREAMBLE_0 { PARAM_VALUE.PREAMBLE_0 } {
	# Procedure called to update PREAMBLE_0 when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.PREAMBLE_0 { PARAM_VALUE.PREAMBLE_0 } {
	# Procedure called to validate PREAMBLE_0
	return true
}

proc update_PARAM_VALUE.PREAMBLE_1 { PARAM_VALUE.PREAMBLE_1 } {
	# Procedure called to update PREAMBLE_1 when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.PREAMBLE_1 { PARAM_VALUE.PREAMBLE_1 } {
	# Procedure called to validate PREAMBLE_1
	return true
}

proc update_PARAM_VALUE.ROUTE_ID_0 { PARAM_VALUE.ROUTE_ID_0 } {
	# Procedure called to update ROUTE_ID_0 when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.ROUTE_ID_0 { PARAM_VALUE.ROUTE_ID_0 } {
	# Procedure called to validate ROUTE_ID_0
	return true
}

proc update_PARAM_VALUE.ROUTE_ID_1 { PARAM_VALUE.ROUTE_ID_1 } {
	# Procedure called to update ROUTE_ID_1 when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.ROUTE_ID_1 { PARAM_VALUE.ROUTE_ID_1 } {
	# Procedure called to validate ROUTE_ID_1
	return true
}


proc update_MODELPARAM_VALUE.PREAMBLE_0 { MODELPARAM_VALUE.PREAMBLE_0 PARAM_VALUE.PREAMBLE_0 } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.PREAMBLE_0}] ${MODELPARAM_VALUE.PREAMBLE_0}
}

proc update_MODELPARAM_VALUE.PREAMBLE_1 { MODELPARAM_VALUE.PREAMBLE_1 PARAM_VALUE.PREAMBLE_1 } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.PREAMBLE_1}] ${MODELPARAM_VALUE.PREAMBLE_1}
}

proc update_MODELPARAM_VALUE.ROUTE_ID_0 { MODELPARAM_VALUE.ROUTE_ID_0 PARAM_VALUE.ROUTE_ID_0 } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.ROUTE_ID_0}] ${MODELPARAM_VALUE.ROUTE_ID_0}
}

proc update_MODELPARAM_VALUE.ROUTE_ID_1 { MODELPARAM_VALUE.ROUTE_ID_1 PARAM_VALUE.ROUTE_ID_1 } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.ROUTE_ID_1}] ${MODELPARAM_VALUE.ROUTE_ID_1}
}

