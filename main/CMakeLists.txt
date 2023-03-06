idf_component_register(SRCS 
    button.cpp
    captive.cpp
    config.cpp
    led.cpp
    main.cpp
    modes.cpp
    mqtt.cpp
    network.cpp
    output.cpp
    INCLUDE_DIRS ""
    EMBED_TXTFILES login.html success.html
)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++2a)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
