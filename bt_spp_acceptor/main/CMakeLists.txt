set(COMPONENT_SRCS bt_spp_acceptor.c ili9340.c fontx.c)
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
spiffs_create_partition_image(storage ../font FLASH_IN_PROJECT
