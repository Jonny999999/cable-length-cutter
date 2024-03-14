idf_component_register(
    SRCS 
        "main.cpp"
        "global.cpp"
        "control.cpp"
        "buzzer.cpp"
        "vfd.cpp"
        "display.cpp"
        "cutter.cpp"
        "switchesAnalog.cpp"
		"stepper.cpp"
        "guide-stepper.cpp"
        "encoder.cpp"
        "shutdown.cpp"
    INCLUDE_DIRS 
        "."
    )

# colored build output (errors, warnings...)
idf_build_set_property(COMPILE_OPTIONS "-fdiagnostics-color=always" APPEND)